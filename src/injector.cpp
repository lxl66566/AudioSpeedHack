
#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <iostream>
#include <locale> // For std::locale
#include <map>
#include <string>
#include <tlhelp32.h>
#include <unordered_map>
#include <vector>

#include "ftxui/dom/elements.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"

// --- 数据结构 ---
struct ProcessInfo {
  DWORD pid;
  DWORD parent_pid;
  std::wstring name;
};

// --- 共享内存部分 ---
const wchar_t *MAPPING_NAME = L"GlobalAudioSpeedControl";
HANDLE hMapFile = NULL;
float *pSharedSpeed = NULL;

bool CreateSharedMemory() {
  hMapFile = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                sizeof(float), MAPPING_NAME);
  if (hMapFile == NULL) {
    std::wcerr << L"Could not create file mapping object: " << GetLastError()
               << std::endl;
    return false;
  }
  pSharedSpeed = (float *)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0,
                                        sizeof(float));
  if (pSharedSpeed == NULL) {
    std::wcerr << L"Could not map view of file: " << GetLastError()
               << std::endl;
    CloseHandle(hMapFile);
    hMapFile = NULL;
    return false;
  }
  return true;
}
void SetSpeed(float speed) {
  if (pSharedSpeed) {
    *pSharedSpeed = speed;
    std::wcout << L"Speed set to: " << speed << std::endl;
  }
}
void CleanupSharedMemory() {
  if (pSharedSpeed) {
    UnmapViewOfFile(pSharedSpeed);
    pSharedSpeed = NULL;
  }
  if (hMapFile) {
    CloseHandle(hMapFile);
    hMapFile = NULL;
  }
}

// --- 注入逻辑 (注入单个进程) ---
bool InjectDll(DWORD processId, const std::wstring &dllPath) {
  HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                    PROCESS_VM_WRITE | PROCESS_VM_READ,
                                FALSE, processId);
  if (!hProcess) {
    std::wcerr << L"Failed to open target process " << processId << L": "
               << GetLastError() << std::endl;
    return false;
  }
  const size_t dllPathSize = (dllPath.length() + 1) * sizeof(wchar_t);
  void *loc = VirtualAllocEx(hProcess, 0, dllPathSize, MEM_COMMIT | MEM_RESERVE,
                             PAGE_READWRITE);
  if (!loc) {
    std::wcerr << L"Failed to allocate memory in target process " << processId
               << L": " << GetLastError() << std::endl;
    CloseHandle(hProcess);
    return false;
  }
  if (!WriteProcessMemory(hProcess, loc, dllPath.c_str(), dllPathSize, 0)) {
    std::wcerr << L"Failed to write to process memory " << processId << L": "
               << GetLastError() << std::endl;
    VirtualFreeEx(hProcess, loc, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return false;
  }
  LPTHREAD_START_ROUTINE pLoadLibraryW =
      reinterpret_cast<LPTHREAD_START_ROUTINE>(
          GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
  if (!pLoadLibraryW) {
    std::wcerr << L"Failed to get address of LoadLibraryW: " << GetLastError()
               << std::endl;
    VirtualFreeEx(hProcess, loc, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return false;
  }
  HANDLE hThread = CreateRemoteThread(hProcess, 0, 0, pLoadLibraryW, loc, 0, 0);
  if (!hThread) {
    std::wcerr << L"Failed to create remote thread in " << processId << L": "
               << GetLastError() << std::endl;
    VirtualFreeEx(hProcess, loc, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return false;
  }
  WaitForSingleObject(hThread, INFINITE);
  CloseHandle(hThread);
  VirtualFreeEx(hProcess, loc, 0, MEM_RELEASE);
  CloseHandle(hProcess);
  return true;
}

// --- 核心改进：进程树处理 ---

// 获取所有进程，并识别出“根”进程
std::vector<ProcessInfo>
GetRootProcesses(std::unordered_map<DWORD, ProcessInfo> &all_processes) {
  all_processes.clear();
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return {};
  }

  PROCESSENTRY32W entry;
  entry.dwSize = sizeof(PROCESSENTRY32W);

  if (Process32FirstW(snapshot, &entry)) {
    do {
      all_processes[entry.th32ProcessID] = {
          entry.th32ProcessID, entry.th32ParentProcessID, entry.szExeFile};
    } while (Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);

  std::vector<ProcessInfo> root_processes;
  for (const auto &pair : all_processes) {
    const ProcessInfo &current_proc = pair.second;
    auto parent_it = all_processes.find(current_proc.parent_pid);

    // 如果父进程不存在，或者父进程的名字和当前进程不一样，则认为是根进程
    if (parent_it == all_processes.end() ||
        parent_it->second.name != current_proc.name) {
      root_processes.push_back(current_proc);
    }
  }

  return root_processes;
}

// 注入根进程及其所有子进程
void InjectIntoProcessAndChildren(
    DWORD root_pid, const std::wstring &dllPath,
    const std::unordered_map<DWORD, ProcessInfo> &all_processes) {
  // 1. 构建父子关系图，方便快速查找
  std::unordered_map<DWORD, std::vector<DWORD>> children_map;
  for (const auto &pair : all_processes) {
    children_map[pair.second.parent_pid].push_back(pair.second.pid);
  }

  // 2. 使用一个队列来进行广度优先遍历和注入
  std::vector<DWORD> to_inject;
  to_inject.push_back(root_pid);

  size_t head = 0;
  while (head < to_inject.size()) {
    DWORD current_pid = to_inject[head++];

    std::wcout << L"Injecting into PID: " << current_pid << L" ("
               << all_processes.at(current_pid).name << L")... ";
    if (InjectDll(current_pid, dllPath)) {
      std::wcout << L"Success!" << std::endl;
      // 如果注入成功，将其子进程加入待注入队列
      if (children_map.count(current_pid)) {
        for (DWORD child_pid : children_map.at(current_pid)) {
          to_inject.push_back(child_pid);
        }
      }
    } else {
      std::wcout << L"Failed!" << std::endl;
    }
  }
  std::wcout << L"\nInjection process for the tree completed." << std::endl;
}

// --- TUI 和主函数 ---

// 获取 DLL 路径
std::wstring GetDllPath() {
  wchar_t buffer[MAX_PATH];
  GetModuleFileNameW(NULL, buffer, MAX_PATH);
  std::wstring exePath = buffer;
  size_t lastSlash = exePath.find_last_of(L"\\/");
  if (std::wstring::npos != lastSlash) {
    return exePath.substr(0, lastSlash + 1) + L"audiospeedhack.dll";
  }
  return L"";
}

// --- 过滤逻辑函数 ---
// Helper to convert wstring to lowercase
std::wstring to_lower(std::wstring s) {
  std::transform(s.begin(), s.end(), s.begin(), ::towlower);
  return s;
}

// Helper function to check if a wstring contains only ASCII characters
bool is_ascii(const std::wstring &s) {
  for (wchar_t c : s) {
    if (c > 127) { // ASCII characters are 0-127
      return false;
    }
  }
  return true;
}

void ApplyFilter(
    const std::map<std::wstring, std::vector<ProcessInfo>> &all_groups,
    const std::wstring &filter,
    // Output parameters:
    std::vector<std::wstring> &filtered_menu_entries,
    std::vector<std::wstring> &filtered_process_names, int &selected_entry) {
  filtered_menu_entries.clear();
  filtered_process_names.clear();
  std::wstring lower_filter = to_lower(filter);

  // Temporary storage to hold pairs of (process_name, menu_entry) before final
  // sort
  std::vector<std::pair<std::wstring, std::wstring>> temp_filtered_results;

  for (const auto &pair : all_groups) {
    const std::wstring &name = pair.first;
    const std::vector<ProcessInfo> &instances = pair.second;

    bool name_matches = to_lower(name).find(lower_filter) != std::wstring::npos;
    bool pid_matches = false;

    // Also check if any PID in the group matches the filter
    if (!name_matches) {
      for (const auto &proc_info : instances) {
        if (std::to_wstring(proc_info.pid).find(filter) != std::wstring::npos) {
          pid_matches = true;
          break;
        }
      }
    }

    if (name_matches || pid_matches) {
      temp_filtered_results.push_back(
          {name,
           name + L" (" + std::to_wstring(instances.size()) + L" instances)"});
    }
  }

  // Apply the custom sorting logic to temp_filtered_results
  std::sort(temp_filtered_results.begin(), temp_filtered_results.end(),
            [](const std::pair<std::wstring, std::wstring> &a,
               const std::pair<std::wstring, std::wstring> &b) {
              bool a_is_ascii = is_ascii(a.first);
              bool b_is_ascii = is_ascii(b.first);

              if (!a_is_ascii && b_is_ascii) {
                return true; // a (non-ASCII) comes before b (ASCII)
              }
              if (a_is_ascii && !b_is_ascii) {
                return false; // b (non-ASCII) comes before a (ASCII)
              }
              // If both are ASCII or both are non-ASCII, sort alphabetically
              return a.first < b.first;
            });

  // Populate the output vectors from the sorted temporary results
  for (const auto &item : temp_filtered_results) {
    filtered_process_names.push_back(item.first);
    filtered_menu_entries.push_back(item.second);
  }

  // Clamp selected entry to be within bounds
  if (selected_entry >= (int)filtered_menu_entries.size()) {
    selected_entry = (int)filtered_menu_entries.size() - 1;
  }
  if (selected_entry < 0) {
    selected_entry = 0;
  }
}
int main(int argc, char *argv[]) {
  std::wcin.imbue(std::locale(""));
  std::wcout.imbue(std::locale(""));

  // 1. 获取 DLL 路径
  std::wstring dllPath = GetDllPath();
  if (dllPath.empty() ||
      GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    std::wcerr << L"Error: audiospeedhack.dll not found!" << std::endl;
    system("pause");
    return 1;
  }
  std::wcout << L"Using DLL: " << dllPath << std::endl;

  // 2. 获取并分组进程
  std::unordered_map<DWORD, ProcessInfo> all_processes;
  std::vector<ProcessInfo> root_processes = GetRootProcesses(all_processes);
  std::map<std::wstring, std::vector<ProcessInfo>> grouped_processes;
  for (const auto &proc : root_processes) {
    grouped_processes[proc.name].push_back(proc);
  }

  // 3. *** TUI 状态管理 ***
  std::wstring filter_string;
  std::vector<std::wstring> filtered_menu_entries;
  std::vector<std::wstring> filtered_process_names;
  int selected_entry = 0;
  std::wstring selected_process_name;

  // 初始化显示列表
  ApplyFilter(grouped_processes, filter_string, filtered_menu_entries,
              filtered_process_names, selected_entry);

  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  // 4. *** 创建带事件拦截的 TUI 组件 ***
  ftxui::MenuOption option;
  option.on_enter = [&] {
    if (selected_entry >= 0 && selected_entry < filtered_process_names.size()) {
      selected_process_name = filtered_process_names[selected_entry];
      screen.Exit();
    }
  };

  // Menu现在使用动态的 filtered_menu_entries
  auto menu = ftxui::Menu(&filtered_menu_entries, &selected_entry, option);

  // 使用 CatchEvent 包装 Menu 来处理自定义输入
  auto component = ftxui::CatchEvent(menu, [&](ftxui::Event event) {
    if (event.is_character()) {
      wchar_t ch = event.character()[0];
      if (std::iswalnum(ch) || ch == L'-' || ch == L'_' || ch == L'.') {
        filter_string += ch;
        ApplyFilter(grouped_processes, filter_string, filtered_menu_entries,
                    filtered_process_names, selected_entry);
        return true; // Event handled
      }
    }
    if (event == ftxui::Event::Backspace) {
      if (!filter_string.empty()) {
        filter_string.pop_back();
        ApplyFilter(grouped_processes, filter_string, filtered_menu_entries,
                    filtered_process_names, selected_entry);
      }
      return true; // Event handled
    }
    if (event == ftxui::Event::Escape) {
      if (!filter_string.empty()) {
        filter_string.clear();
        ApplyFilter(grouped_processes, filter_string, filtered_menu_entries,
                    filtered_process_names, selected_entry);
      } else {
        // If filter is already empty, Esc exits the program
        screen.Exit();
      }
      return true; // Event handled
    }
    return false; // Event not handled, pass to Menu
  });

  // 5. *** 创建渲染器以显示所有 TUI 元素 ***
  auto renderer = ftxui::Renderer(component, [&] {
    return ftxui::vbox(
               {ftxui::text(L"Select a process group (Use ↑/↓ and Enter):") |
                    ftxui::bold,
                ftxui::hbox(
                    {ftxui::text(L"Filter: "),
                     ftxui::text(filter_string) |
                         ftxui::inverted, // Inverted style for the input text
                     ftxui::text(L"  (Type to search, Backspace to delete, Esc "
                                 L"to clear/exit)")}),
                ftxui::separator(),
                component->Render() | ftxui::vscroll_indicator | ftxui::frame |
                    ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 20)}) |
           ftxui::border;
  });

  screen.Loop(renderer);

  // 6. 后续注入逻辑
  if (selected_process_name.empty()) {
    std::wcout << L"No process selected. Exiting." << std::endl;
    return 0;
  }

  if (!CreateSharedMemory()) {
    system("pause");
    return 1;
  }

  std::wcout << L"\nInjecting into all processes named '"
             << selected_process_name << L"' and their children..."
             << std::endl;
  const auto &roots_to_inject = grouped_processes[selected_process_name];
  for (const auto &root_proc : roots_to_inject) {
    std::wcout << L"--- Starting injection for root PID: " << root_proc.pid
               << L" ---" << std::endl;
    InjectIntoProcessAndChildren(root_proc.pid, dllPath, all_processes);
  }

  // 7. 循环设置速度
  std::wcout << L"\nEnter new speed (e.g., 1.5). Type 'exit' to quit."
             << std::endl;
  float currentSpeed = 1.0f;
  SetSpeed(currentSpeed);
  std::string input;
  while (true) {
    std::wcout << L"> ";
    std::cin >> input;
    if (input == "exit")
      break;
    try {
      currentSpeed = std::stof(input);
      SetSpeed(currentSpeed);
    } catch (const std::exception &) {
      std::wcout << L"Invalid input. Please enter a number." << std::endl;
    }
  }

  CleanupSharedMemory();
  return 0;
}

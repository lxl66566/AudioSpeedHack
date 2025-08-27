
#include <windows.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <tlhelp32.h>
#include <unordered_map>
#include <vector>


#include "ftxui/dom/elements.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"

// --- 数据结构 ---
struct ProcessInfo {
  DWORD pid;
  DWORD parent_pid;
  std::wstring name;
};

// --- 共享内存部分 (无变化) ---
const wchar_t *MAPPING_NAME = L"GlobalAudioSpeedControl";
HANDLE hMapFile = NULL;
float *pSharedSpeed = NULL;

bool CreateSharedMemory() { /* ... 与之前版本完全相同 ... */
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
void SetSpeed(float speed) { /* ... 与之前版本完全相同 ... */
  if (pSharedSpeed) {
    *pSharedSpeed = speed;
    std::wcout << L"Speed set to: " << speed << std::endl;
  }
}
void CleanupSharedMemory() { /* ... 与之前版本完全相同 ... */
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
bool InjectDll(DWORD processId,
               const std::wstring &dllPath) { /* ... 与之前版本完全相同 ... */
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

  // 按名称排序
  std::sort(root_processes.begin(), root_processes.end(),
            [](const ProcessInfo &a, const ProcessInfo &b) {
              return a.name < b.name;
            });

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

int main(int argc, char *argv[]) {
  std::wcin.imbue(std::locale(""));
  std::wcout.imbue(std::locale(""));

  // 1. 获取并检查 DLL 路径
  std::wstring dllPath = GetDllPath();
  if (dllPath.empty() ||
      GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    std::wcerr << L"Error: audiospeedhack.dll not found in the same directory "
                  L"as the injector!"
               << std::endl;
    system("pause");
    return 1;
  }
  std::wcout << L"Using DLL: " << dllPath << std::endl;

  // 2. 获取所有进程和根进程
  std::unordered_map<DWORD, ProcessInfo> all_processes;
  std::vector<ProcessInfo> root_processes = GetRootProcesses(all_processes);
  if (root_processes.empty()) {
    std::wcerr << L"Could not find any running processes." << std::endl;
    system("pause");
    return 1;
  }

  // 3. *** 新逻辑：按进程名称对根进程进行分组 ***
  std::map<std::wstring, std::vector<ProcessInfo>> grouped_processes;
  for (const auto &proc : root_processes) {
    grouped_processes[proc.name].push_back(proc);
  }

  // 4. *** TUI 选择进程 (基于分组后的名称) ***
  std::wstring selected_process_name;
  int selected_entry = 0;
  std::vector<std::wstring> menu_entries;
  std::vector<std::wstring> process_names; // 用于通过索引查找名称

  for (const auto &pair : grouped_processes) {
    // TUI 菜单项显示为 "进程名 (实例数)"
    menu_entries.push_back(pair.first + L" (" +
                           std::to_wstring(pair.second.size()) +
                           L" instances)");
    process_names.push_back(pair.first);
  }

  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  ftxui::MenuOption option;
  option.on_enter = [&] {
    if (!process_names.empty()) {
      selected_process_name = process_names[selected_entry];
      screen.Exit();
    }
  };

  auto menu = ftxui::Menu(&menu_entries, &selected_entry, option);
  auto renderer = ftxui::Renderer(menu, [&] {
    return ftxui::vbox(
               {ftxui::text(
                    L"Select a process group to inject (Use ↑/↓ and Enter):") |
                    ftxui::bold,
                ftxui::separator(),
                menu->Render() | ftxui::vscroll_indicator | ftxui::frame |
                    ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 20)}) |
           ftxui::border;
  });

  screen.Loop(renderer);

  if (selected_process_name.empty()) {
    std::wcout << L"No process selected. Exiting." << std::endl;
    return 0;
  }

  // 5. 创建共享内存
  if (!CreateSharedMemory()) {
    system("pause");
    return 1;
  }

  // 6. *** 新逻辑：注入所有同名的根进程及其子进程 ***
  std::wcout << L"\nInjecting into all processes named '"
             << selected_process_name << L"' and their children..."
             << std::endl;
  std::wcout << L"--------------------------------------------------"
             << std::endl;
  const auto &roots_to_inject = grouped_processes[selected_process_name];
  for (const auto &root_proc : roots_to_inject) {
    std::wcout << L"--- Starting injection for root PID: " << root_proc.pid
               << L" ---" << std::endl;
    InjectIntoProcessAndChildren(root_proc.pid, dllPath, all_processes);
    std::wcout << L"--- Finished injection for root PID: " << root_proc.pid
               << L" ---\n"
               << std::endl;
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
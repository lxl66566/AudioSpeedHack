#include <iostream>
#include <locale>
#include <map>
#include <string>
#include <vector>

#include "injection.h"
#include "process_utils.h"
#include "shared_memory.h"
#include "tui_selection.h"

int main(int argc, char *argv[]) {
  // 设置本地化，以正确显示宽字符
  std::wcin.imbue(std::locale(""));
  std::wcout.imbue(std::locale(""));

  // 1. 检查并获取 DLL 路径
  std::wstring dllPath = GetDllPath(L"audiospeedhack.dll");
  if (dllPath.empty() ||
      GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    std::wcerr
        << L"Error: audiospeedhack.dll not found in the application directory!"
        << std::endl;
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

  // 3. 使用 TUI 模块让用户选择进程
  std::wstring selected_process_name = SelectProcessTUI(grouped_processes);

  // 4. 根据用户选择执行注入逻辑
  if (selected_process_name.empty()) {
    std::wcout << L"No process selected. Exiting." << std::endl;
    return 0;
  }

  std::unique_ptr<SharedMemoryValue<float>> speed_control;
  try {
    // 使用智能指针管理，传入名字和创建模式
    speed_control = std::make_unique<SharedMemoryValue<float>>(
        L"GlobalAudioSpeedControl", SharedMemoryValue<float>::Mode::Create);
  } catch (const std::runtime_error &e) {
    std::wcerr << L"Error initializing shared memory: " << e.what()
               << std::endl;
    system("pause");
    return 1;
  }

  std::wcout << L"\nInjecting into all processes named '"
             << selected_process_name << L"' and their children..."
             << std::endl;
  const auto &roots_to_inject = grouped_processes.at(selected_process_name);
  std::vector<DWORD> injectedPIDs; // 存储所有被注入的PID
  for (const auto &root_proc : roots_to_inject) {
    std::wcout << L"--- Starting injection for root PID: " << root_proc.pid
               << L" ---" << std::endl;
    InjectIntoProcessAndChildren(root_proc.pid, dllPath, all_processes,
                                 injectedPIDs);
  }
  std::wcout << L"\nInjection process for the tree completed." << std::endl;

  // 5. 循环设置速度
  std::wcout << L"\nEnter new speed (e.g., 1.5). Type 'exit' to quit."
             << std::endl;
  float currentSpeed = 1.0f;
  speed_control->SetValue(currentSpeed); // 设置初始速度

  std::string input;
  while (true) {
    std::wcout << L"> ";
    std::cin >> input;
    if (input == "exit") {
      break;
    }
    try {
      currentSpeed = std::stof(input);
      if (speed_control->SetValue(currentSpeed)) { // 调用新方法
        std::wcout << L"Speed set to: " << currentSpeed << std::endl;
      }
    } catch (const std::exception &) {
      std::wcout << L"Invalid input. Please enter a number." << std::endl;
    }
  }
  return 0;
}
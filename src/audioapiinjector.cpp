
#include <windows.h>

#include <iostream>
#include <locale>
#include <map>
#include <signal.h>
#include <string>
#include <vector>

#include "injection.h"
#include "process_utils.h"
#include "tui_selection.h"

// --- 全局变量用于Debug Monitor ---
HANDLE hBufferReadyEvent, hDataReadyEvent;
HANDLE hBufferMapping;
void *pBufferView = nullptr;
const char *DBWIN_BUFFER_READY_EVENT = "DBWIN_BUFFER_READY";
const char *DBWIN_DATA_READY_EVENT = "DBWIN_DATA_READY";
const char *DBWIN_BUFFER_MAPPING = "DB_WIN_BUFFER";
const DWORD DBWIN_BUFFER_SIZE = 4096;

// 用于优雅退出的标志
volatile bool g_bContinueListening = true;

// --- Debug Monitor 初始化和清理 ---

bool InitializeDebugMonitor() {
  // 1. 创建共享内存映射
  hBufferMapping =
      CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                         DBWIN_BUFFER_SIZE, DBWIN_BUFFER_MAPPING);
  if (hBufferMapping == NULL) {
    std::cerr << "Error: Could not create file mapping object: "
              << GetLastError() << std::endl;
    return false;
  }

  // 2. 映射共享内存视图
  pBufferView = MapViewOfFile(hBufferMapping, FILE_MAP_READ, 0, 0, 0);
  if (pBufferView == nullptr) {
    std::cerr << "Error: Could not map view of file: " << GetLastError()
              << std::endl;
    CloseHandle(hBufferMapping);
    return false;
  }

  // 3. 创建事件
  hBufferReadyEvent = CreateEventA(NULL, FALSE, TRUE, DBWIN_BUFFER_READY_EVENT);
  if (hBufferReadyEvent == NULL) {
    std::cerr << "Error: Could not create BufferReady event: " << GetLastError()
              << std::endl;
    UnmapViewOfFile(pBufferView);
    CloseHandle(hBufferMapping);
    return false;
  }

  hDataReadyEvent = CreateEventA(NULL, FALSE, FALSE, DBWIN_DATA_READY_EVENT);
  if (hDataReadyEvent == NULL) {
    std::cerr << "Error: Could not create DataReady event: " << GetLastError()
              << std::endl;
    CloseHandle(hBufferReadyEvent);
    UnmapViewOfFile(pBufferView);
    CloseHandle(hBufferMapping);
    return false;
  }

  return true;
}

void CleanupDebugMonitor() {
  if (hDataReadyEvent)
    CloseHandle(hDataReadyEvent);
  if (hBufferReadyEvent)
    CloseHandle(hBufferReadyEvent);
  if (pBufferView)
    UnmapViewOfFile(pBufferView);
  if (hBufferMapping)
    CloseHandle(hBufferMapping);
}

// --- 控制台事件处理，用于捕获 Ctrl+C ---
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
  if (ctrlType == CTRL_C_EVENT) {
    std::cout << "\nCtrl+C detected. Shutting down listener..." << std::endl;
    g_bContinueListening = false;
    // 触发一次事件，让等待的循环退出
    SetEvent(hDataReadyEvent);
    return TRUE; // 表示我们已经处理了该事件
  }
  return FALSE; // 交给下一个处理程序
}

// --- 主循环，监听调试输出 ---
void DebugMonitorLoop(const std::vector<DWORD> &targetPIDs) {
  while (g_bContinueListening) {
    // 通知其他进程，调试缓冲区已准备好接收数据
    SetEvent(hBufferReadyEvent);

    // 等待有进程写入数据，设置超时以检查退出标志
    DWORD waitResult = WaitForSingleObject(hDataReadyEvent, 500);

    if (!g_bContinueListening) {
      break;
    }

    if (waitResult == WAIT_OBJECT_0 && pBufferView) {
      // 读取PID
      DWORD messagePID = *(static_cast<DWORD *>(pBufferView));

      // 检查PID是否在我们关注的列表中
      bool isTarget = false;
      for (DWORD pid : targetPIDs) {
        if (messagePID == pid) {
          isTarget = true;
          break;
        }
      }

      if (isTarget) {
        // 读取消息内容 (PID之后就是字符串)
        char *message = static_cast<char *>(pBufferView) + sizeof(DWORD);
        // 直接输出到 stdout
        std::cout << "[PID: " << messagePID << "] " << message << std::endl;
      }
    }
  }
}

int main() {
  std::wcin.imbue(std::locale(""));
  std::wcout.imbue(std::locale(""));

  const std::wstring dllToInject = L"audioapiinfo.dll";

  // 1. 检查 DLL 是否存在
  std::wstring dllPath = GetDllPath(dllToInject);
  if (dllPath.empty() ||
      GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    std::wcerr << L"Error: " << dllToInject
               << L" not found in the application directory!" << std::endl;
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

  if (selected_process_name.empty()) {
    std::wcout << L"No process selected. Exiting." << std::endl;
    return 0;
  }

  // 4. 初始化 Debug Monitor
  if (!InitializeDebugMonitor()) {
    std::cerr << "Failed to initialize debug monitor. Exiting." << std::endl;
    system("pause");
    return 1;
  }

  // 注册 Ctrl+C 处理程序
  SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

  // 5. 注入所有选定的进程实例
  std::wcout << L"\nInjecting into all processes named '"
             << selected_process_name << L"'..." << std::endl;

  const auto &roots_to_inject = grouped_processes.at(selected_process_name);
  std::vector<DWORD> injectedPIDs; // 存储所有被注入的PID

  for (const auto &root_proc : roots_to_inject) {
    std::wcout << L"--- Injecting into root PID: " << root_proc.pid << L" ("
               << root_proc.name << L") and its children ---" << std::endl;
    // 调用 InjectIntoProcessAndChildren 来递归注入，并收集所有被注入的PID
    InjectIntoProcessAndChildren(root_proc.pid, dllPath, all_processes,
                                 injectedPIDs);
  }

  if (injectedPIDs.empty()) {
    std::cerr << "No processes were successfully injected. Exiting."
              << std::endl;
    CleanupDebugMonitor();
    system("pause");
    return 1;
  }

  // 6. 开始监听
  std::cout << "\nInjection complete. Listening for debug messages..."
            << std::endl;
  std::cout << "Press Ctrl+C to stop listening and exit." << std::endl;
  DebugMonitorLoop(injectedPIDs);

  // 7. 清理
  CleanupDebugMonitor();
  SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE); // 注销处理程序
  std::cout << "Listener stopped. Exiting." << std::endl;

  return 0;
}

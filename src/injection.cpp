#include "injection.h"
#include <iostream>
#include <vector>

// 接受一个dllName参数
std::wstring GetDllPath(const std::wstring &dllName) {
  wchar_t buffer[MAX_PATH];
  GetModuleFileNameW(NULL, buffer, MAX_PATH);
  std::wstring exePath = buffer;
  size_t lastSlash = exePath.find_last_of(L"\\/");
  if (std::wstring::npos != lastSlash) {
    return exePath.substr(0, lastSlash + 1) + dllName; // 使用传入的dllName
  }
  return L"";
}

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

void InjectIntoProcessAndChildren(
    DWORD root_pid, const std::wstring &dllPath,
    const std::unordered_map<DWORD, ProcessInfo> &all_processes,
    std::vector<DWORD> &injectedPIDs) {
  std::unordered_map<DWORD, std::vector<DWORD>> children_map;
  for (const auto &pair : all_processes) {
    children_map[pair.second.parent_pid].push_back(pair.second.pid);
  }

  std::vector<DWORD> to_inject;
  to_inject.push_back(root_pid);

  size_t head = 0;
  while (head < to_inject.size()) {
    DWORD current_pid = to_inject[head++];

    std::wcout << L"Injecting into PID: " << current_pid << L" ("
               << all_processes.at(current_pid).name << L")... ";
    if (InjectDll(current_pid, dllPath)) {
      std::wcout << L"Success!" << std::endl;
      injectedPIDs.push_back(current_pid); // 将成功注入的PID添加到列表中
      if (children_map.count(current_pid)) {
        for (DWORD child_pid : children_map.at(current_pid)) {
          to_inject.push_back(child_pid);
        }
      }
    } else {
      std::wcout << L"Failed!" << std::endl;
    }
  }
}

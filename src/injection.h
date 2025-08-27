#ifndef INJECTION_H
#define INJECTION_H

#include <windows.h>

#include "process_utils.h" // 需要 ProcessInfo 结构体
#include <string>
#include <unordered_map>

// 获取指定DLL的完整路径（假设与exe在同一目录）
std::wstring GetDllPath(const std::wstring &dllName);

// 将 DLL 注入到单个进程中
bool InjectDll(DWORD processId, const std::wstring &dllPath);

// 注入一个根进程及其所有同名子进程
void InjectIntoProcessAndChildren(
    DWORD root_pid, const std::wstring &dllPath,
    const std::unordered_map<DWORD, ProcessInfo> &all_processes);

#endif // INJECTION_H
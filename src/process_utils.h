#ifndef PROCESS_UTILS_H
#define PROCESS_UTILS_H

#include <windows.h>

#include <string>
#include <unordered_map>
#include <vector>

// 进程信息结构体
struct ProcessInfo {
  DWORD pid;
  DWORD parent_pid;
  std::wstring name;
};

// 获取所有进程，并识别出“根”进程（父进程不存在或父进程名不同）
// all_processes: [输出参数] 用于填充所有进程信息的 map
// 返回值: 一个包含所有被识别为根进程的列表
std::vector<ProcessInfo>
GetRootProcesses(std::unordered_map<DWORD, ProcessInfo> &all_processes);

#endif // PROCESS_UTILS_H
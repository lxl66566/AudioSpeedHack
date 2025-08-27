#include "process_utils.h"
#include <tlhelp32.h>

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
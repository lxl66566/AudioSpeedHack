#ifndef TUI_SELECTION_H
#define TUI_SELECTION_H

#include "process_utils.h" // 需要 ProcessInfo
#include <map>
#include <string>
#include <vector>

// 启动 TUI 界面，让用户从分组的进程中进行选择
// grouped_processes: [输入参数] 按进程名分组的根进程列表
// 返回值: 用户选择的进程组名。如果用户未选择，则返回空字符串。
std::wstring SelectProcessTUI(
    const std::map<std::wstring, std::vector<ProcessInfo>> &grouped_processes);

#endif // TUI_SELECTION_H
#include "tui_selection.h"

#include <algorithm>
#include <cwctype>

#include "ftxui/dom/elements.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"

// --- 过滤逻辑的辅助函数 ---
namespace { // 使用匿名命名空间避免链接冲突

// 将 wstring 转换为小写
std::wstring to_lower(std::wstring s) {
  std::transform(s.begin(), s.end(), s.begin(), ::towlower);
  return s;
}

// 检查 wstring 是否只包含 ASCII 字符
bool is_ascii(const std::wstring &s) {
  for (wchar_t c : s) {
    if (c > 127) {
      return false;
    }
  }
  return true;
}

// 应用过滤器并更新菜单列表
void ApplyFilter(
    const std::map<std::wstring, std::vector<ProcessInfo>> &all_groups,
    const std::wstring &filter,
    // 输出参数:
    std::vector<std::wstring> &filtered_menu_entries,
    std::vector<std::wstring> &filtered_process_names, int &selected_entry) {
  filtered_menu_entries.clear();
  filtered_process_names.clear();
  std::wstring lower_filter = to_lower(filter);

  std::vector<std::pair<std::wstring, std::wstring>> temp_filtered_results;

  for (const auto &pair : all_groups) {
    const std::wstring &name = pair.first;
    const std::vector<ProcessInfo> &instances = pair.second;
    bool name_matches = to_lower(name).find(lower_filter) != std::wstring::npos;
    bool pid_matches = false;

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

  std::sort(temp_filtered_results.begin(), temp_filtered_results.end(),
            [](const std::pair<std::wstring, std::wstring> &a,
               const std::pair<std::wstring, std::wstring> &b) {
              bool a_is_ascii = is_ascii(a.first);
              bool b_is_ascii = is_ascii(b.first);
              if (!a_is_ascii && b_is_ascii)
                return true;
              if (a_is_ascii && !b_is_ascii)
                return false;
              return a.first < b.first;
            });

  for (const auto &item : temp_filtered_results) {
    filtered_process_names.push_back(item.first);
    filtered_menu_entries.push_back(item.second);
  }

  if (selected_entry >= (int)filtered_menu_entries.size()) {
    selected_entry = (int)filtered_menu_entries.size() - 1;
  }
  if (selected_entry < 0) {
    selected_entry = 0;
  }
}

} // namespace

std::wstring SelectProcessTUI(
    const std::map<std::wstring, std::vector<ProcessInfo>> &grouped_processes) {
  std::wstring filter_string;
  std::vector<std::wstring> filtered_menu_entries;
  std::vector<std::wstring> filtered_process_names;
  int selected_entry = 0;
  std::wstring selected_process_name;

  ApplyFilter(grouped_processes, filter_string, filtered_menu_entries,
              filtered_process_names, selected_entry);

  auto screen = ftxui::ScreenInteractive::TerminalOutput();

  ftxui::MenuOption option;
  option.on_enter = [&] {
    if (selected_entry >= 0 && selected_entry < filtered_process_names.size()) {
      selected_process_name = filtered_process_names[selected_entry];
      screen.Exit();
    }
  };

  auto menu = ftxui::Menu(&filtered_menu_entries, &selected_entry, option);

  auto component = ftxui::CatchEvent(menu, [&](ftxui::Event event) {
    if (event.is_character()) {
      wchar_t ch = event.character()[0];
      if (std::iswalnum(ch) || ch == L'-' || ch == L'_' || ch == L'.') {
        filter_string += ch;
        ApplyFilter(grouped_processes, filter_string, filtered_menu_entries,
                    filtered_process_names, selected_entry);
        return true;
      }
    }
    if (event == ftxui::Event::Backspace && !filter_string.empty()) {
      filter_string.pop_back();
      ApplyFilter(grouped_processes, filter_string, filtered_menu_entries,
                  filtered_process_names, selected_entry);
      return true;
    }
    if (event == ftxui::Event::Escape) {
      if (!filter_string.empty()) {
        filter_string.clear();
        ApplyFilter(grouped_processes, filter_string, filtered_menu_entries,
                    filtered_process_names, selected_entry);
      } else {
        screen.Exit();
      }
      return true;
    }
    return false;
  });

  auto renderer = ftxui::Renderer(component, [&] {
    return ftxui::vbox(
               {ftxui::text(L"Select a process group (Use ↑/↓ and Enter):") |
                    ftxui::bold,
                ftxui::hbox({ftxui::text(L"Filter: "),
                             ftxui::text(filter_string) | ftxui::inverted,
                             ftxui::text(L"  (Type to search, Backspace to "
                                         L"delete, Esc to clear/exit)")}),
                ftxui::separator(),
                component->Render() | ftxui::vscroll_indicator | ftxui::frame |
                    ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 20)}) |
           ftxui::border;
  });

  screen.Loop(renderer);

  return selected_process_name;
}
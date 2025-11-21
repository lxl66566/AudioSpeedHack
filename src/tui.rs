use std::fs;

use anyhow::Result;
use terminal_menu::{
    TerminalMenuItem, back_button, button, label, list, menu, mut_menu, run, submenu,
};

use crate::{cache::GLOBAL_CACHE, cli::*};

const NONE_EXEC_ITEM: &str = "None";

/// TUI 主函数，引导用户选择命令和参数，并返回一个完整的 Cli 对象
pub fn run_tui() -> Result<Cli> {
    let executable_options = exec_options();
    let prev_cli = GLOBAL_CACHE.lock().unwrap().last_command.clone();

    // 2. 构建菜单
    let mut main_menu_items = vec![
        label(concat!(
            env!("CARGO_PKG_NAME"),
            ": ",
            env!("CARGO_PKG_REPOSITORY"),
            "  请选择一个要执行的操作，按 q 退出:"
        )),
        submenu("UnpackDll (解压DLL)", unpack_dll_menu(&executable_options)),
        button("Clean (清除 AudioSpeedHack 残留)"),
        button("Exit (退出)"),
    ];
    if prev_cli.is_some() {
        main_menu_items.insert(1, button("使用上次参数运行"));
    }
    let main_menu = menu(main_menu_items);

    run(&main_menu);

    if mut_menu(&main_menu).canceled() {
        anyhow::bail!("用户取消操作。");
    }

    // 3. 根据用户的选择，构建并返回 Cli 对象
    let mut tmp = mut_menu(&main_menu);
    let chosen_command_name = tmp.selected_item_name();
    let command = match chosen_command_name {
        "Clean (清除 AudioSpeedHack 残留)" => anyhow::Ok(Commands::Clean),
        "UnpackDll (解压DLL)" => {
            let sub_menu = tmp.get_submenu("UnpackDll (解压DLL)");
            if sub_menu.canceled() {
                anyhow::bail!("用户在 UnpackDll 菜单中取消了操作。");
            }
            let exec_selection = sub_menu.selection_value("游戏 exe，用于检测架构 (可选)");
            let exec = if exec_selection == NONE_EXEC_ITEM {
                None
            } else {
                Some(exec_selection.into())
            };
            Ok(Commands::UnpackDll(UnpackDllArgs {
                dll: match sub_menu.selection_value("选择解压的 DLL") {
                    "ALL" => None,
                    spe => Some(spe.to_lowercase().parse().expect("TUI internal error")),
                },
                x86: sub_menu.selection_value("选择架构") == "x86",
                speed: sub_menu.selection_value("选择速度").parse()?,
                exec,
            }))
        }
        "使用上次参数运行" => {
            return Ok(Cli {
                command: prev_cli.ok_or_else(|| anyhow::anyhow!("上次命令不存在"))?,
            });
        }
        _ => anyhow::bail!("用户退出了程序。"),
    }?;

    Ok(Cli { command })
}

/// 'UnpackDll' 命令的子菜单
fn unpack_dll_menu(executables: &[String]) -> Vec<TerminalMenuItem> {
    vec![
        label("配置 UnpackDll 命令参数"),
        list("选择解压的 DLL", vec!["ALL", "dsound", "MMDevAPI"]),
        list("选择架构", vec!["Auto/x64", "x86"]),
        list("选择速度", speed_options()),
        list("游戏 exe，用于检测架构 (可选)", executables.to_vec()),
        button("确认！"),
        back_button("Back (返回)"),
    ]
}

/// 生成速度选项 (1.0 ~ 2.5)
fn speed_options() -> Vec<&'static str> {
    vec![
        "1.0", "1.1", "1.2", "1.3", "1.4", "1.5", "1.6", "1.7", "1.8", "1.9", "2.0", "2.1", "2.2",
        "2.3", "2.4", "2.5",
    ]
}

/// 获取当前目录下的文件和文件夹作为 `exec` 的选项
fn exec_options() -> Vec<String> {
    let mut options = vec![NONE_EXEC_ITEM.to_string()]; // 提供不选择的选项
    if let Ok(entries) = fs::read_dir(".") {
        for entry in entries
            .flatten()
            .filter(|e| e.file_type().map(|t| t.is_file()).unwrap_or(false))
            .filter(|e| {
                // 过滤掉自身
                if let Some(name) = e.file_name().to_str()
                    && name.contains(env!("CARGO_PKG_NAME"))
                {
                    return false;
                }
                if let Some(ext) = e.path().extension().and_then(|e| e.to_str()) {
                    return [
                        "exe", //, "bat", "cmd", "ps1", "sh"
                    ]
                    .contains(&ext.to_lowercase().as_str());
                }
                false
            })
        {
            if let Some(name) = entry.file_name().to_str() {
                options.push(name.to_string());
            }
        }
    }
    options
}

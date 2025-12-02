use std::{fs, iter};

use anyhow::Result;
use once_fn::once;
use terminal_menu::{
    TerminalMenuItem, back_button, button, label, list, menu, mut_menu, run, submenu,
};

use crate::{
    cache::GLOBAL_CACHE,
    cli::*,
    utils::{SPEED_MAX, SupportedDLLs},
};

const NONE_EXEC_ITEM: &str = "None";

/// TUI 主函数，引导用户选择命令和参数，并返回一个完整的 Cli 对象
pub fn run_tui() -> Result<Cli> {
    let prev_cli = GLOBAL_CACHE.lock().unwrap().last_command.clone();

    // 2. 构建菜单
    let mut main_menu_items = vec![
        label(concat!(
            env!("CARGO_PKG_NAME"),
            " (v",
            env!("CARGO_PKG_VERSION"),
            "): ",
            env!("CARGO_PKG_REPOSITORY"),
            "  请选择一个要执行的操作，按 q 退出:"
        )),
        submenu("语音加速 (SPEEDUP)", speedup_menu()),
        submenu("语音不中断 (ZeroInterrupt)", zerointerrupt_menu()),
        submenu("检测架构", detect_menu()),
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
        "语音加速 (SPEEDUP)" => {
            let sub_menu = tmp.get_submenu("语音加速 (SPEEDUP)");
            if sub_menu.canceled() {
                anyhow::bail!("用户取消了操作。");
            }
            let exec_selection = sub_menu.selection_value("游戏 exe，用于检测架构 (可选)");
            let exec = if exec_selection == NONE_EXEC_ITEM {
                None
            } else {
                Some(exec_selection.into())
            };
            let speed = match sub_menu.selection_value("选择速度") {
                "None" => None,
                speed => Some(speed.parse()?),
            };
            Ok(Commands::UnpackDll(UnpackDllArgs {
                dll: match sub_menu.selection_value("选择解压的 DLL") {
                    "ALL" => None,
                    spe => Some(spe.to_lowercase().parse().expect("TUI internal error")),
                },
                x86: sub_menu.selection_value("选择架构") == "x86",
                speed,
                exec,
            }))
        }
        "语音不中断 (ZeroInterrupt)" => {
            let sub_menu = tmp.get_submenu("语音不中断 (ZeroInterrupt)");
            if sub_menu.canceled() {
                anyhow::bail!("用户取消了操作。");
            }
            let exec_selection = sub_menu.selection_value("游戏 exe，用于检测架构 (可选)");
            let exec = if exec_selection == NONE_EXEC_ITEM {
                None
            } else {
                Some(exec_selection.into())
            };
            Ok(Commands::UnpackDll(UnpackDllArgs {
                dll: Some(SupportedDLLs::DSoundZeroInterrupt),
                x86: sub_menu.selection_value("选择架构") == "x86",
                speed: None,
                exec,
            }))
        }
        "检测架构" => {
            let sub_menu = tmp.get_submenu("检测架构");
            if sub_menu.canceled() {
                anyhow::bail!("用户取消了操作。");
            }
            let exec_selection = sub_menu.selection_value("游戏 exe，用于检测架构");
            let exe = if exec_selection == NONE_EXEC_ITEM {
                anyhow::bail!("未指定游戏 exe")
            } else {
                exec_selection.into()
            };
            Ok(Commands::Detect(DetectArgs { exe }))
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

/// speedup 子菜单
fn speedup_menu() -> Vec<TerminalMenuItem> {
    vec![
        label("配置 加速 参数"),
        list("选择解压的 DLL", vec!["MMDevAPI", "dsound", "ALL"]),
        list("选择架构", vec!["Auto/x64", "x86"]),
        list("选择速度", speed_options()),
        list("游戏 exe，用于检测架构 (可选)", exec_options()),
        button("确认！"),
        back_button("Back (返回)"),
    ]
}

/// zerointerrupt 子菜单
fn zerointerrupt_menu() -> Vec<TerminalMenuItem> {
    vec![
        label("配置 语音不中断 参数"),
        list("选择架构", vec!["Auto/x64", "x86"]),
        list("游戏 exe，用于检测架构 (可选)", exec_options()),
        button("确认！"),
        back_button("Back (返回)"),
    ]
}

fn detect_menu() -> Vec<TerminalMenuItem> {
    vec![
        label("检测架构"),
        list("游戏 exe，用于检测架构", exec_options()),
        button("确认！"),
        back_button("Back (返回)"),
    ]
}

/// 生成速度选项 (1.0 ~ 2.0)
fn speed_options() -> Vec<String> {
    // 逻辑：从 10 迭代到 SPEED_MAX * 10 (例如 20)
    let start = 10;
    let end = (SPEED_MAX * 10.0) as i32;

    iter::once("None".to_string())
        .chain((start..=end).map(|x| format!("{:.1}", x as f32 / 10.0)))
        .collect()
}

/// 获取当前目录下的文件和文件夹作为 `exec` 的选项
#[once]
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

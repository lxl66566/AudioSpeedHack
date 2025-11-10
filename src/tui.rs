use std::{fs, path::PathBuf};

use anyhow::{Context, Result};
use cpal::traits::{DeviceTrait, HostTrait};
use terminal_menu::{
    TerminalMenuItem, back_button, button, label, list, menu, mut_menu, run, submenu,
};

use crate::{cache::GLOBAL_CACHE, cli::*, device::DeviceType};

const NONE_EXEC_ITEM: &str = "None";

/// 非交互式地获取可用设备名称列表
fn get_device_names(device_type: DeviceType) -> Result<Vec<String>> {
    let host = cpal::default_host();
    let devices = host.devices().context("无法获取音频设备列表")?;

    let names = devices
        .filter_map(|d| {
            let config_ok = match device_type {
                DeviceType::Input => d.default_input_config().is_ok(),
                DeviceType::Output => d.default_output_config().is_ok(),
            };
            if config_ok { d.name().ok() } else { None }
        })
        .collect::<Vec<_>>();

    Ok(names)
}

/// TUI 主函数，引导用户选择命令和参数，并返回一个完整的 Cli 对象
pub fn run_tui() -> Result<Cli> {
    // 1. 在启动菜单前，预先获取所有需要的数据
    let input_devices = get_device_names(DeviceType::Input)?;
    let output_devices = get_device_names(DeviceType::Output)?;
    let executable_options = exec_options();
    let prev_cli = GLOBAL_CACHE.lock().unwrap().last_command.clone();

    // 2. 构建菜单
    let mut main_menu_items = vec![
        label("请选择一个要执行的操作，按 q 退出:"),
        submenu(
            "UnpackAndStart (解压并启动)",
            unpack_and_start_menu(&input_devices, &output_devices, &executable_options),
        ),
        submenu(
            "Start (启动程序)",
            start_menu(&input_devices, &output_devices, &executable_options),
        ),
        submenu("UnpackDll (解压DLL)", unpack_dll_menu()),
        button("ListDevices (列出设备)"),
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
        "ListDevices (列出设备)" => anyhow::Ok(Commands::ListDevices),
        "Clean (清除 AudioSpeedHack 残留)" => anyhow::Ok(Commands::Clean),
        "UnpackDll (解压DLL)" => {
            let sub_menu = tmp.get_submenu("UnpackDll (解压DLL)");
            if sub_menu.canceled() {
                anyhow::bail!("用户在 UnpackDll 菜单中取消了操作。");
            }
            Ok(Commands::UnpackDll(UnpackDllArgs {
                dll: None,
                x86: sub_menu.selection_value("选择架构") == "x86",
                speed: sub_menu.selection_value("选择速度").parse()?,
            }))
        }
        "Start (启动程序)" => {
            let sub_menu = tmp.get_submenu("Start (启动程序)");
            if sub_menu.canceled() {
                anyhow::bail!("用户在 Start 菜单中取消了操作。");
            }

            let selected_input_name = sub_menu.selection_value("选择输入设备");
            let input_device_index = input_devices
                .iter()
                .position(|r| r == selected_input_name)
                .context("选择的输入设备无效")?;

            let selected_output_name = sub_menu.selection_value("选择输出设备");
            let output_device_index = output_devices
                .iter()
                .position(|r| r == selected_output_name)
                .context("选择的输出设备无效")?;

            let exec_selection = sub_menu.selection_value("执行程序 (可选)");
            let exec = if exec_selection == NONE_EXEC_ITEM {
                None
            } else {
                Some(PathBuf::from(exec_selection))
            };

            Ok(Commands::Start(StartArgs {
                input_device: input_device_index,
                output_device: output_device_index,
                speed: sub_menu.selection_value("选择速度").parse()?,
                exec,
            }))
        }
        "UnpackAndStart (解压并启动)" => {
            let sub_menu = tmp.get_submenu("UnpackAndStart (解压并启动)");
            if sub_menu.canceled() {
                anyhow::bail!("用户在 UnpackAndStart 菜单中取消了操作。");
            }

            let selected_input_name = sub_menu.selection_value("选择输入设备");
            let input_device_index = input_devices
                .iter()
                .position(|r| r == selected_input_name)
                .context("选择的输入设备无效")?;

            let selected_output_name = sub_menu.selection_value("选择输出设备");
            let output_device_index = output_devices
                .iter()
                .position(|r| r == selected_output_name)
                .context("选择的输出设备无效")?;

            let exec_selection = sub_menu.selection_value("执行程序 (可选)");
            let exec = if exec_selection == NONE_EXEC_ITEM {
                None
            } else {
                Some(exec_selection.to_string())
            };

            Ok(Commands::UnpackAndStart(UnpackAndStartArgs {
                dll: None,
                x86: sub_menu.selection_value("选择架构") == "x86",
                input_device: input_device_index,
                output_device: output_device_index,
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
fn unpack_dll_menu() -> Vec<TerminalMenuItem> {
    vec![
        label("配置 UnpackDll 命令参数"),
        list("选择架构", vec!["x64", "x86"]),
        list("选择速度", speed_options()),
        button("确认！"),
        back_button("Back (返回)"),
    ]
}

/// 'Start' 命令的子菜单
fn start_menu<'a>(
    input_devices: &'a [String],
    output_devices: &'a [String],
    executables: &'a [String],
) -> Vec<TerminalMenuItem> {
    vec![
        label("配置 Start 命令参数"),
        list("选择输入设备", input_devices.to_vec()),
        list("选择输出设备", output_devices.to_vec()),
        list("选择速度", speed_options()),
        list("执行程序 (可选)", executables.to_vec()),
        button("确认！"),
        back_button("Back (返回)"),
    ]
}

/// 'UnpackAndStart' 命令的子菜单
fn unpack_and_start_menu<'a>(
    input_devices: &'a [String],
    output_devices: &'a [String],
    executables: &'a [String],
) -> Vec<TerminalMenuItem> {
    vec![
        label("配置 UnpackAndStart 命令参数"),
        list("选择架构", vec!["Auto/x64", "x86"]),
        list("选择输入设备", input_devices.to_vec()),
        list("选择输出设备", output_devices.to_vec()),
        list("选择速度", speed_options()),
        list("执行程序 (可选)", executables.to_vec()),
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
                    return ["exe", "bat", "cmd", "ps1", "sh"]
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

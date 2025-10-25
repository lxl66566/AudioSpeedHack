
use anyhow::Result;
use cpal::traits::{DeviceTrait, HostTrait};
use terminal_menu::{button, label, menu, mut_menu, run};

#[derive(Debug, Clone, Copy)]
pub enum DeviceType {
    Input,
    Output,
}

impl std::fmt::Display for DeviceType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            DeviceType::Input => write!(f, "输入"),
            DeviceType::Output => write!(f, "输出"),
        }
    }
}

/// 枚举并列出所有可用的音频输入和输出设备及其支持的流配置。
pub fn list_all_devices() -> Result<()> {
    let host = cpal::default_host();

    // --- 列出输入设备 ---
    println!("Input Devices:");
    let input_devices = host.input_devices()?;
    let mut has_input_devices = false;
    for (index, device) in input_devices.enumerate() {
        has_input_devices = true;
        let device_name = device
            .name()
            .unwrap_or_else(|_| "Unknown Device".to_string());
        println!("  [{}] {}", index, device_name);

        // 打印支持的输入配置
        if let Ok(configs) = device.supported_input_configs() {
            for config in configs {
                println!(
                    "    - Channels: {}, Sample Format: {:?}, Sample Rate: {} - {}",
                    config.channels(),
                    config.sample_format(),
                    config.min_sample_rate().0,
                    config.max_sample_rate().0
                );
            }
        }
    }
    if !has_input_devices {
        println!("  No input devices found.");
    }
    println!(); // 添加空行以分隔

    // --- 列出输出设备 ---
    println!("Output Devices:");
    let output_devices = host.output_devices()?;
    let mut has_output_devices = false;
    for (index, device) in output_devices.enumerate() {
        has_output_devices = true;
        let device_name = device
            .name()
            .unwrap_or_else(|_| "Unknown Device".to_string());
        println!("  [{}] {}", index, device_name);

        // 打印支持的输出配置
        if let Ok(configs) = device.supported_output_configs() {
            for config in configs {
                println!(
                    "    - Channels: {}, Sample Format: {:?}, Sample Rate: {} - {}",
                    config.channels(),
                    config.sample_format(),
                    config.min_sample_rate().0,
                    config.max_sample_rate().0
                );
            }
        }
    }
    if !has_output_devices {
        println!("  No output devices found.");
    }

    Ok(())
}

pub fn select_device(host: &cpal::Host, device_type: DeviceType) -> anyhow::Result<cpal::Device> {
    let mut devices = host
        .devices()?
        .filter(|d| match device_type {
            DeviceType::Input => d.default_input_config().is_ok(),
            DeviceType::Output => d.default_output_config().is_ok(),
        })
        .collect::<Vec<_>>();

    if devices.is_empty() {
        anyhow::bail!("未找到可用的{}设备。", device_type);
    }

    // 创建菜单项
    let mut menu_items = vec![
        label("----------------------"),
        label(format!("请选择一个{}设备:", device_type)),
        label("使用方向键选择, Enter确认"),
        label("'q' 或 esc 退出"),
        label("-----------------------"),
    ];

    // 将每个设备添加为按钮
    for device in &devices {
        let device_name = device.name().unwrap_or_else(|_| "Unknown".to_string());
        menu_items.push(button(device_name));
    }

    let menu = menu(menu_items);
    run(&menu);

    if mut_menu(&menu).canceled() {
        anyhow::bail!("用户取消了设备选择。");
    }

    // 获取选择的设备名称并找到对应的设备
    let binding = mut_menu(&menu);
    let selected_name = binding.selected_item_name();
    let selected_index = devices
        .iter()
        .position(|d| d.name().unwrap_or_else(|_| "Unknown".to_string()) == selected_name)
        .unwrap(); // 在这个逻辑下，我们总是能找到它

    // remove and return the selected device
    Ok(devices.remove(selected_index))
}

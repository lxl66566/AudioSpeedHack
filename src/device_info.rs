use anyhow::{Result, anyhow};
use cpal::traits::{DeviceTrait, HostTrait};
use std::io::{self, Write as _};

fn select_device(
    devices_iter: impl Iterator<Item = cpal::Device>,
    device_type: &str,
) -> Result<cpal::Device> {
    // 将迭代器收集到 Vec 中以便通过索引访问
    let devices: Vec<_> = devices_iter.collect();

    if devices.is_empty() {
        return Err(anyhow!("No {} devices available.", device_type));
    }

    // 打印设备列表
    for (index, device) in devices.iter().enumerate() {
        let name = device
            .name()
            .unwrap_or_else(|_| "<Unknown Name>".to_string());
        println!("[{}] {}", index, name);
    }

    // 循环等待用户输入有效的索引
    loop {
        print!(
            "Please select {} device [0-{}]: ",
            device_type,
            devices.len() - 1
        );
        io::stdout().flush()?; // 确保提示符立即显示

        let mut input_text = String::new();
        io::stdin().read_line(&mut input_text)?;

        match input_text.trim().parse::<usize>() {
            Ok(index) if index < devices.len() => {
                // 用户选择了有效的索引，返回对应的设备
                // 因为我们需要返回设备的所有权，所以这里 clone 一份 (cpal 设备句柄通常开销很小)
                // 或者使用 into_iter().nth(index) 也可以，但上面已经 collect 了。
                return Ok(devices[index].clone());
            }
            _ => {
                println!("Invalid selection, please try again.");
            }
        }
    }
}

fn main() -> Result<()> {
    // --- 1. 设置音频设备 ---
    let host = cpal::default_host();

    // 选择输入设备
    println!("--- Select Input Device ---");
    let input_devices = host.input_devices().expect("Failed to get input devices");
    let input_device = select_device(input_devices, "input")?;
    println!("Selected input device: {}\n", input_device.name()?);

    // 选择输出设备
    println!("--- Select Output Device ---");
    let output_devices = host.output_devices().expect("Failed to get output devices");
    let output_device = select_device(output_devices, "output")?;
    println!("Selected output device: {}\n", output_device.name()?);

    println!(
        "--- Supported Input Stream Configs for {} ---",
        input_device.name()?
    );
    if let Ok(configs) = input_device.supported_input_configs() {
        for (i, config) in configs.enumerate() {
            println!(
                "  [{}] Channels: {}, Sample Format: {:?}, Sample Rate: {} - {}",
                i,
                config.channels(),
                config.sample_format(),
                config.min_sample_rate().0,
                config.max_sample_rate().0
            );
        }
    } else {
        println!("  Could not retrieve supported input configs.");
    }
    println!("\n");

    println!(
        "--- Supported Output Stream Configs for {} ---",
        output_device.name()?
    );
    if let Ok(configs) = output_device.supported_output_configs() {
        for (i, config) in configs.enumerate() {
            println!(
                "  [{}] Channels: {}, Sample Format: {:?}, Sample Rate: {} - {}",
                i,
                config.channels(),
                config.sample_format(),
                config.min_sample_rate().0,
                config.max_sample_rate().0
            );
        }
    } else {
        println!("  Could not retrieve supported output configs.");
    }
    println!("\n");

    println!("Please update the constants in the code based on the supported configs above.");
    println!(
        "The program will now exit. Comment out this section to run the full audio processing."
    );

    Ok(())
}

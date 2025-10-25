use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use pitch_shift::PitchShifter;
use ringbuf::HeapRb;
use std::io;

const PITCH_SHIFT_SEMITONES: f32 = -12.0; // 降低一个八度 (12个半音)
const OVERSAMPLING: usize = 16; // pitch_shifter 的处理质量，值越高，质量越好，CPU占用越高
const WINDOW_DURATION_MS: usize = 18; // pitch_shifter 的窗口时长，18ms 是一个不错的默认值。过小或者过大都会导致声音失真。
const BUFFER_LATENCY_MS: u64 = 100; // 我们的环形缓冲区的延迟，用于平滑输入和输出

fn main() -> anyhow::Result<()> {
    println!("--- Rust 音频实时变调程序 ---");

    // 1. 初始化音频主机
    let host = cpal::default_host();

    // 2. 选择输入和输出设备
    let input_device = select_device(&host, "输入")?;
    let output_device = select_device(&host, "输出")?;

    println!("\n选择的设备:");
    println!("  输入: {}", input_device.name()?);
    println!("  输出: {}", output_device.name()?);

    // 3. 获取并配置设备
    // MODIFIED: 我们不再简单地获取两个默认配置然后比较，而是以输入配置为基准，
    // 为输出设备寻找一个兼容的配置。
    let in_config = input_device.default_input_config()?;
    let sample_rate = in_config.sample_rate();
    let input_channels = in_config.channels() as usize;

    // 我们只处理 f32 格式的音频样本
    if in_config.sample_format() != cpal::SampleFormat::F32 {
        anyhow::bail!("仅支持 f32 采样格式的输入设备。");
    }

    // NEW: 寻找一个支持我们输入设备采样率和格式的输出配置。
    // 这是更健壮的做法，而不是依赖可能不匹配的默认配置。
    let supported_out_configs = output_device
        .supported_output_configs()?
        .find(|c| {
            c.sample_format() == cpal::SampleFormat::F32 && c.max_sample_rate() >= sample_rate
        })
        .ok_or_else(|| anyhow::anyhow!("输出设备上未找到支持 f32 格式的配置"))?;

    // 使用输入设备的采样率来创建输出配置
    let out_config: cpal::StreamConfig = supported_out_configs.with_sample_rate(sample_rate).into();
    let output_channels = out_config.channels as usize;

    println!("\n音频流配置:");
    println!("  采样率: {} Hz", sample_rate.0);
    println!("  采样格式: f32");
    println!("  输入声道数: {}", input_channels);
    println!(
        "  输出声道数: {} (将把输入声道映射到前 {} 个声道)",
        output_channels,
        input_channels.min(output_channels)
    );

    // 4. 创建 PitchShifter 实例
    // MODIFIED: PitchShifter 的数量由输入声道数决定
    let mut shifters: Vec<PitchShifter> = (0..input_channels)
        .map(|_| PitchShifter::new(WINDOW_DURATION_MS, sample_rate.0 as usize))
        .collect();

    // 5. 设置环形缓冲区 (Ring Buffer)
    // MODIFIED: 缓冲区大小也由输入声道数决定
    let buffer_size =
        (BUFFER_LATENCY_MS as f32 / 1000.0 * sample_rate.0 as f32) as usize * input_channels;
    let ring_buffer = HeapRb::<f32>::new(buffer_size);
    let (mut producer, mut consumer) = ring_buffer.split();

    // 6. 创建并运行输入流 (这部分无需改变)
    let input_stream = input_device.build_input_stream(
        &in_config.into(),
        move |data: &[f32], _: &cpal::InputCallbackInfo| {
            let pushed = producer.push_slice(data);
            if pushed < data.len() {
                // eprintln!("输入缓冲区溢出");
            }
        },
        |err| eprintln!("输入流错误: {}", err),
        None,
    )?;

    // 7. 创建并运行输出流
    // MODIFIED: 输出回调逻辑现在需要处理声道数不匹配的情况
    let output_stream = output_device.build_output_stream(
        &out_config, // 使用我们新创建的输出配置
        move |data: &mut [f32], _: &cpal::OutputCallbackInfo| {
            // `data` 是输出设备的缓冲区，其大小为 `num_samples * output_channels`
            let num_samples = data.len() / output_channels;

            // NEW: 创建一个临时缓冲区来从 ringbuf 中读取数据，其大小基于输入声道数
            let mut input_buffer = vec![0.0; num_samples * input_channels];
            let read_count = consumer.pop_slice(&mut input_buffer);

            // 如果数据不足，用静音填充
            if read_count < input_buffer.len() {
                input_buffer[read_count..].iter_mut().for_each(|s| *s = 0.0);
            }

            // --- 核心处理逻辑 (基于输入声道数) ---
            // 1. 分离声道 (De-interleave)
            let mut channel_buffers: Vec<Vec<f32>> = vec![vec![0.0; num_samples]; input_channels];
            for i in 0..num_samples {
                for ch in 0..input_channels {
                    channel_buffers[ch][i] = input_buffer[i * input_channels + ch];
                }
            }

            // 2. 对每个声道应用变调
            let mut processed_channels: Vec<Vec<f32>> =
                vec![vec![0.0; num_samples]; input_channels];
            for ch in 0..input_channels {
                shifters[ch].shift_pitch(
                    OVERSAMPLING,
                    PITCH_SHIFT_SEMITONES,
                    &channel_buffers[ch],
                    &mut processed_channels[ch],
                );
            }

            // 3. 合并声道 (Interleave) - NEW: 这是关键的修改
            // 我们将处理好的 `input_channels` 个声道的数据，映射到 `output_channels` 个声道的输出缓冲区中
            for (i, frame) in data.chunks_exact_mut(output_channels).enumerate() {
                // 先将整个输出帧填充为静音
                frame.fill(0.0);
                // 然后将我们处理好的声道数据复制到前几个位置
                // 例如，如果输入是2声道，输出是8声道，这将把立体声数据放到前左和前右扬声器上
                for ch in 0..input_channels {
                    if ch < frame.len() {
                        // 安全检查
                        frame[ch] = processed_channels[ch][i];
                    }
                }
            }
        },
        |err| eprintln!("输出流错误: {}", err),
        None,
    )?;

    // 8. 启动音频流
    input_stream.play()?;
    output_stream.play()?;

    println!("\n>>> 音频流已启动！正在将麦克风输入降低一个八度后播放。");
    println!(">>> 按 Enter 键退出程序。");

    // 9. 保持主线程运行
    let mut _buffer = String::new();
    io::stdin().read_line(&mut _buffer)?;

    println!("程序退出。");
    Ok(())
}

// 辅助函数 `select_device` 无需修改，保持原样
fn select_device(host: &cpal::Host, device_type: &str) -> anyhow::Result<cpal::Device> {
    let mut devices = host
        .devices()?
        .filter(|d| {
            if device_type == "输入" {
                d.default_input_config().is_ok()
            } else {
                d.default_output_config().is_ok()
            }
        })
        .collect::<Vec<_>>();

    if devices.is_empty() {
        anyhow::bail!("未找到可用的{}设备。", device_type);
    }

    println!("\n请选择一个{}设备:", device_type);
    for (i, device) in devices.iter().enumerate() {
        println!(
            "  {}: {}",
            i,
            device.name().unwrap_or_else(|_| "未知设备".to_string())
        );
    }

    loop {
        print!("请输入设备编号: ");
        io::Write::flush(&mut io::stdout())?;

        let mut input = String::new();
        io::stdin().read_line(&mut input)?;
        match input.trim().parse::<usize>() {
            Ok(index) if index < devices.len() => {
                return Ok(devices.remove(index));
            }
            _ => {
                println!("无效的输入，请输入列表中的数字。");
            }
        }
    }
}

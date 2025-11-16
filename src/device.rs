use std::io;

use ::log::{error, info};
use anyhow::{Result, anyhow};
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use pitch_shift::PitchShifter;
use ringbuf::HeapRb;

use crate::utils::AudioExt;

pub const OVERSAMPLING: usize = 16; // pitch_shifter 的处理质量，值越高，质量越好，CPU占用越高
pub const WINDOW_DURATION_MS: usize = 22; // pitch_shifter 的窗口时长，18ms 是一个不错的默认值。过小或者过大都会导致声音失真。
pub const BUFFER_LATENCY_MS: u64 = 100; // 我们的环形缓冲区的延迟，用于平滑输入和输出

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

/// 管理音频输入和输出设备
pub struct DeviceManager {
    pub host: cpal::Host,
    pub input_device: Option<cpal::Device>,
    pub output_device: Option<cpal::Device>,
}

impl Default for DeviceManager {
    fn default() -> Self {
        Self {
            host: cpal::default_host(),
            input_device: None,
            output_device: None,
        }
    }
}

impl DeviceManager {
    #[inline]
    fn set_device(&mut self, device_type: DeviceType, device: cpal::Device) {
        match device_type {
            DeviceType::Input => self.input_device = Some(device),
            DeviceType::Output => self.output_device = Some(device),
        }
    }

    /// 枚举并列出所有可用的音频输入和输出设备及其支持的流配置。
    pub fn list_all_devices(&self) -> Result<()> {
        println!("Input Devices:");
        let input_devices = self.host.input_devices()?;
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
        let output_devices = self.host.output_devices()?;
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

    /// 根据索引选择输入输出设备
    pub fn select_device(&mut self, device_type: DeviceType, index: usize) -> Result<()> {
        let mut devices = self
            .host
            .devices()?
            .filter(|d| match device_type {
                DeviceType::Input => d.default_input_config().is_ok(),
                DeviceType::Output => d.default_output_config().is_ok(),
            })
            .collect::<Vec<_>>();

        if devices.is_empty() {
            anyhow::bail!("未找到可用的{}设备。", device_type);
        }

        self.set_device(device_type, devices.remove(index));
        Ok(())
    }

    pub fn run_process(self, speed: f32) -> anyhow::Result<()> {
        let input_device = self.input_device.expect("输入设备不存在");
        let output_device = self.output_device.expect("输出设备不存在");

        info!(
            "输入设备: {}，输出设备: {}",
            input_device.name()?,
            output_device.name()?
        );

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
        let out_config: cpal::StreamConfig = supported_out_configs
            .try_with_sample_rate(sample_rate)
            .ok_or_else(|| anyhow!("sample rate out of range: {}", sample_rate.0))?
            .into();
        let output_channels = out_config.channels as usize;

        info!("采样率: {} Hz", sample_rate.0);
        info!("采样格式: f32");
        info!("输入声道数: {}", input_channels);
        info!(
            "输出声道数: {} (将把输入声道映射到前 {} 个声道)",
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

        // 6. 创建并运行输入流
        let input_stream = input_device.build_input_stream(
            &in_config.into(),
            move |data: &[f32], _: &cpal::InputCallbackInfo| {
                let pushed = producer.push_slice(data);
                if pushed < data.len() {
                    // eprintln!("输入缓冲区溢出");
                }
            },
            |err| error!("输入流错误: {}", err),
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
                let mut channel_buffers: Vec<Vec<f32>> =
                    vec![vec![0.0; num_samples]; input_channels];
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
                        speed.to_pitch(),
                        &channel_buffers[ch],
                        &mut processed_channels[ch],
                    );
                }

                // 3. 合并声道 (Interleave) - NEW: 这是关键的修改
                // 我们将处理好的 `input_channels` 个声道的数据，映射到 `output_channels`
                // 个声道的输出缓冲区中
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
            |err| error!("输出流错误: {}", err),
            None,
        )?;

        // 8. 启动音频流
        input_stream.play()?;
        output_stream.play()?;

        info!(
            "音频流已启动！正在降低音高：speed {:.1}, pitch {:.4}",
            speed,
            speed.to_pitch()
        );
        info!("按 Enter 键退出程序。");

        let mut _buffer = String::new();
        io::stdin().read_line(&mut _buffer)?;
        Ok(())
    }
}

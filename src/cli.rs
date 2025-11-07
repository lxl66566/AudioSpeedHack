use std::path::PathBuf;

use clap::{Args, Parser, Subcommand};
use serde::{Deserialize, Serialize};

use crate::utils::SupportedDLLs;

/// 基于 dsound 的游戏音频加速器
#[derive(Parser, Debug)]
#[command(author, version, about)]
pub struct Cli {
    #[command(subcommand)]
    pub command: Commands,
}

#[derive(Subcommand, Debug, Serialize, Deserialize, Clone)]
pub enum Commands {
    /// 列出所有可用的输入输出音频设备及其配置与索引
    #[clap(alias = "l")]
    ListDevices,

    /// 根据选择，解压相应的 DLL
    #[clap(alias = "u")]
    UnpackDll(UnpackDllArgs),

    /// 启动音频处理程序
    #[clap(alias = "s")]
    Start(StartArgs),

    /// 解压 DLL 并立即启动音频处理程序
    #[clap(alias = "us")]
    UnpackAndStart(UnpackAndStartArgs),

    /// 还原所有 AudioSpeedHack 所做的更改，包括注册表项和 DLL 文件
    #[clap(alias = "c")]
    Clean,
}

/// 'unpack-dll' 命令的参数
#[derive(Args, Debug, Serialize, Deserialize, Clone)]
pub struct UnpackDllArgs {
    /// 指定解压的 DLL 类型，未指定则全部解压
    #[arg(short, long)]
    pub dll: Option<SupportedDLLs>,

    /// 指定解压 win64 平台的 DLL (若不指定，则默认为 win32)
    #[arg(long)]
    pub win64: bool,

    /// 设置速度参数 (范围: 1.0 ~ 2.5)
    #[arg(short, long, value_parser = validate_speed)]
    pub speed: f32,
}

/// 'start' 命令的参数
#[derive(Args, Debug, Serialize, Deserialize, Clone)]
pub struct StartArgs {
    /// 指定输入设备的索引
    #[arg(short, long)]
    pub input_device: usize,

    /// 指定输出设备的索引
    #[arg(short, long)]
    pub output_device: usize,

    /// 设置速度参数 (范围: 1.0 ~ 2.5)
    #[arg(short, long, value_parser = validate_speed)]
    pub speed: f32,

    /// 开始加速并执行命令或外部程序
    #[arg(long)]
    pub exec: Option<PathBuf>,
}

/// 这个命令组合了 'unpack-dll' 和 'start' 的所有参数。
#[derive(Args, Debug, Serialize, Deserialize, Clone)]
pub struct UnpackAndStartArgs {
    /// 指定解压的 DLL 类型，未指定则全部解压
    #[arg(short, long)]
    pub dll: Option<SupportedDLLs>,

    /// 指定解压 win64 平台的 DLL (若不指定，则默认为 win32)
    #[arg(long)]
    pub win64: bool,

    /// 指定输入设备的索引
    #[arg(short, long)]
    pub input_device: usize,

    /// 指定输出设备的索引
    #[arg(short, long)]
    pub output_device: usize,

    /// 设置速度参数 (范围: 1.0 ~ 2.5)
    #[arg(short, long, value_parser = validate_speed)]
    pub speed: f32,

    /// 开始加速并执行命令或外部程序
    #[arg(long)]
    pub exec: Option<String>,
}

/// 自定义验证函数，用于检查 speed 参数是否在 1.0 到 2.5 的范围内。
#[allow(unused)] // clap used
fn validate_speed(s: &str) -> Result<f32, String> {
    // 尝试将输入字符串解析为 f32
    let speed: f32 = s
        .parse()
        .map_err(|_| format!("`{}` 不是一个有效的浮点数", s))?;

    // 检查解析出的值是否在范围内
    if (1.0..=2.5).contains(&speed) {
        Ok(speed)
    } else {
        Err(format!(
            "速度参数必须在 1.0 到 2.5 的范围内，但输入的是 {}",
            speed
        ))
    }
}

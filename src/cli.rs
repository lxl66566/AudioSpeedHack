use std::path::PathBuf;

use clap::{Args, Parser, Subcommand};
use serde::{Deserialize, Serialize};

use crate::utils::{SPEED_MAX, SupportedDLLs};

/// 基于 dsound 的游戏音频加速器
#[derive(Parser, Debug)]
#[command(author, version, about = concat!(env!("CARGO_PKG_NAME"), ": ", env!("CARGO_PKG_DESCRIPTION"), "\nrepo: ", env!("CARGO_PKG_REPOSITORY")))]
pub struct Cli {
    #[command(subcommand)]
    pub command: Commands,
}

#[derive(Subcommand, Debug, Serialize, Deserialize, Clone)]
pub enum Commands {
    /// 根据选择，解压相应的 DLL
    #[clap(alias = "u")]
    UnpackDll(UnpackDllArgs),

    /// 还原所有 AudioSpeedHack 所做的更改，包括注册表项和 DLL 文件
    #[clap(alias = "c")]
    Clean,

    /// 检测并输出指定 exe 的架构
    #[clap(alias = "d")]
    Detect(DetectArgs),
}

/// 'unpack-dll' 命令的参数
#[derive(Args, Debug, Serialize, Deserialize, Clone)]
pub struct UnpackDllArgs {
    /// 指定解压的 DLL 类型，未指定则全部解压
    #[arg(short, long)]
    pub dll: Option<SupportedDLLs>,

    /// 指定解压 x86 平台的 DLL (若不指定，则默认为 x64)
    #[arg(long)]
    pub x86: bool,

    /// 设置速度参数 (范围: 1.0 ~ 2.0)
    #[arg(short, long, value_parser = validate_speed)]
    pub speed: f32,

    /// 开始加速并执行命令或外部程序。指定此项可以自动检测 x86 或 x64 架构。
    #[arg(long)]
    pub exec: Option<PathBuf>,
}

/// 'detect' 命令的参数
#[derive(Args, Debug, Serialize, Deserialize, Clone)]
pub struct DetectArgs {
    /// 指定要检测的 exe 文件
    pub exe: PathBuf,
}

/// 自定义验证函数，用于检查 speed 参数是否在 1.0 到 2.0 的范围内。
#[allow(unused)] // clap used
fn validate_speed(s: &str) -> Result<f32, String> {
    // 尝试将输入字符串解析为 f32
    let speed: f32 = s
        .parse()
        .map_err(|_| format!("`{}` 不是一个有效的浮点数", s))?;

    // 检查解析出的值是否在范围内
    if (1.0..=SPEED_MAX).contains(&speed) {
        Ok(speed)
    } else {
        Err(format!(
            "速度参数必须在 1.0 到 2.0 的范围内，但输入的是 {}",
            speed
        ))
    }
}

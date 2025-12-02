use std::path::PathBuf;

use clap::{Args, Parser, Subcommand};
use serde::{Deserialize, Serialize};

use crate::utils::SupportedDLLs;

/// 基于 dsound 的游戏音频加速器
#[derive(Parser, Debug)]
#[command(author, version, about = concat!(
    env!("CARGO_PKG_NAME"),
    " (v",
    env!("CARGO_PKG_VERSION"),
    "), repo: ",
    env!("CARGO_PKG_REPOSITORY")
))]
pub struct Cli {
    #[command(subcommand)]
    pub command: Commands,
}

#[derive(Subcommand, Debug, Serialize, Deserialize, Clone)]
pub enum Commands {
    /// 根据选择，解压相应的 DLL
    #[clap(alias = "u")]
    UnpackDll(UnpackDllArgs),

    /// 还原所有 AudioSpeedHack 所做的更改，包括注册表项、DLL 文件和环境变量
    #[clap(alias = "c")]
    Clean,

    /// 检测并输出指定 exe 的架构
    #[clap(alias = "d")]
    Detect(DetectArgs),
}

/// 'unpack-dll' 命令的参数
#[derive(Args, Debug, Serialize, Deserialize, Clone)]
pub struct UnpackDllArgs {
    /// 指定解压的 DLL 类型，未指定则解压全部加速相关 DLL
    #[arg(short, long)]
    pub dll: Option<SupportedDLLs>,

    /// 指定解压 x86 平台的 DLL (若不指定，则默认为 x64)
    #[arg(long)]
    pub x86: bool,

    /// 设置速度参数
    #[arg(short, long)]
    pub speed: Option<f32>,

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

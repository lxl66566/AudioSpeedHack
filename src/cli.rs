use clap::{Args, Parser, Subcommand};

/// 基于 dsound 的游戏音频加速器
#[derive(Parser, Debug)]
#[command(author, version, about)]
pub struct Cli {
    #[command(subcommand)]
    pub command: Commands,
}

#[derive(Subcommand, Debug)]
pub enum Commands {
    /// 修改注册表，使游戏优先从当前目录加载 dsound.dll
    PreRegister,

    /// 列出所有可用的输入输出音频设备及其配置与索引
    ListDevices,

    /// 根据选择，解压相应的 DLL
    UnpackDll(UnpackDllArgs),

    /// 启动音频处理程序
    Start(StartArgs),

    /// 解压 DLL 并立即启动音频处理程序
    UnpackAndStart(UnpackAndStartArgs),
}

/// 'unpack-dll' 命令的参数
#[derive(Args, Debug)]
pub struct UnpackDllArgs {
    /// 指定解压 win64 平台的 DLL (若不指定，则默认为 win32)
    #[arg(long)]
    win64: bool,

    /// 设置速度参数 (范围: 1.0 ~ 2.5)
    #[arg(short, long)]
    speed: f32,
}

/// 'start' 命令的参数
#[derive(Args, Debug)]
pub struct StartArgs {
    /// 指定输入设备的索引
    #[arg(short, long)]
    input_device: usize,

    /// 指定输出设备的索引
    #[arg(short, long)]
    output_device: usize,

    /// 开始加速并执行命令或外部程序
    #[arg(long)]
    exec: Option<String>,
}

/// 'unpack-and-start' 命令的参数
///
/// 这个命令组合了 'unpack-dll' 和 'start' 的所有参数。
#[derive(Args, Debug)]
pub struct UnpackAndStartArgs {
    #[command(flatten)]
    unpack_args: UnpackDllArgs,

    #[command(flatten)]
    start_args: StartArgs,
}

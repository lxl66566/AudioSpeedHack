pub mod asset;
pub mod cli;
pub mod device;
pub mod log;
pub mod tui;
pub mod utils;

use std::{env, fs, process};

use ::log::{error, info};
use clap::Parser;

use crate::{
    cli::{Cli, Commands},
    device::{DeviceManager, DeviceType},
};

struct PauseGuard<'a> {
    msg: &'a str,
}

impl<'a> PauseGuard<'a> {
    fn new(msg: &'a str) -> Self {
        PauseGuard { msg }
    }
}

impl<'a> Drop for PauseGuard<'a> {
    fn drop(&mut self) {
        println!("{}", self.msg);
        std::io::stdin().read_line(&mut String::new()).unwrap();
    }
}

fn main() -> anyhow::Result<()> {
    log::log_init();

    let cli = if std::env::args().len() > 1 {
        Cli::parse()
    } else {
        info!("TUI mode");
        tui::run_tui()?
    };

    info!("args: {:?}", cli);

    match cli.command {
        Commands::ListDevices => {
            let _pause_guard = PauseGuard::new("按任意键退出...");
            DeviceManager::default().list_all_devices()?;
        }
        Commands::UnpackDll(args) => {
            let system = if args.win64 {
                utils::System::Win64
            } else {
                utils::System::Win32
            };
            asset::extract_selected_assets(system, args.speed, env::current_dir()?)?;
            info!("所有 dll 已解压到当前目录。");
        }
        Commands::Start(args) => {
            let mut device_manager = DeviceManager::default();
            device_manager.select_device(DeviceType::Input, args.input_device)?;
            device_manager.select_device(DeviceType::Output, args.output_device)?;
            let _child;
            if let Some(exec) = args.exec {
                _child = process::Command::new(exec).spawn()?;
            }
            device_manager.run_process(args.speed)?;
        }
        Commands::UnpackAndStart(args) => {
            let extracted = asset::extract_selected_assets(
                utils::System::Win32,
                args.speed,
                env::current_dir()?,
            )?;
            info!("extracted files: {extracted:?}");
            let mut device_manager = DeviceManager::default();
            device_manager.select_device(DeviceType::Input, args.input_device)?;
            device_manager.select_device(DeviceType::Output, args.output_device)?;
            let _child;
            if let Some(exec) = args.exec {
                _child = process::Command::new(exec)
                    .env("DSOAL_LOGFILE", "dsoal_error.txt")
                    .spawn()?;
            }
            device_manager.run_process(args.speed)?;
            for f in extracted {
                let _pause_guard;
                if let Err(e) = fs::remove_file(&f) {
                    error!("Failed to remove file {f:?}: {e}");
                    _pause_guard = PauseGuard::new("删除解压的 dll 失败，按任意键继续...");
                }
            }
        }
    }

    Ok(())
}

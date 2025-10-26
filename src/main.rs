pub mod asset;
pub mod cli;
pub mod device;
pub mod log;
pub mod tui;
pub mod utils;

use std::{env, fs, process};

use clap::Parser;

use crate::{
    cli::{Cli, Commands},
    device::{DeviceManager, DeviceType},
};
use ::log::{debug, info};

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
            DeviceManager::default().list_all_devices()?;
        }
        Commands::UnpackDll(args) => {
            let system = if args.win64 {
                utils::System::Win64
            } else {
                utils::System::Win32
            };
            asset::extract_selected_assets(system, args.speed, env::current_dir()?)?;
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
            debug!("extracted files: {extracted:?}");
            let mut device_manager = DeviceManager::default();
            device_manager.select_device(DeviceType::Input, args.input_device)?;
            device_manager.select_device(DeviceType::Output, args.output_device)?;
            let _child;
            if let Some(exec) = args.exec {
                _child = process::Command::new(exec).spawn()?;
            }
            device_manager.run_process(args.speed)?;
            for f in extracted {
                fs::remove_file(f)?;
            }
        }
    }

    Ok(())
}

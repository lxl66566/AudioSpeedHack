pub mod asset;
pub mod cli;
pub mod constant;
pub mod device;
pub mod log;

use clap::Parser;

use crate::{
    cli::{Cli, Commands},
    device::{DeviceManager, DeviceType},
};

fn main() -> anyhow::Result<()> {
    log::log_init();

    let cli = Cli::parse();

    match cli.command {
        Commands::ListDevices => {
            DeviceManager::default().list_all_devices()?;
        }
        Commands::PreRegister => {
            todo!()
        }
        Commands::UnpackDll(args) => {
            todo!()
        }
        Commands::Start(args) => {
            let mut device_manager = DeviceManager::default();
            device_manager.select_device_tui(DeviceType::Input)?;
            device_manager.select_device_tui(DeviceType::Output)?;
            device_manager.run_process()?;
        }
        Commands::UnpackAndStart(args) => {
            todo!()
        }
    }

    Ok(())
}

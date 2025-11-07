pub mod asset;
pub mod cache;
pub mod cli;
pub mod device;
pub mod log;
pub mod reg;
pub mod tui;
pub mod utils;

use std::{env, process};

use ::log::info;
use anyhow::Result;
use clap::Parser;
use config_file2::Storable;

use crate::{
    cache::GLOBAL_CACHE,
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
    // let my_dll_path = r#"MMDevAPI.dll"#;
    // let add_operation = reg::RegistryOperation::Add {
    //     dll_path: my_dll_path,
    // };

    // if let Err(e) = reg::mmdevapi_registry_op(&add_operation) {
    //     eprintln!("添加注册表项时发生错误: {}", e);
    //     eprintln!("请检查路径是否正确以及是否具有相应的权限。");
    // }
    // std::process::exit(0);
    log::log_init();

    let cli = if std::env::args().len() > 1 {
        Cli::parse()
    } else {
        info!("TUI mode");
        tui::run_tui()?
    };

    info!("args: {:?}", cli);

    GLOBAL_CACHE
        .lock()
        .unwrap()
        .store_last_command(cli.command.clone())?;

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
            let extracted =
                asset::extract_selected_and_reg(args.dll, system, args.speed, env::current_dir()?)?;
            GLOBAL_CACHE.lock().unwrap().extend_dlls(extracted)?;
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
            clean()?;
        }
        Commands::UnpackAndStart(args) => {
            let system = if args.win64 {
                utils::System::Win64
            } else {
                utils::System::Win32
            };
            let extracted =
                asset::extract_selected_and_reg(args.dll, system, args.speed, env::current_dir()?)?;
            GLOBAL_CACHE.lock().unwrap().extend_dlls(extracted)?;

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
            clean()?;
        }
        Commands::Clean => {
            clean()?;
            GLOBAL_CACHE.lock().unwrap().clean_self()?;
        }
    }

    Ok(())
}

fn clean() -> Result<()> {
    let _pause_guard = PauseGuard::new("按任意键退出...");
    GLOBAL_CACHE.lock().unwrap().clean_dlls()?;
    GLOBAL_CACHE.lock().unwrap().clean_regs()?;
    GLOBAL_CACHE.lock().unwrap().store()?;
    Ok(())
}

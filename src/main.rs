pub mod asset;
pub mod cache;
pub mod cli;
pub mod log;
pub mod reg;
pub mod tui;
pub mod utils;

use std::env;

use ::log::{error, info};
use anyhow::Result;
use clap::Parser;
use config_file2::Storable;

use crate::{
    cache::GLOBAL_CACHE,
    cli::{Cli, Commands},
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

    let _pause_guard = PauseGuard::new("按任意键关闭窗口...");
    match cli.command {
        Commands::UnpackDll(args) => {
            let exec_arch = args.exec.as_ref().map(|exec_ref| {
                utils::System::detect(exec_ref).unwrap_or_else(|e| {
                    error!("自动检测架构失败: {e:?}，回退到 x64");
                    utils::System::X64
                })
            });
            let extracted = asset::extract_selected_and_reg(
                args.dll,
                exec_arch.unwrap_or(args.x86.into()),
                args.speed,
                env::current_dir()?,
            )?;
            GLOBAL_CACHE.lock().unwrap().extend_dlls(extracted)?;
            drop(PauseGuard::new(
                "按任意键回滚变更，并退出程序。请在关闭游戏后退出。",
            ));
            clean()?;
        }
        Commands::Clean => {
            clean()?;
            GLOBAL_CACHE.lock().unwrap().clean_self()?;
        }
        Commands::Detect(arg) => {
            let arch_res = utils::System::detect(&arg.exe);
            match arch_res {
                Ok(arch) => println!("检测到架构为 {:?}", arch),
                Err(e) => println!("检测失败：{:?}", e),
            }
        }
    }

    Ok(())
}

fn clean() -> Result<()> {
    GLOBAL_CACHE.lock().unwrap().clean_dlls()?;
    GLOBAL_CACHE.lock().unwrap().clean_regs()?;
    Ok(())
}

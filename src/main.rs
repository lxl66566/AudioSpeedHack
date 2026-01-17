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

use crate::{
    asset::AssetOperations,
    cache::GLOBAL_CACHE,
    cli::{Cli, Commands},
    reg::RegOperations,
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

    GLOBAL_CACHE
        .lock()
        .unwrap()
        .store_last_command(cli.command.clone())?;

    let _pause_guard = PauseGuard::new("按 Enter 关闭窗口...");
    match cli.command {
        Commands::UnpackDll(args) => {
            let exec_arch = args.exec.as_ref().map(|exec_ref| {
                utils::System::detect(exec_ref).unwrap_or_else(|e| {
                    error!("自动检测架构失败: {e:?}，回退到 x64");
                    utils::System::X64
                })
            });

            let extracted = args
                .dll
                .extract_dlls(exec_arch.unwrap_or(args.x86.into()), env::current_dir()?)?;
            args.dll.set_reg()?;
            if let Some(speed) = args.speed {
                args.dll.set_env(speed)?;
            }

            {
                let mut lock = GLOBAL_CACHE.lock().unwrap();
                lock.extend_dlls(extracted)?;
                lock.extend_env_vars(args.dll.envs())?;
            }

            drop(PauseGuard::new(
                "DLL 解压成功，请自行启动游戏。游玩结束后，按 Enter 回滚变更，并退出程序。",
            ));
            clean()?;
        }
        Commands::Clean => {
            clean()?;
            GLOBAL_CACHE.lock().unwrap().clean_self()?;
        }
        Commands::Detect(arg) => {
            utils::System::detect(&arg.exe)?;
        }
    }

    Ok(())
}

fn clean() -> Result<()> {
    GLOBAL_CACHE.lock().unwrap().clean_regs()?;
    GLOBAL_CACHE.lock().unwrap().clean_dlls()?;
    GLOBAL_CACHE.lock().unwrap().clean_envs()?;
    Ok(())
}

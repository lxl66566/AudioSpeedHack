use std::{
    fs,
    path::{Path, PathBuf},
    sync::{LazyLock as Lazy, Mutex},
};

use anyhow::Result;
use config_file2::{LoadConfigFile, Storable};
use log::{error, info, warn};
use serde::{Deserialize, Serialize};

use crate::{
    cli::{Commands, UnpackDllArgs},
    reg,
};

const DEFAULT_CACHE_PATH: &str = "cache.toml";
pub static GLOBAL_CACHE: Lazy<Mutex<Cache>> = Lazy::new(|| {
    Cache::load_or_default(DEFAULT_CACHE_PATH)
        .unwrap_or_else(|e| {
            error!("Failed to load cache: {e:?}, use default.");
            Cache::default()
        })
        .into()
});

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct Cache {
    #[serde(skip_serializing_if = "Option::is_none", default)]
    pub last_command: Option<Commands>,
    #[serde(skip_serializing_if = "Option::is_none")]
    dll_paths: Option<Vec<PathBuf>>,
}

impl Storable for Cache {
    fn path(&self) -> impl AsRef<Path> {
        Path::new(DEFAULT_CACHE_PATH)
    }
}

impl Cache {
    /// 清理所有 cache 中的 dll 文件，并避免在遇到错误时出现不一致状态
    pub fn clean_dlls(&mut self) -> Result<()> {
        let mut dlls = self.dll_paths.take().unwrap_or_default();
        dlls.push("SPEEDUP_announcement.txt".into());
        let mut process_result = Ok(());
        while let Some(dll) = dlls.pop() {
            if let Err(e) = std::fs::remove_file(&dll) {
                if e.kind() == std::io::ErrorKind::NotFound {
                    warn!("文件 {dll:?} 不存在，跳过删除");
                    continue;
                }
                dlls.push(dll);
                process_result = Err(e.into());
                break;
            }
            info!("成功删除文件：{dll:?}");
        }
        self.dll_paths = Some(dlls);
        self.store()?;
        process_result
    }

    /// 清理注册表项，注册表从 last command 里获取
    pub fn clean_regs(&mut self) -> Result<()> {
        reg::registry_op(
            &reg::RegistryOperation::Delete,
            self.last_command.as_ref().and_then(|cmd| match cmd {
                Commands::UnpackDll(UnpackDllArgs { dll: res, .. }) => *res,
                _ => None,
            }),
        )?;
        Ok(())
    }

    pub fn extend_dlls(&mut self, newdlls: Vec<PathBuf>) -> Result<()> {
        match self.dll_paths.as_mut() {
            Some(dlls) => dlls.extend(newdlls),
            None => self.dll_paths = Some(newdlls),
        }
        self.store()?;
        Ok(())
    }

    pub fn store_last_command(&mut self, cmd: Commands) -> Result<()> {
        // 如果是 clean，则不存储
        if matches!(cmd, Commands::Clean) {
            return Ok(());
        }
        self.last_command = Some(cmd);
        self.store()?;
        Ok(())
    }

    pub fn clean_self(&mut self) -> Result<()> {
        fs::remove_file(DEFAULT_CACHE_PATH)?;
        Ok(())
    }
}

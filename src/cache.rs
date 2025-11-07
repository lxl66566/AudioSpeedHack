use std::{
    fs,
    path::{Path, PathBuf},
    sync::{LazyLock as Lazy, Mutex},
};

use anyhow::Result;
use config_file2::{LoadConfigFile, Storable};
use log::{info, warn};
use serde::{Deserialize, Serialize};

use crate::{cli::Commands, reg};

const DEFAULT_CACHE_PATH: &str = "cache.toml";
pub static GLOBAL_CACHE: Lazy<Mutex<Cache>> = Lazy::new(|| {
    Cache::load_or_default(DEFAULT_CACHE_PATH)
        .expect("读取缓存文件失败")
        .into()
});

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct Cache {
    #[serde(skip_serializing_if = "Option::is_none", default)]
    last_command: Option<Commands>,
    #[serde(skip_serializing_if = "Option::is_none")]
    dlls: Option<Vec<PathBuf>>,
}

impl Storable for Cache {
    fn path(&self) -> impl AsRef<Path> {
        Path::new(DEFAULT_CACHE_PATH)
    }
}

impl Cache {
    pub fn clean_dlls(&mut self) -> Result<()> {
        if let Some(dlls) = self.dlls.as_mut() {
            for dll in dlls.iter() {
                if let Err(e) = std::fs::remove_file(dll) {
                    if e.kind() == std::io::ErrorKind::NotFound {
                        warn!("文件 {dll:?} 不存在，跳过删除");
                        continue;
                    }
                    return Err(e.into());
                }
                info!("成功删除文件：{dll:?}");
            }
            dlls.clear();
            self.store()?;
        }
        Ok(())
    }

    pub fn clean_regs(&mut self) -> Result<()> {
        reg::mmdevapi_registry_op(&reg::RegistryOperation::Delete)?;
        Ok(())
    }

    pub fn extend_dlls(&mut self, newdlls: Vec<PathBuf>) -> Result<()> {
        match self.dlls.as_mut() {
            Some(dlls) => dlls.extend(newdlls),
            None => self.dlls = Some(newdlls),
        }
        self.store()?;
        Ok(())
    }

    pub fn store_last_command(&mut self, cmd: Commands) -> Result<()> {
        self.last_command = Some(cmd);
        self.store()?;
        Ok(())
    }

    pub fn clean_self(&mut self) -> Result<()> {
        fs::remove_file(DEFAULT_CACHE_PATH)?;
        Ok(())
    }
}

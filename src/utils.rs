use std::{fs, path::Path};

use anyhow::Result;
use clap::ValueEnum;
use goblin::pe::PE;
use log::info;
use serde::{Deserialize, Serialize};
use strum_macros::{Display, EnumIter, EnumString};

pub const DSOUND_DLL_NAME: &str = "dsound.dll";
pub const MMDEVAPI_DLL_NAME: &str = "MMDevAPI.dll";

#[derive(Debug, Clone, Copy, Display, ValueEnum, Serialize, Deserialize, EnumString, EnumIter)]
#[strum(serialize_all = "lowercase")]
#[clap(rename_all = "lowercase")]
pub enum SupportedDLLs {
    DSound,
    MMDevAPI,
}

#[derive(Debug, Clone, Copy, Display, EnumString)]
#[strum(serialize_all = "lowercase")]
pub enum System {
    X64,
    X86,
}

impl From<bool> for System {
    /// from arg bool: default is x64, if true, then x86
    fn from(value: bool) -> Self {
        if value { System::X86 } else { System::X64 }
    }
}

impl System {
    pub fn detect(path: impl AsRef<Path>) -> Result<Self> {
        let buf = fs::read(path)?;
        let res = PE::parse(&buf)?;
        if res.is_64 {
            info!("Detected x64 PE");
            Ok(Self::X64)
        } else {
            info!("Detected x86 PE");
            Ok(Self::X86)
        }
    }
}

pub trait AudioExt {
    fn to_pitch(&self) -> f32;
}

impl AudioExt for f32 {
    #[inline]
    fn to_pitch(&self) -> f32 {
        -12.0 * self.log2()
    }
}

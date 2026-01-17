use std::{fs, path::Path};

use anyhow::Result;
use clap::ValueEnum;
use goblin::pe::PE;
use log::info;
use serde::{Deserialize, Serialize};
use strum_macros::{Display, EnumIter, EnumString};

pub const SPEED_MAX: f32 = 2.0;

pub const SOUNDTOUCH_DLL_NAME: &str = "SoundTouch.dll";
pub const ONNXRUNTIME_DLL_NAME: &str = "onnxruntime.dll";
pub const DSOUND_DLL_NAME: &str = "dsound.dll";
pub const MMDEVAPI_DLL_NAME: &str = "MMDevAPI.dll";
pub const SPEEDUP_ENV_NAME: &str = "SPEEDUP";

#[derive(Debug, Clone, Copy, Display, ValueEnum, Serialize, Deserialize, EnumString, EnumIter)]
#[strum(serialize_all = "lowercase")]
#[clap(rename_all = "lowercase")]
pub enum SupportedDLLs {
    DSound,
    MMDevAPI,
    ALL,
    DSoundZeroInterrupt,
}

impl SupportedDLLs {
    pub fn envs(&self) -> Vec<String> {
        match self {
            SupportedDLLs::DSound | SupportedDLLs::MMDevAPI | SupportedDLLs::ALL => {
                vec![SPEEDUP_ENV_NAME.to_string()]
            }
            SupportedDLLs::DSoundZeroInterrupt => vec![],
        }
    }

    pub fn set_env(&self, speed: f32) -> Result<()> {
        match self {
            SupportedDLLs::DSound | SupportedDLLs::MMDevAPI | SupportedDLLs::ALL => {
                windows_env::set(SPEEDUP_ENV_NAME, format!("{:.1}", speed))?;
                info!("env SPEEDUP set to {speed:.1}");
            }
            _ => {}
        }
        Ok(())
    }
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

use clap::ValueEnum;
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
    Win32,
    Win64,
}

impl System {
    pub fn to_arch(&self) -> &'static str {
        match self {
            System::Win32 => "x86",
            System::Win64 => "x64",
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

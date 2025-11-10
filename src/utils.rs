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
    X64,
    X86,
}

impl From<bool> for System {
    /// from arg bool: default is x64, if true, then x86
    fn from(value: bool) -> Self {
        if value { System::X86 } else { System::X64 }
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

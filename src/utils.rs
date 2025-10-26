use strum_macros::{Display, EnumString};

#[derive(Debug, Clone, Copy, Display, EnumString)]
#[strum(serialize_all = "lowercase")]
pub enum System {
    Win32,
    Win64,
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

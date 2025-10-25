use strum_macros::{Display, EnumString};

pub const PITCH_SHIFT_SEMITONES: f32 = -12.0; // 降低一个八度 (12个半音)
pub const OVERSAMPLING: usize = 16; // pitch_shifter 的处理质量，值越高，质量越好，CPU占用越高
pub const WINDOW_DURATION_MS: usize = 18; // pitch_shifter 的窗口时长，18ms 是一个不错的默认值。过小或者过大都会导致声音失真。
pub const BUFFER_LATENCY_MS: u64 = 100; // 我们的环形缓冲区的延迟，用于平滑输入和输出

#[derive(Debug, Clone, Copy, Display, EnumString)]
#[strum(serialize_all = "lowercase")]
pub enum System {
    Win32,
    Win64,
}

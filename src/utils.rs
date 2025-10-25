pub trait AudioExt {
    fn to_pitch(&self) -> f32;
}

impl AudioExt for f32 {
    #[inline]
    fn to_pitch(&self) -> f32 {
        -12.0 * self.log2()
    }
}

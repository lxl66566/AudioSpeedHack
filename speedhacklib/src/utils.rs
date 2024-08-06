/// Convert UTF-8 str to PCWSTR
macro_rules! w {
    ($x: expr) => {
        PCWSTR::from_raw(HSTRING::from($x).as_ptr())
    };
}
pub(crate) use w;

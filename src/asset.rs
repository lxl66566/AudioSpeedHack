use std::{
    fs,
    path::{Path, PathBuf},
};

use anyhow::Result;
use include_assets::{NamedArchive, include_dir};
use log::debug;

use crate::utils;

/// 提取选择的资源文件到指定目录
///
/// # Return
///
/// 创建的文件路径
pub fn extract_selected_assets(
    system: utils::System,
    speed: f32,
    dest: impl AsRef<Path>,
) -> Result<Vec<PathBuf>> {
    assert!((1.0..=2.5).contains(&speed));

    #[cfg(not(debug_assertions))]
    let archive = NamedArchive::load(include_dir!("assets", compression = "zstd", level = 22));
    #[cfg(debug_assertions)]
    let archive = NamedArchive::load(include_dir!("assets"));

    let aldrv_bytes = archive
        .get(format!("dsoal-aldrv-{}.dll", system).as_str())
        .unwrap();
    let dsound_bytes = archive
        .get(format!("dsound-{}-{:.1}.dll", system, speed).as_str())
        .unwrap();

    let aldrv_dest = dest.as_ref().join("dsoal-aldrv.dll");
    let dsound_dest = dest.as_ref().join("dsound.dll");
    let mut ret = vec![];

    if !aldrv_dest.exists() {
        ret.push(aldrv_dest.clone());
    }
    fs::write(aldrv_dest, aldrv_bytes)?;
    debug!("Extracted dsoal-aldrv.dll");

    if !dsound_dest.exists() {
        ret.push(dsound_dest.clone());
    }
    fs::write(dsound_dest, dsound_bytes)?;
    debug!("Extracted dsound.dll");
    Ok(ret)
}

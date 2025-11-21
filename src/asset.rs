use std::{
    fs,
    path::{Path, PathBuf},
};

use anyhow::Result;
use include_assets::{NamedArchive, include_dir};
use log::info;
use strum::IntoEnumIterator;

use crate::{
    reg,
    utils::{self, DSOUND_DLL_NAME, MMDEVAPI_DLL_NAME, SOUNDTOUCH_DLL_NAME, SupportedDLLs},
};

pub fn extract_soundtouch_assets(
    system: utils::System,
    dest: impl AsRef<Path>,
) -> Result<Vec<PathBuf>> {
    #[cfg(not(debug_assertions))]
    let st_archive = NamedArchive::load(include_dir!(
        "assets/SoundTouch",
        compression = "zstd",
        level = 22
    ));
    #[cfg(debug_assertions)]
    let st_archive = NamedArchive::load(include_dir!("assets/SoundTouch"));

    let st_bytes = st_archive
        .get(format!("SoundTouch-{}.dll", system).as_str())
        .unwrap();
    let st_dest = dest.as_ref().join(SOUNDTOUCH_DLL_NAME);

    let mut ret = vec![];
    if !st_dest.exists() {
        ret.push(st_dest.clone());
    }
    fs::write(st_dest, st_bytes)?;
    info!("Extracted {SOUNDTOUCH_DLL_NAME}");
    Ok(ret)
}

/// 提取选择的 dsound dll 到指定目录
///
/// # Return
///
/// 创建的文件路径
pub fn extract_dsound_assets(
    system: utils::System,
    speed: f32,
    dest: impl AsRef<Path>,
) -> Result<Vec<PathBuf>> {
    assert!((1.0..=2.5).contains(&speed));

    #[cfg(not(debug_assertions))]
    let dsound_archive = NamedArchive::load(include_dir!(
        "assets/dsound",
        compression = "zstd",
        level = 22
    ));
    #[cfg(debug_assertions)]
    let dsound_archive = NamedArchive::load(include_dir!("assets/dsound"));

    let dsound_bytes: &[u8] = dsound_archive
        .get(format!("dsound-{}-{:.1}.dll", system, speed).as_str())
        .unwrap();

    let dsound_dest = dest.as_ref().join(DSOUND_DLL_NAME);
    let mut ret = vec![];
    if !dsound_dest.exists() {
        ret.push(dsound_dest.clone());
    }
    fs::write(dsound_dest, dsound_bytes)?;
    info!("Extracted {}", DSOUND_DLL_NAME);
    Ok(ret)
}

/// 提取选择的 MMDevAPI dll 到指定目录
///
/// # Return
///
/// 创建的文件路径
pub fn extract_mmdevapi_assets(
    system: utils::System,
    speed: f32,
    dest: impl AsRef<Path>,
) -> Result<Vec<PathBuf>> {
    assert!((1.0..=2.5).contains(&speed));

    #[cfg(not(debug_assertions))]
    let mm_archive = NamedArchive::load(include_dir!(
        "assets/MMDevAPI",
        compression = "zstd",
        level = 22
    ));
    #[cfg(debug_assertions)]
    let mm_archive = NamedArchive::load(include_dir!("assets/MMDevAPI"));

    let mm_bytes = mm_archive
        .get(format!("{}-{}-{:.1}.dll", "MMDevAPI", system, speed).as_str())
        .unwrap();
    let mm_dest = dest.as_ref().join(MMDEVAPI_DLL_NAME);
    let mut ret = vec![];

    if !mm_dest.exists() {
        ret.push(mm_dest.clone());
    }
    fs::write(mm_dest, mm_bytes)?;
    info!("Extracted {}", MMDEVAPI_DLL_NAME);
    Ok(ret)
}

fn extract_specific_dll_and_reg(
    dll: SupportedDLLs,
    system: utils::System,
    speed: f32,
    dest: impl AsRef<Path>,
) -> Result<Vec<PathBuf>> {
    let ret = match dll {
        SupportedDLLs::DSound => extract_dsound_assets(system, speed, dest)?,
        SupportedDLLs::MMDevAPI => {
            let tmp = extract_mmdevapi_assets(system, speed, dest)?;
            reg::registry_op(&reg::RegistryOperation::Add, Some(dll))?;
            tmp
        }
    };
    Ok(ret)
}

/// 提取指定的 dll 到指定目录，如果为空，则提取全部 dll
/// 如果该 dll 对应了需要写入的注册表，则写入注册表
///
/// # Returns
///
/// 成功则返回所有提取的 dll 的绝对路径
pub fn extract_selected_and_reg(
    selected: Option<SupportedDLLs>,
    system: utils::System,
    speed: f32,
    dest: impl AsRef<Path>,
) -> Result<Vec<PathBuf>> {
    let mut ret = extract_soundtouch_assets(system, &dest)?;
    if let Some(dll) = selected {
        ret.extend(extract_specific_dll_and_reg(dll, system, speed, dest)?);
    } else {
        for dll in SupportedDLLs::iter() {
            ret.extend(extract_specific_dll_and_reg(dll, system, speed, &dest)?);
        }
    }
    Ok(ret)
}

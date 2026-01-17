use std::{
    fs,
    path::{Path, PathBuf},
};

use anyhow::Result;
use include_assets::{NamedArchive, include_dir};
use log::info;

use crate::utils::{
        self, DSOUND_DLL_NAME, MMDEVAPI_DLL_NAME, ONNXRUNTIME_DLL_NAME, SOUNDTOUCH_DLL_NAME,
        SupportedDLLs,
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

pub fn extract_onnxruntime_assets(
    system: utils::System,
    dest: impl AsRef<Path>,
) -> Result<Vec<PathBuf>> {
    #[cfg(not(debug_assertions))]
    let st_archive = NamedArchive::load(include_dir!(
        "assets/onnxruntime",
        compression = "zstd",
        level = 22
    ));
    #[cfg(debug_assertions)]
    let st_archive = NamedArchive::load(include_dir!("assets/onnxruntime"));

    let st_bytes = st_archive
        .get(format!("onnxruntime-{}.dll", system).as_str())
        .unwrap();
    let st_dest = dest.as_ref().join(ONNXRUNTIME_DLL_NAME);

    let mut ret = vec![];
    if !st_dest.exists() {
        ret.push(st_dest.clone());
    }
    fs::write(st_dest, st_bytes)?;
    info!("Extracted {ONNXRUNTIME_DLL_NAME}");
    Ok(ret)
}

pub fn extract_model_assets(dest: impl AsRef<Path>) -> Result<Vec<PathBuf>> {
    #[cfg(not(debug_assertions))]
    let st_archive = NamedArchive::load(include_dir!(
        "assets/models",
        compression = "zstd",
        level = 22
    ));
    #[cfg(debug_assertions)]
    let st_archive = NamedArchive::load(include_dir!("assets/models"));

    let st_bytes = st_archive.get("silero_vad.onnx").unwrap();
    let st_dest = dest.as_ref().join("silero_vad.onnx");

    let mut ret = vec![];
    if !st_dest.exists() {
        ret.push(st_dest.clone());
    }
    fs::write(st_dest, st_bytes)?;
    info!("Extracted silero_vad.onnx");
    Ok(ret)
}

/// 提取选择的 dsound dll 到指定目录
///
/// # Return
///
/// 创建的文件路径
pub fn extract_dsound_assets(
    system: utils::System,
    dest: impl AsRef<Path>,
    zerointerrupt: bool,
) -> Result<Vec<PathBuf>> {
    #[cfg(not(debug_assertions))]
    let dsound_archive = NamedArchive::load(include_dir!(
        "assets/dsound",
        compression = "zstd",
        level = 22
    ));
    #[cfg(debug_assertions)]
    let dsound_archive = NamedArchive::load(include_dir!("assets/dsound"));

    let dsound_src = if !zerointerrupt {
        format!("dsound-{}.dll", system)
    } else {
        format!("dsound-zerointerrupt-{}.dll", system)
    };

    let dsound_bytes: &[u8] = dsound_archive.get(&dsound_src).unwrap();

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
    dest: impl AsRef<Path>,
) -> Result<Vec<PathBuf>> {
    #[cfg(not(debug_assertions))]
    let mm_archive = NamedArchive::load(include_dir!(
        "assets/MMDevAPI",
        compression = "zstd",
        level = 22
    ));
    #[cfg(debug_assertions)]
    let mm_archive = NamedArchive::load(include_dir!("assets/MMDevAPI"));

    let mm_bytes = mm_archive
        .get(format!("{}-{}.dll", "MMDevAPI", system).as_str())
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

pub trait AssetOperations {
    fn extract_dlls(&self, system: utils::System, dest: impl AsRef<Path>) -> Result<Vec<PathBuf>>;
}

impl AssetOperations for SupportedDLLs {
    fn extract_dlls(&self, system: utils::System, dest: impl AsRef<Path>) -> Result<Vec<PathBuf>> {
        let ret = match self {
            SupportedDLLs::DSound => vec![
                extract_soundtouch_assets(system, &dest)?,
                extract_dsound_assets(system, &dest, false)?,
            ],
            SupportedDLLs::MMDevAPI => vec![
                extract_soundtouch_assets(system, &dest)?,
                extract_mmdevapi_assets(system, &dest)?,
            ],
            SupportedDLLs::ALL => vec![
                extract_soundtouch_assets(system, &dest)?,
                extract_dsound_assets(system, &dest, false)?,
                extract_mmdevapi_assets(system, &dest)?,
            ],
            SupportedDLLs::DSoundZeroInterrupt => vec![
                extract_dsound_assets(system, &dest, true)?,
                extract_model_assets(&dest)?,
                extract_onnxruntime_assets(system, &dest)?,
            ],
        };
        Ok(ret.into_iter().flatten().collect())
    }
}

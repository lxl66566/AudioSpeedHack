use std::sync::LazyLock as Lazy;

use log::{info, warn};
use windows_registry_obj::{BaseKey, RegValueData, Registry, Result};

use crate::{
    asset::mmdevapi_stub_path,
    utils::{SupportedDLLs, System},
};

/// (键路径, ThreadingModel, 是否为 WOW6432Node 视图)
/// 注册表值指向转发桩的绝对路径：裸名在限制 DLL 搜索顺序的游戏进程内
/// 解析失败，导致引擎判定无音频设备而完全无声；两个视图分别引用对应
/// 架构的桩（64 位视图→x64，WOW6432Node→x86）。
const MMDEVAPI_REGISTRY_TABLE: [(&str, &str, bool); 8] = [
    (
        r"SOFTWARE\Classes\CLSID\{06CCA63E-9941-441B-B004-39F999ADA412}\InprocServer32",
        "both",
        false,
    ),
    (
        r"SOFTWARE\Classes\CLSID\{93C063B0-68CB-4DE7-B032-8F56C1D2E99D}\InprocServer32",
        "both",
        false,
    ),
    (
        r"SOFTWARE\Classes\CLSID\{BCDE0395-E52F-467C-8E3D-C4579291692E}\InprocServer32",
        "both",
        false,
    ),
    (
        r"SOFTWARE\Classes\CLSID\{E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}\InprocServer32",
        "free",
        false,
    ),
    (
        r"SOFTWARE\Classes\WOW6432Node\CLSID\{06CCA63E-9941-441B-B004-39F999ADA412}\InprocServer32",
        "both",
        true,
    ),
    (
        r"SOFTWARE\Classes\WOW6432Node\CLSID\{93C063B0-68CB-4DE7-B032-8F56C1D2E99D}\InprocServer32",
        "both",
        true,
    ),
    (
        r"SOFTWARE\Classes\WOW6432Node\CLSID\{BCDE0395-E52F-467C-8E3D-C4579291692E}\InprocServer32",
        "both",
        true,
    ),
    (
        r"SOFTWARE\Classes\WOW6432Node\CLSID\{E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}\InprocServer32",
        "free",
        true,
    ),
];

static MMDEVAPI_REGISTRY_ITEMS: Lazy<Vec<Registry<'static>>> = Lazy::new(|| {
    MMDEVAPI_REGISTRY_TABLE
        .iter()
        .map(|&(path, threading_model, wow)| {
            let system = if wow { System::X86 } else { System::X64 };
            BaseKey::CurrentUser.reg(path).with_values([
                (
                    "",
                    RegValueData::ExpandableString(
                        mmdevapi_stub_path(system)
                            .to_string_lossy()
                            .into_owned()
                            .into(),
                    ),
                ),
                (
                    "ThreadingModel",
                    RegValueData::String(threading_model.into()),
                ),
            ])
        })
        .collect()
});

fn reg_iter<'a>(which: SupportedDLLs) -> impl Iterator<Item = &'a Registry<'a>> {
    match which {
        SupportedDLLs::MMDevAPI | SupportedDLLs::ALL => MMDEVAPI_REGISTRY_ITEMS.iter(),
        _ => [].iter(),
    }
}

/// 写入指定 DLL 类型所需的注册表项
pub fn set_reg(which: SupportedDLLs) -> Result<()> {
    for item in reg_iter(which) {
        item.set()?;
        info!("registry created: {:?}", item.full_path());
    }
    Ok(())
}

/// 无条件清理全部 MMDevAPI 注册表项。
/// 键集合是编译期固定的，不依赖 cache 中的 last_command：
/// 该记录是单槽且会被任意命令覆盖，依赖它会让孤儿键永远无法回滚。
/// 删除整棵 {CLSID} 键（set 时隐式创建）而不是只删 InprocServer32，避免残留空壳键。
pub fn clean_reg() -> Result<()> {
    for item in MMDEVAPI_REGISTRY_ITEMS.iter() {
        let clsid = item.parent();
        if !clsid.exists() {
            continue;
        }
        match clsid.remove_registry() {
            Ok(()) => info!("registry removed: {:?}", clsid.full_path()),
            Err(e) => warn!("failed to remove registry {:?}: {e}", clsid.full_path()),
        }
    }
    Ok(())
}

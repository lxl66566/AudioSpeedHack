use std::{io, sync::LazyLock as Lazy};

use log::{self, info};
use windows_registry_obj::{BaseKey, RegValueData, Registry};

use crate::{
    asset::mmdevapi_stub_path,
    utils::{SupportedDLLs, System},
};

/// 定义要执行的顶级注册表操作。
pub enum RegistryOperation {
    Add,
    Delete,
}

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

/// 主函数，执行注册表的添加或删除操作。
fn registry_op(operation: &RegistryOperation, which: SupportedDLLs) -> io::Result<()> {
    match operation {
        RegistryOperation::Add => {
            for item in reg_iter(which) {
                item.set()?;
                info!("registry created: {:?}", item.full_path());
            }
        }
        RegistryOperation::Delete => {
            for item in reg_iter(which) {
                match item.remove_registry() {
                    Ok(_) => info!("registry removed: {:?}", item.full_path()),
                    Err(e) => log::warn!("failed to remove registry {:?}: {}", item.full_path(), e),
                };
            }
        }
    }

    Ok(())
}

pub trait RegOperations {
    fn set_reg(&self) -> io::Result<()>;
    fn clean_reg(&self) -> io::Result<()>;
}

impl RegOperations for SupportedDLLs {
    fn set_reg(&self) -> io::Result<()> {
        registry_op(&RegistryOperation::Add, *self)
    }

    fn clean_reg(&self) -> io::Result<()> {
        registry_op(&RegistryOperation::Delete, *self)
    }
}

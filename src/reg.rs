use std::{io, sync::LazyLock as Lazy};

use log::{self, info};
use windows_registry_obj::{BaseKey, RegValueData, Registry};

use crate::utils::{MMDEVAPI_DLL_NAME, SupportedDLLs};

/// 定义要执行的顶级注册表操作。
pub enum RegistryOperation {
    Add,
    Delete,
}

static MMDEVAPI_REGISTRY_ITEMS: Lazy<Vec<Registry<'static>>> = Lazy::new(|| {
    [
        BaseKey::CurrentUser
            .reg("SOFTWARE\\Classes\\CLSID\\{06CCA63E-9941-441B-B004-39F999ADA412}\\InprocServer32")
            .with_values([
                ("", RegValueData::ExpandableString(MMDEVAPI_DLL_NAME.into())),
                ("ThreadingModel", RegValueData::String("both".into())),
            ]),
        BaseKey::CurrentUser
            .reg("SOFTWARE\\Classes\\CLSID\\{93C063B0-68CB-4DE7-B032-8F56C1D2E99D}\\InprocServer32")
            .with_values([
                ("", RegValueData::ExpandableString(MMDEVAPI_DLL_NAME.into())),
                ("ThreadingModel", RegValueData::String("both".into())),
            ]),
        BaseKey::CurrentUser
            .reg("SOFTWARE\\Classes\\CLSID\\{BCDE0395-E52F-467C-8E3D-C4579291692E}\\InprocServer32")
            .with_values([
                ("", RegValueData::ExpandableString(MMDEVAPI_DLL_NAME.into())),
                ("ThreadingModel", RegValueData::String("both".into())),
            ]),
        BaseKey::CurrentUser
            .reg("SOFTWARE\\Classes\\CLSID\\{E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}\\InprocServer32")
            .with_values([
                ("", RegValueData::ExpandableString(MMDEVAPI_DLL_NAME.into())),
                ("ThreadingModel", RegValueData::String("free".into())),
            ]),

        // WOW6432Node Entries
        BaseKey::CurrentUser
            .reg("SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{06CCA63E-9941-441B-B004-39F999ADA412}\\InprocServer32")
            .with_values([
                ("", RegValueData::ExpandableString(MMDEVAPI_DLL_NAME.into())),
                ("ThreadingModel", RegValueData::String("both".into())),
            ]),
        BaseKey::CurrentUser
            .reg("SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{93C063B0-68CB-4DE7-B032-8F56C1D2E99D}\\InprocServer32")
            .with_values([
                ("", RegValueData::ExpandableString(MMDEVAPI_DLL_NAME.into())),
                ("ThreadingModel", RegValueData::String("both".into())),
            ]),
        BaseKey::CurrentUser
            .reg("SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{BCDE0395-E52F-467C-8E3D-C4579291692E}\\InprocServer32")
            .with_values([
                ("", RegValueData::ExpandableString(MMDEVAPI_DLL_NAME.into())),
                ("ThreadingModel", RegValueData::String("both".into())),
            ]),
        BaseKey::CurrentUser
            .reg("SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}\\InprocServer32")
            .with_values([
                ("", RegValueData::ExpandableString(MMDEVAPI_DLL_NAME.into())),
                ("ThreadingModel", RegValueData::String("free".into())),
            ]),
    ].to_vec()
});

fn reg_iter<'a>(which: Option<SupportedDLLs>) -> impl Iterator<Item = &'a Registry<'a>> {
    match which {
        Some(SupportedDLLs::MMDevAPI) => MMDEVAPI_REGISTRY_ITEMS.iter(),
        Some(_) => [].iter(),
        _ => MMDEVAPI_REGISTRY_ITEMS.iter(),
    }
}

/// 主函数，执行注册表的添加或删除操作。
pub fn registry_op(operation: &RegistryOperation, which: Option<SupportedDLLs>) -> io::Result<()> {
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

use std::io;

use log;
use winreg::{RegKey, RegValue, enums::*};

use crate::utils::MMDEVAPI_DLL_NAME;

// --- 结构定义区 (保持不变，具有良好的泛用性) ---

/// 代表一个具体的注册表值的数据和类型。
pub enum ValueData<'a> {
    String(&'a str),
    ExpandableString(&'a str),
    DWord(u32),
    QWord(u64),
}

/// 一个通用的结构，代表任何一个要设置的注册表值。
pub struct RegistryValue<'a> {
    pub key_path: &'static str,
    pub value_name: &'static str,
    pub data: ValueData<'a>,
}

/// 定义要执行的顶级注册表操作。
pub enum RegistryOperation {
    Add,
    Delete,
}

// --- 数据定义区 ---

/// 统一的列表，定义了所有需要添加或设置的注册表值。
/// 添加操作将按顺序执行。
const REGISTRY_ITEMS: &[RegistryValue] = &[
    // --- Standard 64-bit Entries ---
    // CLSID: {06CCA63E-9941-441B-B004-39F999ADA412}
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\CLSID\\{06CCA63E-9941-441B-B004-39F999ADA412}\\InprocServer32",
        value_name: "",
        data: ValueData::ExpandableString(MMDEVAPI_DLL_NAME),
    },
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\CLSID\\{06CCA63E-9941-441B-B004-39F999ADA412}\\InprocServer32",
        value_name: "ThreadingModel",
        data: ValueData::String("both"),
    },
    // ... 其他 14 个 RegistryValue 项 ...
    // CLSID: {93C063B0-68CB-4DE7-B032-8F56C1D2E99D}
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\CLSID\\{93C063B0-68CB-4DE7-B032-8F56C1D2E99D}\\InprocServer32",
        value_name: "",
        data: ValueData::ExpandableString(MMDEVAPI_DLL_NAME),
    },
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\CLSID\\{93C063B0-68CB-4DE7-B032-8F56C1D2E99D}\\InprocServer32",
        value_name: "ThreadingModel",
        data: ValueData::String("both"),
    },
    // CLSID: {BCDE0395-E52F-467C-8E3D-C4579291692E}
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\CLSID\\{BCDE0395-E52F-467C-8E3D-C4579291692E}\\InprocServer32",
        value_name: "",
        data: ValueData::ExpandableString(MMDEVAPI_DLL_NAME),
    },
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\CLSID\\{BCDE0395-E52F-467C-8E3D-C4579291692E}\\InprocServer32",
        value_name: "ThreadingModel",
        data: ValueData::String("both"),
    },
    // CLSID: {E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\CLSID\\{E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}\\InprocServer32",
        value_name: "",
        data: ValueData::ExpandableString(MMDEVAPI_DLL_NAME),
    },
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\CLSID\\{E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}\\InprocServer32",
        value_name: "ThreadingModel",
        data: ValueData::String("free"),
    },
    // --- WOW6432Node Entries ---
    // CLSID: {06CCA63E-9941-441B-B004-39F999ADA412}
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{06CCA63E-9941-441B-B004-39F999ADA412}\\InprocServer32",
        value_name: "",
        data: ValueData::ExpandableString(MMDEVAPI_DLL_NAME),
    },
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{06CCA63E-9941-441B-B004-39F999ADA412}\\InprocServer32",
        value_name: "ThreadingModel",
        data: ValueData::String("both"),
    },
    // CLSID: {93C063B0-68CB-4DE7-B032-8F56C1D2E99D}
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{93C063B0-68CB-4DE7-B032-8F56C1D2E99D}\\InprocServer32",
        value_name: "",
        data: ValueData::ExpandableString(MMDEVAPI_DLL_NAME),
    },
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{93C063B0-68CB-4DE7-B032-8F56C1D2E99D}\\InprocServer32",
        value_name: "ThreadingModel",
        data: ValueData::String("both"),
    },
    // CLSID: {BCDE0395-E52F-467C-8E3D-C4579291692E}
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{BCDE0395-E52F-467C-8E3D-C4579291692E}\\InprocServer32",
        value_name: "",
        data: ValueData::ExpandableString(MMDEVAPI_DLL_NAME),
    },
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{BCDE0395-E52F-467C-8E3D-C4579291692E}\\InprocServer32",
        value_name: "ThreadingModel",
        data: ValueData::String("both"),
    },
    // CLSID: {E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}\\InprocServer32",
        value_name: "",
        data: ValueData::ExpandableString(MMDEVAPI_DLL_NAME),
    },
    RegistryValue {
        key_path: "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}\\InprocServer32",
        value_name: "ThreadingModel",
        data: ValueData::String("free"),
    },
];

/// 明确定义需要删除的顶级键。这是最安全的方式。删除操作将按倒序执行。
const KEYS_TO_DELETE: &[&str] = &[
    "SOFTWARE\\Classes\\CLSID\\{06CCA63E-9941-441B-B004-39F999ADA412}",
    "SOFTWARE\\Classes\\CLSID\\{93C063B0-68CB-4DE7-B032-8F56C1D2E99D}",
    "SOFTWARE\\Classes\\CLSID\\{BCDE0395-E52F-467C-8E3D-C4579291692E}",
    "SOFTWARE\\Classes\\CLSID\\{E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}",
    "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{06CCA63E-9941-441B-B004-39F999ADA412}",
    "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{93C063B0-68CB-4DE7-B032-8F56C1D2E99D}",
    "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{BCDE0395-E52F-467C-8E3D-C4579291692E}",
    "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}",
];

/// 主函数，执行注册表的添加或删除操作。
pub fn registry_op(operation: &RegistryOperation) -> io::Result<()> {
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);

    match operation {
        RegistryOperation::Add => {
            // 添加逻辑保持不变，按顺序遍历并创建/设置所有项。
            for item in REGISTRY_ITEMS.iter() {
                let (key, _disposition) = hkcu.create_subkey(item.key_path)?;
                let value_display_name = if item.value_name.is_empty() {
                    "(默认)"
                } else {
                    item.value_name
                };
                log::info!(
                    "正在设置值: HKEY_CURRENT_USER\\{}\\[{}]",
                    item.key_path,
                    value_display_name
                );

                match item.data {
                    ValueData::String(s_val) => key.set_value(item.value_name, &s_val)?,
                    ValueData::ExpandableString(es_val) => {
                        let bytes: Vec<u8> = es_val
                            .encode_utf16()
                            .chain(std::iter::once(0))
                            .flat_map(u16::to_le_bytes)
                            .collect();
                        let reg_value = RegValue {
                            vtype: REG_EXPAND_SZ,
                            bytes,
                        };
                        key.set_raw_value(item.value_name, &reg_value)?;
                    }
                    ValueData::DWord(dw_val) => key.set_value(item.value_name, &dw_val)?,
                    ValueData::QWord(qw_val) => key.set_value(item.value_name, &qw_val)?,
                }
            }
        }
        RegistryOperation::Delete => {
            // 使用明确、安全的列表进行删除操作。
            // 倒序遍历以确保先删除子键（如果存在嵌套定义）。
            for key_path in KEYS_TO_DELETE.iter().rev() {
                match hkcu.delete_subkey_all(key_path) {
                    Ok(_) => {
                        log::info!("成功删除注册表项: HKEY_CURRENT_USER\\{}", key_path);
                    }
                    Err(e) if e.kind() == io::ErrorKind::NotFound => {
                        log::warn!("注册表项不存在，跳过删除: HKEY_CURRENT_USER\\{}", key_path);
                    }
                    Err(e) => {
                        log::error!("删除注册表项 HKEY_CURRENT_USER\\{} 时出错: {}", key_path, e);
                        return Err(e);
                    }
                }
            }
        }
    }

    Ok(())
}

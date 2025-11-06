use std::io;
use winreg::RegKey;
use winreg::RegValue;
use winreg::enums::*;

/// 定义要执行的注册表操作
pub enum RegistryOperation<'a> {
    /// 添加注册表项，需要提供 DLL 的新路径
    Add { dll_path: &'a str },
    /// 删除注册表项
    Delete,
}

struct KeyInfo<'a> {
    clsid: &'a str,
    threading_model: &'a str,
}

// 将所有需要修改的键集中管理
const KEYS_TO_MODIFY: &[KeyInfo] = &[
    KeyInfo {
        clsid: "{06CCA63E-9941-441B-B004-39F999ADA412}",
        threading_model: "both",
    },
    KeyInfo {
        clsid: "{93C063B0-68CB-4DE7-B032-8F56C1D2E99D}",
        threading_model: "both",
    },
    KeyInfo {
        clsid: "{BCDE0395-E52F-467C-8E3D-C4579291692E}",
        threading_model: "both",
    },
    KeyInfo {
        clsid: "{E2F7A62A-862B-40AE-BBC2-5C0CA9A5B7E1}",
        threading_model: "free",
    },
];

/// 主函数，用于执行注册表的添加或删除操作
///
/// # 参数
/// * `operation` - 指定是添加还是删除
///
/// # 返回
/// * `io::Result<()>` - 如果成功则返回 `Ok(())`，否则返回包含错误的 `Err`
pub fn mmdevapi_registry_op(operation: &RegistryOperation) -> io::Result<()> {
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);

    let clsid_base_key = hkcu.open_subkey_with_flags("SOFTWARE\\Classes\\CLSID", KEY_ALL_ACCESS)?;

    match operation {
        RegistryOperation::Add { dll_path } => {
            for key_info in KEYS_TO_MODIFY {
                let key_path = format!("{}\\InprocServer32", key_info.clsid);
                println!("  正在处理: {}", key_path);

                let (inproc_key, _disposition) = clsid_base_key.create_subkey(&key_path)?;

                let expanded_path = RegValue {
                    vtype: REG_EXPAND_SZ,
                    bytes: dll_path
                        .encode_utf16()
                        .chain(std::iter::once(0)) // 添加 null 终止符
                        .flat_map(|c| c.to_le_bytes())
                        .collect(),
                };
                inproc_key.set_raw_value("", &expanded_path)?;
                inproc_key.set_value("ThreadingModel", &key_info.threading_model)?;
            }
        }

        RegistryOperation::Delete => {
            for key_info in KEYS_TO_MODIFY {
                match clsid_base_key.delete_subkey_all(key_info.clsid) {
                    Ok(_) => {
                        log::info!("删除注册表项: {}", key_info.clsid);
                    }
                    Err(e) if e.kind() == io::ErrorKind::NotFound => {
                        log::info!("注册表项不存在，跳过删除: {}", key_info.clsid);
                    }
                    Err(e) => return Err(e),
                }
            }
        }
    }

    Ok(())
}

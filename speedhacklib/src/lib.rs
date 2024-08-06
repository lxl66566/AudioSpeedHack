pub mod utils;

use std::{
    error::Error,
    ffi::{c_int, CString},
    iter, mem,
    os::raw::c_void,
};

use retour::static_detour;
use utils::w;
use windows::{
    core::{HSTRING, PCSTR, PCWSTR},
    Win32::{
        Foundation::{BOOL, HANDLE, HWND},
        Media::Audio::{HWAVEOUT, WAVEHDR},
        System::{
            LibraryLoader::{GetModuleHandleW, GetProcAddress},
            SystemServices::{
                DLL_PROCESS_ATTACH, DLL_PROCESS_DETACH, DLL_THREAD_ATTACH, DLL_THREAD_DETACH,
            },
        },
    },
};

type Result<T> = std::result::Result<T, Box<dyn Error>>;

macro_rules! detour_fn_type {
    ($(($hooktype: ident => $($content: tt)*))*) => {
        static_detour! {
            $(static $hooktype: $($content)*;)*
        }
        $(type $hooktype = $($content)*;)*
    };
}

detour_fn_type!(
    (MessageBoxWHook => unsafe extern "system" fn(HWND, PCWSTR, PCWSTR, u32) -> c_int)
    (WaveOutWriteHook => unsafe extern "system" fn(HWAVEOUT, *mut WAVEHDR, u32) -> u32)
);

macro_rules! inject {
    ($module: expr, $symbol: expr, $hooktype: ident, $function: ident) => {
        if let Some(address) = get_module_symbol_address($module, $symbol) {
            let target: $hooktype = mem::transmute(address);
            // Initialize AND enable the detour (the 2nd parameter can also be a closure)
            $hooktype.initialize(target, $function)?.enable()?;
        }
    };
}

fn inject_all() -> Result<()> {
    unsafe {
        // Called when the DLL is attached to the process.
        // Retrieve an absolute address of `MessageBoxW`. This is required for
        // libraries due to the import address table. If `MessageBoxW` would be
        // provided directly as the target, it would only hook this DLL's
        // `MessageBoxW`. Using the method below an absolute address is retrieved
        // instead, detouring all invocations of `MessageBoxW` in the active process.
        inject!(
            "user32.dll",
            "MessageBoxW",
            MessageBoxWHook,
            messageboxw_detour
        );
    }
    Ok(())
}

/// Called whenever `MessageBoxW` is invoked in the process.
fn messageboxw_detour(hwnd: HWND, text: PCWSTR, _caption: PCWSTR, msgbox_style: u32) -> c_int {
    // Call the original `MessageBoxW`, but replace the caption
    let replaced_caption = w!("Detoured!");
    let text = w!("Detoured!");
    unsafe { MessageBoxWHook.call(hwnd, text, replaced_caption, msgbox_style) }
}

/// Returns a module symbol's absolute address.
fn get_module_symbol_address(module: &str, symbol: &str) -> Option<usize> {
    let module = module
        .encode_utf16()
        .chain(iter::once(0))
        .collect::<Vec<u16>>();
    let symbol = CString::new(symbol).unwrap();
    unsafe {
        let handle = GetModuleHandleW(PCWSTR(module.as_ptr() as _)).unwrap();
        GetProcAddress(handle, PCSTR(symbol.as_ptr() as _)).map(|func| func as usize)
    }
}

#[no_mangle]
unsafe extern "system" fn DllMain(_hinst: HANDLE, reason: u32, _reserved: *mut c_void) -> BOOL {
    match reason {
        DLL_PROCESS_ATTACH => {
            println!("attaching");
            inject_all().unwrap_or_else(|e| panic!("injection error: {e:?}"));
        }
        DLL_PROCESS_DETACH => {
            println!("detaching");
        }
        DLL_THREAD_ATTACH => {}
        DLL_THREAD_DETACH => {}
        _ => {}
    };
    BOOL::from(true)
}

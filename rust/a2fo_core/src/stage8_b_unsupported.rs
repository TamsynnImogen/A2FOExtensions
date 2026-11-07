//! Diagnostic and return policy for the D3D8 experimental backend's unsupported calls.
use core::ffi::{c_char, c_void};

type Fail = unsafe extern "C" fn(*mut c_void, *const c_char) -> i32;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage8_b_unsupported(
    operation: u32,
    context: *mut c_void,
    fail: Option<Fail>,
) -> i32 {
    let message: &[u8] = match operation {
        0 => b"SetCursorProperties unsupported in experimental backend\0",
        1 => b"SetCursorPosition unsupported in experimental backend\0",
        2 => b"ShowCursor unsupported in experimental backend\0",
        3 => b"CreateAdditionalSwapChain unsupported in experimental backend\0",
        4 => b"GetRasterStatus unsupported in experimental backend\0",
        5 => b"SetGammaRamp unsupported in experimental backend\0",
        6 => b"GetGammaRamp unsupported in experimental backend\0",
        7 => b"CreateVolumeTexture unsupported in experimental backend\0",
        8 => b"CreateCubeTexture unsupported in experimental backend\0",
        9 => b"GetInfo unsupported in experimental backend\0",
        10 => b"SetPaletteEntries unsupported in experimental backend\0",
        11 => b"GetPaletteEntries unsupported in experimental backend\0",
        12 => b"SetCurrentTexturePalette unsupported in experimental backend\0",
        13 => b"GetCurrentTexturePalette unsupported in experimental backend\0",
        14 => b"ProcessVertices unsupported in experimental backend\0",
        15 => b"CreatePixelShader unsupported in experimental backend\0",
        16 => b"DeletePixelShader unsupported in experimental backend\0",
        17 => b"SetPixelShaderConstant unsupported in experimental backend\0",
        18 => b"GetPixelShaderConstant unsupported in experimental backend\0",
        19 => b"GetPixelShaderFunction unsupported in experimental backend\0",
        20 => b"DrawRectPatch unsupported in experimental backend\0",
        21 => b"DrawTriPatch unsupported in experimental backend\0",
        22 => b"DeletePatch unsupported in experimental backend\0",
        _ => return 0x8876_086cu32 as i32,
    };
    let result = match fail {
        Some(fail) => unsafe { fail(context, message.as_ptr().cast::<c_char>()) },
        None => 0x8876_086cu32 as i32,
    };
    if operation == 2 { 0 } else { result }
}

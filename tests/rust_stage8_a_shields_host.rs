extern crate alloc;

#[path = "../rust/a2fo_core/src/stage8_a.rs"]
mod stage8_a;

use core::ffi::c_void;

unsafe extern "C" fn lower(_: *mut c_void, byte: u8) -> u8 {
    byte.to_ascii_lowercase()
}

unsafe extern "C" fn space(_: *mut c_void, byte: u8) -> u8 {
    u8::from(byte.is_ascii_whitespace())
}

fn local(contents: &[u8]) -> u8 {
    unsafe {
        stage8_a::a2fo_rs_stage8_shield_parse(
            contents.as_ptr(), contents.len(), Some(lower), Some(space), None,
            core::ptr::null_mut(),
        )
    }
}

fn parameter(value: &[u8]) -> u8 {
    unsafe {
        stage8_a::a2fo_rs_stage8_shield_parameter(
            value.as_ptr(), value.len(), Some(lower), Some(space),
            core::ptr::null_mut(),
        )
    }
}

#[test]
fn local_flags_follow_last_valid_line_and_keep_null_termination() {
    assert_eq!(local(b"alwaysShowShields=1\nalwaysShowShields=off\n"), 1);
    assert_eq!(local(b"// ignored\n alwaysShowShields = yes; // comment"), 2);
    assert_eq!(local(b"alwaysShowShields=0\nalwaysShowShields=bogus"), 1);
    assert_eq!(local(b"alwaysShowShields=1\0garbage"), 2);
    assert_eq!(local(b"#include \"parent.odf\"\n"), 0);
}

#[test]
fn parameter_db_numeric_mode_accepts_trailing_text() {
    assert_eq!(parameter(b"  +12suffix  "), 1);
    assert_eq!(parameter(b"-000tail"), 0);
    assert_eq!(parameter(b"0x10"), 0);
    assert_eq!(parameter(b" YES "), 1);
    assert_eq!(parameter(b"off"), 0);
}

//! Portable firearc configuration and inherited turret-order policy.
use alloc::vec::Vec;
use core::slice;

unsafe extern "C" {
    fn isspace(value: i32) -> i32;
}

fn crt_space(value: u8) -> bool {
    // The previous C++ parser passes an unsigned char to the active CRT.
    unsafe { isspace(i32::from(value)) != 0 }
}

fn identifier_character(value: u8) -> bool {
    value.is_ascii_alphanumeric() || value == b'_'
}

fn strip_c_comments(source: &[u8]) -> Vec<u8> {
    let mut output = Vec::with_capacity(source.len());
    let mut index = 0;
    let mut line_comment = false;
    let mut block_comment = false;
    while index < source.len() {
        let current = source[index];
        let next = source.get(index + 1).copied().unwrap_or(0);
        if line_comment {
            if current == b'\n' || current == b'\r' {
                line_comment = false;
                output.push(current);
            } else {
                output.push(b' ');
            }
        } else if block_comment {
            if current == b'*' && next == b'/' {
                output.extend_from_slice(b"  ");
                index += 1;
                block_comment = false;
            } else {
                output.push(if current == b'\n' || current == b'\r' {
                    current
                } else {
                    b' '
                });
            }
        } else if current == b'/' && next == b'/' {
            output.extend_from_slice(b"  ");
            index += 1;
            line_comment = true;
        } else if current == b'/' && next == b'*' {
            output.extend_from_slice(b"  ");
            index += 1;
            block_comment = true;
        } else {
            output.push(current);
        }
        index += 1;
    }
    output
}

fn firearc_status(source: &[u8], enabled: &mut u8) -> u32 {
    let stripped = strip_c_comments(source);
    let mut status = 0;
    let mut candidate = *enabled != 0;
    let mut begin = 0;
    while begin < stripped.len() {
        let semicolon = stripped[begin..].iter().position(|&v| v == b';');
        let end = semicolon.map_or(stripped.len(), |offset| begin + offset);
        let statement = &stripped[begin..end];
        if let Some(equals) = statement.iter().position(|&v| v == b'=') {
            let mut identifier_end = equals;
            while identifier_end != 0 && !identifier_character(statement[identifier_end - 1]) {
                identifier_end -= 1;
            }
            let mut identifier_begin = identifier_end;
            while identifier_begin != 0 && identifier_character(statement[identifier_begin - 1]) {
                identifier_begin -= 1;
            }
            if &statement[identifier_begin..identifier_end] == b"firearc" {
                let mut cursor = equals + 1;
                while cursor < statement.len() && crt_space(statement[cursor]) {
                    cursor += 1;
                }
                if cursor < statement.len()
                    && (statement[cursor] == b'0' || statement[cursor] == b'1')
                {
                    let parsed = statement[cursor] == b'1';
                    cursor += 1;
                    while cursor < statement.len() && crt_space(statement[cursor]) {
                        cursor += 1;
                    }
                    if cursor == statement.len() {
                        candidate = parsed;
                        status = 1;
                    } else {
                        status = 2;
                    }
                } else {
                    status = 2;
                }
            }
        }
        match semicolon {
            Some(_) => begin = end + 1,
            None => break,
        }
    }
    if status == 1 {
        *enabled = u8::from(candidate);
    }
    status
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_combat_parse_firearc(
    source: *const u8,
    length: usize,
    enabled: *mut u8,
) -> u32 {
    if enabled.is_null() || (source.is_null() && length != 0) {
        return 2;
    }
    let bytes = if length == 0 {
        &[][..]
    } else {
        unsafe { slice::from_raw_parts(source, length) }
    };
    firearc_status(bytes, unsafe { &mut *enabled })
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_combat_attack_target(
    command_id: u32,
    target_form: u32,
    target_handle: u32,
    target_exists: u8,
) -> u32 {
    if command_id == 6 && target_form == 2 && target_handle != 0 && target_exists != 0 {
        target_handle
    } else {
        0
    }
}

#[repr(C)]
pub struct OrderTargetState {
    ordered_target_handle: u32,
    visual_target_handle: u32,
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_combat_update_target(
    previous: u32,
    next: u32,
    current_visual: u32,
    output: *mut OrderTargetState,
) {
    if output.is_null() {
        return;
    }
    let visual = if next != 0 {
        next
    } else if previous != 0 && current_visual == previous {
        0
    } else {
        current_visual
    };
    unsafe {
        *output = OrderTargetState {
            ordered_target_handle: next,
            visual_target_handle: visual,
        }
    };
}

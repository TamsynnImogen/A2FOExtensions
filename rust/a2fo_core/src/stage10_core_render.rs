//! Renderer CPU control. Native callbacks retain checked engine fields, COM,
//! x87 kernels, native storage/exception ownership and exact API operations.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
fn op(s: Step, c: *mut c_void, id: u32, arg: usize) -> usize {
    unsafe { s(c, id, arg) }
}
/// # Safety
/// Live synchronous callback/context; native exceptions never cross this ABI.
/// Kinds: 0=general gate,1=mesh routing,2=compositor admission,3=DX9 tokens,
/// 4=source load mask,5=source merge dispatch,6=cache eviction,7=suffix parsing.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_core_render_flow(
    kind: u32,
    c: *mut c_void,
    s: Step,
    arg: usize,
) -> usize {
    match kind {
        0 => usize::from((0..5).all(|id| op(s, c, id, 0) != 0)),
        1 => {
            if op(s, c, 0, 0) == 0 || op(s, c, 1, 0) == 0 {
                return 0;
            }
            if op(s, c, 2, 0) != 0 {
                op(s, c, 3, 0);
            }
            if op(s, c, 4, 0) != 0 {
                op(s, c, 5, 0);
            }
            if arg != 4 && arg != 5 {
                if op(s, c, 7, 0) != 0 {
                    op(s, c, 8, 0);
                }
                return 0;
            }
            op(s, c, 9, 0);
            if op(s, c, 10, 0) != 0 {
                op(s, c, 11, 0);
            }
            op(s, c, 12, 0);
            if arg == 5 && op(s, c, 13, 0) != 0 && op(s, c, 14, 0) != 0 {
                op(s, c, 15, 0);
                return 0;
            }
            if op(s, c, 16, 0) == 0 {
                op(s, c, 17, 0);
                return 0;
            }
            let cloak = op(s, c, 18, 0) as u32 as i32;
            if arg == 4
                && op(s, c, 19, 0) != 0
                && op(s, c, 20, 0) != 0
                && !(1..=3).contains(&cloak)
                && op(s, c, 21, 0) == 0
            {
                return 0;
            }
            if op(s, c, 22, 0) == 0 || op(s, c, 23, 0) == 0 {
                return 0;
            }
            op(s, c, 24, 0);
            if op(s, c, 25, 0) == 0 || op(s, c, 26, 0) == 0 {
                return 0;
            }
            op(s, c, 27, usize::from(arg == 5));
            0
        }
        2 => {
            if op(s, c, 0, 0) == 0 {
                return 1;
            }
            if op(s, c, 1, 0) == 0 || op(s, c, 2, 0) != 0 || op(s, c, 3, 0) == 0 {
                return 0;
            }
            op(s, c, 4, 0);
            if op(s, c, 5, 0) == 0 {
                return 0;
            }
            op(s, c, 6, 0);
            let mut lights = 0;
            while op(s, c, 7, 0) != 0 {
                if op(s, c, 8, 0) == 0 || lights == arg {
                    op(s, c, 9, 0);
                    return 0;
                }
                lights += 1;
                op(s, c, 10, 0);
            }
            if op(s, c, 11, lights) == 0 {
                op(s, c, 12, 0);
                return 0;
            }
            op(s, c, 13, lights);
            1
        }
        3 => {
            let mut quoted = false;
            let mut index = 0;
            loop {
                let ch = op(s, c, 0, index);
                if ch == usize::MAX {
                    return 0;
                }
                if ch == 34 {
                    quoted = !quoted;
                } else if ch == 0 || (!quoted && (ch == 32 || ch == 9)) {
                    if op(s, c, 1, 0) != 0 {
                        match op(s, c, 2, 0) {
                            1 => return 1,
                            usize::MAX => return 0,
                            _ => {}
                        }
                    }
                    if op(s, c, 3, 0) == usize::MAX {
                        return 0;
                    }
                    if ch == 0 {
                        return 0;
                    }
                } else if op(s, c, 4, ch) == usize::MAX {
                    return 0;
                }
                index += 1;
            }
        }
        4 => {
            let mut mask = arg as u8;
            let count = op(s, c, 0, 0);
            for index in 0..count {
                let bit = 1u32.wrapping_shl(index as u32) as u8;
                if mask & bit != 0 && op(s, c, 1, index) == 0 {
                    mask &= !bit;
                }
            }
            mask as usize
        }
        5 => {
            let count = op(s, c, 0, 0);
            for index in 0..count {
                if arg & (1usize.wrapping_shl(index as u32)) == 0 {
                    continue;
                }
                op(s, c, 1, index);
                let dense = op(s, c, 2, 0) != 0;
                op(s, c, if dense { 3 } else { 4 }, 0);
            }
            0
        }
        6 => {
            while op(s, c, 0, 0) >= arg {
                if op(s, c, 1, 0) == 0 {
                    break;
                }
            }
            0
        }
        7 => {
            let mut index = 0;
            loop {
                let byte = op(s, c, 0, index);
                if byte == 0 {
                    return if index != 0 && op(s, c, 3, 0) != usize::MAX {
                        1
                    } else {
                        0
                    };
                }
                if byte == usize::MAX || index >= 32 {
                    return 0;
                }
                if op(s, c, 1, byte) == 0 && byte != 95 && byte != 45 {
                    return 0;
                }
                if op(s, c, 2, byte) == usize::MAX {
                    return 0;
                }
                index += 1;
            }
        }
        _ => 0,
    }
}
/// # Safety
/// Native writable UTF-16 words, disjoint from other active mutable access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_core_render_lower16(data: *mut u16, length: usize) {
    for index in 0..length {
        let p = unsafe { data.add(index) };
        let byte = unsafe { p.read() };
        if (65..=90).contains(&byte) {
            unsafe {
                p.write(byte + 32);
            }
        }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_core_render_separator(empty: u8, last: u8) -> u8 {
    u8::from(empty == 0 && last != b'\\' && last != b'/')
}
#[cfg(test)]
mod tests {
    use super::*;
    use std::vec::Vec;
    struct C {
        calls: Vec<(u32, usize)>,
        deny: u32,
        count: usize,
        text: Vec<u16>,
        token: Vec<u16>,
    }
    unsafe extern "C" fn step(p: *mut c_void, id: u32, arg: usize) -> usize {
        let c = unsafe { &mut *p.cast::<C>() };
        c.calls.push((id, arg));
        usize::from(id != c.deny)
    }
    fn context(deny: u32) -> C {
        C {
            calls: Vec::new(),
            deny,
            count: 0,
            text: Vec::new(),
            token: Vec::new(),
        }
    }
    #[test]
    fn gate_stops_at_first_unavailable_native_field() {
        let mut c = context(2);
        assert_eq!(
            unsafe { a2fo_rs_stage10_core_render_flow(0, (&mut c as *mut C).cast(), step, 0) },
            0
        );
        assert_eq!(c.calls, [(0, 0), (1, 0), (2, 0)]);
    }
    #[test]
    fn unsupported_mesh_never_reads_material() {
        let mut c = context(99);
        unsafe { a2fo_rs_stage10_core_render_flow(1, (&mut c as *mut C).cast(), step, 9) };
        assert_eq!(
            c.calls,
            [
                (0, 0),
                (1, 0),
                (2, 0),
                (3, 0),
                (4, 0),
                (5, 0),
                (7, 0),
                (8, 0)
            ]
        );
    }
    unsafe extern "C" fn light(p: *mut c_void, id: u32, arg: usize) -> usize {
        let c = unsafe { &mut *p.cast::<C>() };
        c.calls.push((id, arg));
        match id {
            0 | 1 | 3 | 5 | 7 | 8 => 1,
            2 => 0,
            10 => {
                c.count += 1;
                0
            }
            _ => 0,
        }
    }
    #[test]
    fn light_cycle_has_eight_advances_and_checks_ninth_readability() {
        let mut c = context(99);
        assert_eq!(
            unsafe { a2fo_rs_stage10_core_render_flow(2, (&mut c as *mut C).cast(), light, 8) },
            0
        );
        assert_eq!(c.count, 8);
        assert_eq!(c.calls.iter().filter(|&&(op, _)| op == 8).count(), 9);
        assert_eq!(c.calls.last(), Some(&(9, 0)));
    }
    unsafe extern "C" fn tokens(p: *mut c_void, id: u32, arg: usize) -> usize {
        let c = unsafe { &mut *p.cast::<C>() };
        match id {
            0 => c.text[arg] as usize,
            1 => usize::from(!c.token.is_empty()),
            2 => {
                for b in &mut c.token {
                    if (65..=90).contains(b) {
                        *b += 32;
                    }
                }
                usize::from(c.token == [47, 100, 51, 100, 57] || c.token == [45, 100, 51, 100, 57])
            }
            3 => {
                c.token.clear();
                0
            }
            4 => {
                c.token.push(arg as u16);
                0
            }
            _ => 0,
        }
    }
    #[test]
    fn command_quotes_whitespace_and_exact_token() {
        for (text, want) in [
            ("game /D3D9", 1),
            ("game \"-d3d9\"", 1),
            ("/d\"3d\"9", 1),
            ("game /d3d90", 0),
            ("\"a /d3d9\"", 0),
        ] {
            let mut c = context(99);
            c.text = text.encode_utf16().chain(core::iter::once(0)).collect();
            assert_eq!(
                unsafe {
                    a2fo_rs_stage10_core_render_flow(3, (&mut c as *mut C).cast(), tokens, 0)
                },
                want,
                "{text}"
            );
        }
    }
    #[test]
    fn utf16_ascii_only_and_separator_contract() {
        let mut text = [65, 90, 0, 0x104, 0xff80];
        unsafe { a2fo_rs_stage10_core_render_lower16(text.as_mut_ptr(), text.len()) };
        assert_eq!(text, [97, 122, 0, 0x104, 0xff80]);
        assert_eq!(a2fo_rs_stage10_core_render_separator(0, b'a'), 1);
        assert_eq!(a2fo_rs_stage10_core_render_separator(0, b'/'), 0);
        assert_eq!(a2fo_rs_stage10_core_render_separator(1, 0), 0);
    }
}

use core::ffi::c_void;

const UNWRITABLE_FLAGS: u64 = 0x1_0000_0000;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Stats {
    visible: u32,
    matches: u32,
    changed: u32,
    highest: u32,
    remaining: usize,
    truncated: u8,
}

pub type Step = unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> u64;

// Native guarantees a readable NUL-terminated name when non-null. Preserve
// the original short-circuit reads; no arbitrary length cap is introduced.
unsafe fn parse_name(name: *const u8) -> Option<u32> {
    if name.is_null() {
        return None;
    }
    let first = unsafe { name.read() };
    if first != b'o' && first != b'O' {
        return None;
    }
    let second = unsafe { name.add(1).read() };
    if second != b'q' && second != b'Q' {
        return None;
    }
    let mut cursor = unsafe { name.add(2) };
    let mut ch = unsafe { cursor.read() };
    if !ch.is_ascii_digit() {
        return None;
    }
    let mut value = 0u32;
    while ch.is_ascii_digit() {
        let digit = u32::from(ch - b'0');
        if value > (u32::MAX - digit) / 10 {
            return None;
        }
        value = value * 10 + digit;
        cursor = unsafe { cursor.add(1) };
        ch = unsafe { cursor.read() };
    }
    if ch != 0 || value == 0 {
        None
    } else {
        Some(value)
    }
}

/// # Safety
/// If nonnull, `name` must address a readable NUL-terminated string.
/// `output` must point to a writable `u32` when a name is accepted.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_quarters_parse(name: *const u8, output: *mut u32) -> u8 {
    if output.is_null() {
        return 0;
    }
    let Some(value) = (unsafe { parse_name(name) }) else {
        return 0;
    };
    unsafe { output.write(value) };
    1
}

unsafe fn walk_node(
    node: usize,
    depth: usize,
    max_depth: usize,
    hidden_flag: u32,
    context: *mut c_void,
    step: Step,
    stats: &mut Stats,
) {
    if node == 0 {
        return;
    }
    if depth > max_depth || stats.remaining == 0 {
        stats.truncated = 1;
        return;
    }
    stats.remaining -= 1;

    // The pointer returned by op 0 is consumed before any further callback.
    // Do not keep a slice into the native scratch buffer across callbacks.
    let index = {
        let name = unsafe { step(context, 0, node, 0) } as usize as *const u8;
        unsafe { parse_name(name) }
    };
    if let Some(index) = index {
        stats.matches = stats.matches.wrapping_add(1);
        stats.highest = stats.highest.max(index);
        let flags = unsafe { step(context, 1, node, 0) };
        if flags != UNWRITABLE_FLAGS {
            let flags = flags as u32;
            let updated = if index <= stats.visible {
                flags & !hidden_flag
            } else {
                flags | hidden_flag
            };
            if updated != flags {
                unsafe { step(context, 2, node, updated as usize) };
                stats.changed = stats.changed.wrapping_add(1);
            }
        }
    }

    let mut child = unsafe { step(context, 3, node, 0) } as usize;
    while child != 0 && stats.remaining != 0 {
        // Read the next pointer before recursing: child callbacks may mutate
        // its sibling link while processing that subtree.
        let next = unsafe { step(context, 4, child, 0) } as usize;
        unsafe {
            walk_node(
                child,
                depth.wrapping_add(1),
                max_depth,
                hidden_flag,
                context,
                step,
                stats,
            )
        };
        child = next;
    }
    if child != 0 {
        stats.truncated = 1;
    }
}

/// # Safety
/// For nonzero `node`, `stats` must point to a readable and writable `Stats`.
/// `context` and `step` must remain valid for synchronous traversal; each
/// returned name must be readable through its NUL before the next callback.
/// The callback validates native nodes and must not unwind or access `stats`
/// while this call uses its private copy.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_quarters_walk(
    node: usize,
    depth: usize,
    max_depth: usize,
    hidden_flag: u32,
    context: *mut c_void,
    step: Step,
    stats: *mut Stats,
) {
    if node == 0 || stats.is_null() {
        return;
    }
    // Do not hold a reference to foreign stats while native callbacks run.
    let mut local = unsafe { stats.read() };
    unsafe {
        walk_node(
            node,
            depth,
            max_depth,
            hidden_flag,
            context,
            step,
            &mut local,
        )
    };
    unsafe { stats.write(local) };
}

#[cfg(test)]
mod tests {
    use super::*;
    extern crate std;
    use self::std::vec::Vec;

    #[test]
    fn parser_matches_prefix_decimal_overflow_and_terminal_nul() {
        let mut result = 77;
        for (name, expected) in [
            (b"oq1\0".as_slice(), Some(1)),
            (b"OQ00042\0".as_slice(), Some(42)),
            (b"oq4294967295\0".as_slice(), Some(u32::MAX)),
            (b"oq0\0".as_slice(), None),
            (b"oq4294967296\0".as_slice(), None),
            (b"oq1x\0".as_slice(), None),
            (b"oq\0".as_slice(), None),
            (b"x\0".as_slice(), None),
        ] {
            result = 77;
            let admitted = unsafe { a2fo_rs_stage9_a1_quarters_parse(name.as_ptr(), &mut result) };
            assert_eq!(admitted, u8::from(expected.is_some()));
            assert_eq!(result, expected.unwrap_or(77));
        }
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_quarters_parse(core::ptr::null(), &mut result) },
            0
        );
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_quarters_parse(
                    c"oq1".as_ptr().cast::<u8>(),
                    core::ptr::null_mut(),
                )
            },
            0
        );
    }

    #[derive(Clone, Copy)]
    struct Node {
        name: &'static [u8],
        flags: u32,
        writable: bool,
        first: usize,
        next: usize,
    }
    struct Context {
        nodes: [Node; 4],
        scratch: [u8; 96],
        calls: Vec<(u32, usize)>,
        mutate_next_of_one: bool,
    }
    unsafe extern "C" fn step(
        opaque: *mut c_void,
        operation: u32,
        node: usize,
        argument: usize,
    ) -> u64 {
        let context = unsafe { &mut *opaque.cast::<Context>() };
        context.calls.push((operation, node));
        match operation {
            0 => {
                context.scratch.fill(0);
                let name = context.nodes[node].name;
                context.scratch[..name.len()].copy_from_slice(name);
                if node == 1 && context.mutate_next_of_one {
                    context.nodes[1].next = 3;
                }
                context.scratch.as_ptr() as usize as u64
            }
            1 => {
                context.scratch.fill(0); // Name must already be consumed.
                if context.nodes[node].writable {
                    u64::from(context.nodes[node].flags)
                } else {
                    UNWRITABLE_FLAGS
                }
            }
            2 => {
                context.nodes[node].flags = argument as u32;
                0
            }
            3 => context.nodes[node].first as u64,
            4 => context.nodes[node].next as u64,
            _ => unreachable!(),
        }
    }
    fn context() -> Context {
        Context {
            nodes: [
                Node {
                    name: b"\0",
                    flags: 0,
                    writable: false,
                    first: 0,
                    next: 0,
                },
                Node {
                    name: b"oq1\0",
                    flags: 1,
                    writable: true,
                    first: 2,
                    next: 0,
                },
                Node {
                    name: b"oq2\0",
                    flags: 0,
                    writable: true,
                    first: 0,
                    next: 3,
                },
                Node {
                    name: b"oq3\0",
                    flags: 0,
                    writable: false,
                    first: 0,
                    next: 0,
                },
            ],
            scratch: [0; 96],
            calls: Vec::new(),
            mutate_next_of_one: false,
        }
    }
    #[test]
    fn visible_bits_writable_gate_and_budget() {
        let mut c = context();
        let mut stats = Stats {
            visible: 1,
            matches: 0,
            changed: 0,
            highest: 0,
            remaining: 2,
            truncated: 0,
        };
        unsafe {
            a2fo_rs_stage9_a1_quarters_walk(
                1,
                0,
                256,
                1,
                (&mut c as *mut Context).cast(),
                step,
                &mut stats,
            )
        };
        assert_eq!(
            stats,
            Stats {
                visible: 1,
                matches: 2,
                changed: 2,
                highest: 2,
                remaining: 0,
                truncated: 1
            }
        );
        assert_eq!(c.nodes[1].flags, 0);
        assert_eq!(c.nodes[2].flags, 1);
        assert_eq!(c.nodes[3].flags, 0);
        assert_eq!(c.calls.iter().filter(|&&(op, _)| op == 0).count(), 2);
    }
    #[test]
    fn depth_limit_sets_truncated_without_consuming_child() {
        let mut c = context();
        let mut stats = Stats {
            visible: 1,
            matches: 0,
            changed: 0,
            highest: 0,
            remaining: 3,
            truncated: 0,
        };
        unsafe {
            a2fo_rs_stage9_a1_quarters_walk(
                1,
                0,
                0,
                1,
                (&mut c as *mut Context).cast(),
                step,
                &mut stats,
            )
        };
        assert_eq!(stats.remaining, 2);
        assert_eq!(stats.matches, 1);
        assert_eq!(stats.truncated, 1);
        assert!(!c.calls.contains(&(0, 2)));
    }
    #[test]
    fn next_sibling_is_read_before_child_callback_mutation() {
        let mut c = context();
        c.nodes[1].first = 0;
        c.nodes[1].next = 2;
        c.nodes[2].next = 0;
        c.nodes[3].first = 1;
        c.mutate_next_of_one = true;
        let mut stats = Stats {
            visible: 1,
            matches: 0,
            changed: 0,
            highest: 0,
            remaining: 3,
            truncated: 0,
        };
        unsafe {
            a2fo_rs_stage9_a1_quarters_walk(
                3,
                0,
                256,
                1,
                (&mut c as *mut Context).cast(),
                step,
                &mut stats,
            )
        };
        assert_eq!(stats.matches, 3);
        assert!(
            c.calls
                .windows(2)
                .any(|pair| pair[0] == (4, 1) && pair[1] == (0, 1))
        );
        assert!(c.calls.contains(&(0, 2)));
        assert_eq!(stats.remaining, 0);
    }
    #[test]
    fn counters_wrap_and_null_root_preserves_stats() {
        let mut c = context();
        let mut stats = Stats {
            visible: 1,
            matches: u32::MAX,
            changed: u32::MAX,
            highest: 0,
            remaining: 1,
            truncated: 0,
        };
        unsafe {
            a2fo_rs_stage9_a1_quarters_walk(
                1,
                0,
                256,
                1,
                (&mut c as *mut Context).cast(),
                step,
                &mut stats,
            )
        };
        assert_eq!(stats.matches, 0);
        assert_eq!(stats.changed, 0);
        let saved = stats;
        unsafe {
            a2fo_rs_stage9_a1_quarters_walk(
                0,
                0,
                256,
                1,
                (&mut c as *mut Context).cast(),
                step,
                &mut stats,
            )
        };
        assert_eq!(stats, saved);
    }
}

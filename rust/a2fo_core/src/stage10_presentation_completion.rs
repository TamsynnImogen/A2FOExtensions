//! Small remaining presentation and live-object traversals. Native callbacks
//! retain iterators, checked engine access, strings, exceptions and API calls.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, u64) -> u64;

struct Flow {
    context: *mut c_void,
    step: Step,
    failed: *const u8,
}
impl Flow {
    fn call(&self, id: u32, arg: u64) -> Option<u64> {
        let value = unsafe { (self.step)(self.context, id, arg) };
        (unsafe { self.failed.read() } == 0).then_some(value)
    }
    fn run(&self, kind: u32, arg: u64) -> Option<u64> {
        match kind {
            // Native cursor operations: end, predicate, action, increment.
            0..=3 => {
                let mut count = 0;
                while self.call(0, 0)? != 0 {
                    let selected = kind == 0 || self.call(1, 0)? != 0;
                    if selected {
                        if kind == 1 {
                            return Some(1);
                        }
                        if kind != 2 {
                            self.call(2, 0)?;
                        }
                        count += 1;
                    }
                    self.call(3, 0)?;
                }
                Some(count)
            }
            // Checked C string: next read precedes NUL test and native append.
            4 => {
                for index in 0..arg {
                    if self.call(0, index)? == 0 {
                        return Some(0);
                    }
                    if self.call(1, 0)? != 0 {
                        return Some(1);
                    }
                    self.call(2, 0)?;
                }
                Some(0)
            }
            // Loading text: drop CR and empty lines, clear after moved strings.
            5 => {
                let mut index = 0;
                loop {
                    let ch = self.call(0, index)?;
                    if ch == 0 || ch == 10 {
                        if self.call(1, 0)? != 0 {
                            self.call(2, 0)?;
                        }
                        self.call(3, 0)?;
                        // Native push/clear may mutate the input through an
                        // allocator callback. Preserve the original reread.
                        if self.call(0, index)? == 0 {
                            return Some(0);
                        }
                    } else if ch != 13 {
                        self.call(4, ch)?;
                    }
                    index += 1;
                }
            }
            // Live object list: snapshot the successor before invoking visitor.
            6 => {
                if self.call(0, 0)? == 0 || self.call(1, 0)? == 0 {
                    return Some(0);
                }
                self.call(2, 0)?;
                let count = self.call(3, 0)?;
                if self.call(4, 0)? == 0 || count > arg {
                    return Some(0);
                }
                self.call(6, 0)?;
                let mut index = 0;
                while self.call(7, 0)? != 0 && self.call(8, 0)? != 0 && index < count {
                    self.call(10, 0)?;
                    self.call(11, 0)?;
                    if self.call(12, 0)? != 0 && self.call(13, 0)? == 0 {
                        self.call(14, 0)?;
                    }
                    if self.call(15, 0)? != 0 {
                        break;
                    }
                    self.call(16, 0)?;
                    index += 1;
                }
                Some(0)
            }
            // std::remove_if ordering: first match, then stable native moves.
            7 => {
                loop {
                    if self.call(0, 0)? == 0 {
                        return Some(0);
                    }
                    if self.call(1, 0)? != 0 {
                        break;
                    }
                    self.call(3, 0)?;
                }
                self.call(4, 0)?;
                self.call(3, 0)?;
                while self.call(0, 0)? != 0 {
                    if self.call(1, 0)? == 0 {
                        self.call(5, 0)?;
                        self.call(6, 0)?;
                    }
                    self.call(3, 0)?;
                }
                Some(0)
            }
            _ => Some(0),
        }
    }
}

/// # Safety
/// Synchronous native callback/context and failure byte remain live. The
/// callback contains C++ exceptions, and may mutate later traversed fields.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_presentation_flow(
    kind: u32,
    context: *mut c_void,
    step: Step,
    failed: *const u8,
    arg: u64,
) -> u64 {
    if failed.is_null() {
        return 0;
    }
    Flow {
        context,
        step,
        failed,
    }
    .run(kind, arg)
    .unwrap_or(0)
}

/// # Safety
/// `pixels` addresses `count` writable u32 pixels with no concurrent access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_presentation_layer_alpha(pixels: *mut u32, count: usize) {
    for index in 0..count {
        let p = unsafe { pixels.add(index) };
        let value = unsafe { p.read() };
        unsafe {
            p.write(if value & 0xffffff != 0 {
                value | 0xff000000
            } else {
                0
            })
        };
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[derive(Default)]
    struct C {
        ids: Vec<u32>,
        cursor: usize,
        failed: u8,
        fail: Option<u32>,
    }
    unsafe extern "C" fn walk(p: *mut c_void, id: u32, _: u64) -> u64 {
        let c = unsafe { &mut *p.cast::<C>() };
        c.ids.push(id);
        if c.fail == Some(id) {
            c.failed = 1;
        }
        match id {
            0 => u64::from(c.cursor < 3),
            1 => u64::from(c.cursor == 1),
            3 => {
                c.cursor += 1;
                0
            }
            _ => 0,
        }
    }
    fn run(kind: u32, c: &mut C, step: Step, arg: u64) -> u64 {
        unsafe {
            a2fo_rs_stage10_presentation_flow(kind, (c as *mut C).cast(), step, &c.failed, arg)
        }
    }
    #[test]
    fn find_stops_before_increment_and_failure_stops_before_action() {
        let mut c = C::default();
        assert_eq!(run(1, &mut c, walk, 0), 1);
        assert_eq!(c.cursor, 1);
        assert_eq!(c.ids, [0, 1, 3, 0, 1]);
        let mut c = C {
            fail: Some(2),
            ..C::default()
        };
        run(0, &mut c, walk, 0);
        assert_eq!(c.ids, [0, 2]);
    }
    #[test]
    fn count_and_filter_visit_preserve_cursor_order() {
        let mut c = C::default();
        assert_eq!(run(2, &mut c, walk, 0), 1);
        assert!(!c.ids.contains(&2));
        let mut c = C::default();
        assert_eq!(run(3, &mut c, walk, 0), 1);
        assert_eq!(c.ids, [0, 1, 3, 0, 1, 2, 3, 0, 1, 3, 0]);
    }
    unsafe extern "C" fn live(p: *mut c_void, id: u32, _: u64) -> u64 {
        let c = unsafe { &mut *p.cast::<C>() };
        c.ids.push(id);
        match id {
            0 | 1 | 4 | 7 | 8 | 12 => 1,
            3 => 5,
            // Visitor mutates current node into self-link; successor was read
            // before this mutation, but the equality decision is deliberately lazy.
            14 => {
                c.cursor = 1;
                0
            }
            15 => c.cursor as u64,
            _ => 0,
        }
    }
    #[test]
    fn live_successor_is_read_before_visitor_and_cycle_guard_is_after() {
        let mut c = C::default();
        run(6, &mut c, live, 100);
        assert_eq!(c.ids, [0, 1, 2, 3, 4, 6, 7, 8, 10, 11, 12, 13, 14, 15]);
        let mut c = C::default();
        run(6, &mut c, live, 4);
        assert_eq!(c.ids, [0, 1, 2, 3, 4]);
    }
    #[test]
    fn layered_black_ignores_original_alpha() {
        let mut pixels = [0, 0x80000000, 0x112233, 0x12345678, 0xffffffff];
        unsafe { a2fo_rs_stage10_presentation_layer_alpha(pixels.as_mut_ptr(), pixels.len()) };
        assert_eq!(pixels, [0, 0, 0xff112233, 0xff345678, 0xffffffff]);
    }
    struct Text {
        bytes: Vec<u8>,
        line: Vec<u8>,
        lines: Vec<Vec<u8>>,
        failed: u8,
        mutate: bool,
        reads: Vec<usize>,
    }
    unsafe extern "C" fn text_step(p: *mut c_void, id: u32, arg: u64) -> u64 {
        let c = unsafe { &mut *p.cast::<Text>() };
        match id {
            0 => {
                c.reads.push(arg as usize);
                c.bytes[arg as usize] as u64
            }
            1 => u64::from(!c.line.is_empty()),
            2 => {
                c.lines.push(core::mem::take(&mut c.line));
                if c.mutate {
                    c.bytes[1] = b'X';
                    c.mutate = false;
                }
                0
            }
            3 => {
                c.line.clear();
                0
            }
            4 => {
                c.line.push(arg as u8);
                0
            }
            _ => 0,
        }
    }
    #[test]
    fn split_rereads_terminator_after_native_push_mutates_input() {
        let mut c = Text {
            bytes: b"a\0b\0".to_vec(),
            line: vec![],
            lines: vec![],
            failed: 0,
            mutate: true,
            reads: vec![],
        };
        unsafe {
            a2fo_rs_stage10_presentation_flow(5, (&raw mut c).cast(), text_step, &c.failed, 0)
        };
        assert_eq!(c.lines, [b"a".to_vec(), b"b".to_vec()]);
        assert_eq!(c.reads, [0, 1, 1, 2, 3, 3]);
    }
    unsafe extern "C" fn bounded(p: *mut c_void, id: u32, index: u64) -> u64 {
        let c = unsafe { &mut *p.cast::<C>() };
        c.ids.push(id);
        match id {
            0 => u64::from(index < 2),
            1 => 0,
            _ => 0,
        }
    }
    #[test]
    fn checked_string_read_failure_does_not_append_or_probe_further() {
        let mut c = C::default();
        assert_eq!(run(4, &mut c, bounded, 260), 0);
        assert_eq!(c.ids, [0, 1, 2, 0, 1, 2, 0]);
        let mut c = C::default();
        assert_eq!(run(4, &mut c, bounded, 1), 0);
        assert_eq!(c.ids, [0, 1, 2]);
    }
}

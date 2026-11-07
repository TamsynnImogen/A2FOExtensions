//! Fire-arc text control. Native strings, streams, CRT calls and exceptions
//! remain in C++; these synchronous callbacks borrow native context.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
#[inline]
unsafe fn call(step: Step, context: *mut c_void, op: u32, arg: usize) -> usize {
    unsafe { step(context, op, arg) }
}
/// 0 = right, 1 = left, 2 = direct append, 3 = separator and append.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_fire_arcs_join(
    left_len: usize,
    right_len: usize,
    last: u8,
) -> u8 {
    if left_len == 0 {
        0
    } else if right_len == 0 {
        1
    } else if last == b'\\' || last == b'/' {
        2
    } else {
        3
    }
}
/// Ops: 0 clear, 1 open, 2 seek/tell, 3 log oversized, 4 seek begin,
/// 5 stream into ostringstream, 6 assign str, 7 test stream state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_fire_arcs_read(
    step: Step,
    context: *mut c_void,
    size: *const i64,
    failed: *const u8,
) -> u8 {
    if size.is_null() || failed.is_null() {
        return 0;
    }
    let invoke = |op| unsafe { call(step, context, op, 0) };
    invoke(0);
    if unsafe { *failed } != 0 {
        return 0;
    }
    if invoke(1) == 0 || unsafe { *failed } != 0 {
        return 0;
    }
    invoke(2);
    if unsafe { *failed } != 0 {
        return 0;
    }
    if unsafe { *size } < 0 || unsafe { *size } > 2 * 1024 * 1024 {
        invoke(3);
        return 0;
    }
    for op in [4, 5, 6] {
        invoke(op);
        if unsafe { *failed } != 0 {
            return 0;
        }
    }
    let result = invoke(7) != 0;
    u8::from(result && unsafe { *failed } == 0)
}
/// Ops: 0 get root, 1 path, 2 read, 3 parse, 4 log valid, 5 log invalid.
/// Parse status: 0 absent, 1 valid, 2 invalid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_fire_arcs_root(
    step: Step,
    context: *mut c_void,
    candidate: *const u8,
    failed: *const u8,
) -> usize {
    if candidate.is_null() || failed.is_null() {
        return 1;
    }
    let invoke = |op| unsafe { call(step, context, op, 0) };
    if invoke(0) == 0 || unsafe { *failed } != 0 {
        return 1;
    }
    invoke(1);
    if unsafe { *failed } != 0 {
        return 1;
    }
    if invoke(2) == 0 || unsafe { *failed } != 0 {
        return 1;
    }
    let status = invoke(3);
    if unsafe { *failed } != 0 {
        return 1;
    }
    if status == 1 {
        invoke(4);
        if unsafe { *failed } != 0 {
            return 1;
        }
        if unsafe { *candidate } != 0 { 2 } else { 3 }
    } else if status == 2 {
        invoke(5);
        if unsafe { *failed } != 0 {
            return 1;
        }
        4
    } else {
        1
    }
}
/// Ops: 0 length, 1 native CRT isspace(index), 2 native substr(begin),
/// 3 native move assignment with span length supplied in arg.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_fire_arcs_trim(
    step: Step,
    context: *mut c_void,
    failed: *const u8,
) {
    if failed.is_null() {
        return;
    }
    let invoke = |op, arg| unsafe { call(step, context, op, arg) };
    let len = invoke(0, 0);
    if unsafe { *failed } != 0 {
        return;
    }
    let mut begin = 0;
    while begin < len {
        if invoke(1, begin) == 0 || unsafe { *failed } != 0 {
            break;
        }
        begin += 1;
    }
    if unsafe { *failed } != 0 {
        return;
    }
    let mut end = len;
    while end > begin {
        if invoke(1, end - 1) == 0 || unsafe { *failed } != 0 {
            break;
        }
        end -= 1;
    }
    if unsafe { *failed } != 0 {
        return;
    }
    invoke(2, begin);
    if unsafe { *failed } == 0 {
        invoke(3, end - begin);
    }
}
/// Ops: 0 length, 1 native CRT tolower and write byte at index.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_fire_arcs_lower(step: Step, context: *mut c_void) {
    let len = unsafe { call(step, context, 0, 0) };
    for index in 0..len {
        unsafe {
            call(step, context, 1, index);
        }
    }
}
/// Ops: 0 clear present, 1 clear output, 2 guard, 3 engine call,
/// 4 set present, 5 assign C string, 6 trim, 7 clear on caught failure.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_fire_arcs_query(
    step: Step,
    context: *mut c_void,
    failed: *const u8,
) -> u8 {
    if failed.is_null() {
        return 0;
    }
    let invoke = |op| unsafe { call(step, context, op, 0) };
    invoke(0);
    invoke(1);
    if invoke(2) == 0 {
        return 0;
    }
    let found = invoke(3);
    if found == 0 {
        return 1;
    }
    invoke(4);
    invoke(5);
    if unsafe { *failed } != 0 {
        invoke(7);
        return 0;
    }
    invoke(6);
    if unsafe { *failed } != 0 {
        invoke(7);
        return 0;
    }
    1
}
/// Ops: 0 strtof and capture end/errno, 1 end==start, 2 ERANGE,
/// 3 finite, 4 current byte, 5 advance, 6 CRT isspace, 7 write output.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_fire_arcs_float(step: Step, context: *mut c_void) -> u8 {
    let invoke = |op| unsafe { call(step, context, op, 0) };
    if invoke(0) == 0 {
        return 0;
    }
    if invoke(1) != 0 || invoke(2) != 0 || invoke(3) == 0 {
        return 0;
    }
    let byte = invoke(4);
    if byte == b'f' as usize || byte == b'F' as usize {
        invoke(5);
    }
    while invoke(6) != 0 {
        invoke(5);
    }
    if invoke(4) != 0 {
        return 0;
    }
    invoke(7);
    1
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn join_branches() {
        assert_eq!(a2fo_rs_stage10_fire_arcs_join(0, 1, 0), 0);
        assert_eq!(a2fo_rs_stage10_fire_arcs_join(1, 0, 0), 1);
        assert_eq!(a2fo_rs_stage10_fire_arcs_join(1, 1, b'/'), 2);
        assert_eq!(a2fo_rs_stage10_fire_arcs_join(1, 1, b'\\'), 2);
        assert_eq!(a2fo_rs_stage10_fire_arcs_join(1, 1, b'x'), 3);
    }
}
#[cfg(test)]
mod flow_tests {
    use super::*;
    struct Trace {
        ops: Vec<u32>,
        failed: u8,
        values: [usize; 8],
        bytes: Vec<u8>,
        cursor: usize,
    }
    unsafe extern "C" fn step(ctx: *mut c_void, op: u32, _: usize) -> usize {
        let c = unsafe { &mut *(ctx as *mut Trace) };
        c.ops.push(op);
        if op == 5 && c.values[5] == usize::MAX {
            c.failed = 1;
        }
        match op {
            4 if !c.bytes.is_empty() => c.bytes[c.cursor] as usize,
            5 if !c.bytes.is_empty() => {
                c.cursor += 1;
                0
            }
            6 if !c.bytes.is_empty() => {
                usize::from(c.bytes[c.cursor] == b' ' || c.bytes[c.cursor] == b'\t')
            }
            _ => c.values[op as usize],
        }
    }
    fn trace() -> Trace {
        Trace {
            ops: Vec::new(),
            failed: 0,
            values: [0; 8],
            bytes: Vec::new(),
            cursor: 0,
        }
    }
    #[test]
    fn read_bound_and_order() {
        let mut c = trace();
        c.values[1] = 1;
        c.values[7] = 1;
        let mut size = 2 * 1024 * 1024;
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_fire_arcs_read(step, &mut c as *mut _ as _, &size, &c.failed)
            },
            1
        );
        assert_eq!(c.ops, [0, 1, 2, 4, 5, 6, 7]);
        c.ops.clear();
        size += 1;
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_fire_arcs_read(step, &mut c as *mut _ as _, &size, &c.failed)
            },
            0
        );
        assert_eq!(c.ops, [0, 1, 2, 3]);
    }
    #[test]
    fn root_status_order() {
        let mut c = trace();
        c.values[0] = 1;
        c.values[2] = 1;
        c.values[3] = 2;
        let candidate = 0;
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_fire_arcs_root(step, &mut c as *mut _ as _, &candidate, &c.failed)
            },
            4
        );
        assert_eq!(c.ops, [0, 1, 2, 3, 5]);
        c.ops.clear();
        c.values[3] = 1;
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_fire_arcs_root(step, &mut c as *mut _ as _, &candidate, &c.failed)
            },
            3
        );
        assert_eq!(c.ops, [0, 1, 2, 3, 4]);
    }
    #[test]
    fn query_clears_before_guard_and_on_assignment_failure() {
        let mut c = trace();
        assert_eq!(
            unsafe { a2fo_rs_stage10_fire_arcs_query(step, &mut c as *mut _ as _, &c.failed) },
            0
        );
        assert_eq!(c.ops, [0, 1, 2]);
        c.ops.clear();
        c.values[2] = 1;
        c.values[3] = 1;
        c.values[5] = usize::MAX;
        assert_eq!(
            unsafe { a2fo_rs_stage10_fire_arcs_query(step, &mut c as *mut _ as _, &c.failed) },
            0
        );
        assert_eq!(c.ops, [0, 1, 2, 3, 4, 5, 7]);
    }
    #[test]
    fn float_suffix_whitespace_and_embedded_nul() {
        let mut c = trace();
        c.values[0] = 1;
        c.values[3] = 1;
        c.bytes = b"f \0garbage".to_vec();
        assert_eq!(
            unsafe { a2fo_rs_stage10_fire_arcs_float(step, &mut c as *mut _ as _) },
            1
        );
        assert_eq!(c.ops, [0, 1, 2, 3, 4, 5, 6, 5, 6, 4, 7]);
        assert_eq!(c.cursor, 2);
        c.ops.clear();
        c.cursor = 0;
        c.bytes = b"fX\0".to_vec();
        assert_eq!(
            unsafe { a2fo_rs_stage10_fire_arcs_float(step, &mut c as *mut _ as _) },
            0
        );
        assert!(!c.ops.contains(&7));
    }
}
#[cfg(test)]
mod byte_tests {
    use super::*;
    struct Bytes {
        data: Vec<u8>,
        begin: usize,
        failed: u8,
        visits: Vec<usize>,
    }
    unsafe extern "C" fn trim_step(ctx: *mut c_void, op: u32, arg: usize) -> usize {
        let c = unsafe { &mut *(ctx as *mut Bytes) };
        match op {
            0 => c.data.len(),
            1 => {
                c.visits.push(arg);
                usize::from(c.data[arg] == b' ' || c.data[arg] == b'\t')
            }
            2 => {
                c.begin = arg;
                0
            }
            3 => {
                c.data = c.data[c.begin..c.begin + arg].to_vec();
                0
            }
            _ => 0,
        }
    }
    unsafe extern "C" fn lower_step(ctx: *mut c_void, op: u32, arg: usize) -> usize {
        let c = unsafe { &mut *(ctx as *mut Bytes) };
        if op == 0 {
            return c.data.len();
        }
        c.visits.push(arg);
        c.data[arg] = c.data[arg].to_ascii_lowercase();
        0
    }
    #[test]
    fn trim_mutation_preserves_embedded_nul() {
        let mut c = Bytes {
            data: b" \tA\0B \t".to_vec(),
            begin: 0,
            failed: 0,
            visits: vec![],
        };
        unsafe { a2fo_rs_stage10_fire_arcs_trim(trim_step, &mut c as *mut _ as _, &c.failed) };
        assert_eq!(c.data, b"A\0B");
        assert_eq!(c.visits, [0, 1, 2, 6, 5, 4]);
    }
    #[test]
    fn lower_mutates_each_byte_in_order() {
        let mut c = Bytes {
            data: vec![b'A', 0, 0xC0, b'Z'],
            begin: 0,
            failed: 0,
            visits: vec![],
        };
        unsafe { a2fo_rs_stage10_fire_arcs_lower(lower_step, &mut c as *mut _ as _) };
        assert_eq!(c.data, [b'a', 0, 0xC0, b'z']);
        assert_eq!(c.visits, [0, 1, 2, 3]);
    }
}

use core::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ResourceLimitPlan {
    pub values: [i32; 3],
    pub legacy_mask: u8,
    pub modern_mask: u8,
    pub apply_mask: u8,
    pub reserved: u8,
}

pub type RootStep = unsafe extern "C" fn(*mut c_void, u32) -> u8;
pub type BeginStatement =
    unsafe extern "C" fn(*mut c_void, usize, usize, *mut *const u8, *mut usize) -> u8;
pub type Identifier = unsafe extern "C" fn(*mut c_void, usize, *mut *const u8, *mut usize) -> u8;
pub type ParseLiteral = unsafe extern "C" fn(*mut c_void, usize, *mut i32) -> u8;
pub type EndStatement = unsafe extern "C" fn(*mut c_void) -> u8;
pub type ApplyStep = unsafe extern "C" fn(*mut c_void, u32, i32) -> u8;

/// # Safety
/// `step` is a valid synchronous native callback and `context` satisfies its
/// contract. Native callback exceptions are captured in that context.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_resource_scan_roots(
    count: u32,
    context: *mut c_void,
    step: RootStep,
) -> u8 {
    if count > 4096 {
        return 0;
    }
    for index in 0..count {
        if unsafe { step(context, index) } == 0 {
            return 0;
        }
    }
    1
}

unsafe fn equal_bytes(bytes: *const u8, length: usize, expected: &[u8]) -> bool {
    if length != expected.len() || bytes.is_null() {
        return false;
    }
    for (index, expected_byte) in expected.iter().enumerate() {
        if unsafe { bytes.add(index).read() } != *expected_byte {
            return false;
        }
    }
    true
}

fn command(bytes: *const u8, length: usize) -> Option<(usize, bool)> {
    const NAMES: [(&[u8], &[u8]); 3] = [
        (b"cfgMaxDilithium", b"MAX_DILITHIUM"),
        (b"cfgMaxCrew", b"MAX_CREW"),
        (b"cfgMaxOfficers", b"MAX_OFFICERS"),
    ];
    for (index, (legacy, modern)) in NAMES.iter().enumerate() {
        if unsafe { equal_bytes(bytes, length, modern) } {
            return Some((index, true));
        }
        if unsafe { equal_bytes(bytes, length, legacy) } {
            return Some((index, false));
        }
    }
    None
}

/// # Safety
/// `source` covers `length` stable native bytes. All callbacks are valid and
/// synchronous; their returned byte spans remain readable until the next
/// callback, and they never mutate `source`. `output` is writable and disjoint.
/// Native callbacks capture exceptions and return zero; C++ rethrows afterward.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_resource_parse(
    source: *const u8,
    length: usize,
    context: *mut c_void,
    begin_statement: BeginStatement,
    identifier: Identifier,
    parse_literal: ParseLiteral,
    end_statement: EndStatement,
    output: *mut ResourceLimitPlan,
) -> u8 {
    if (source.is_null() && length != 0) || output.is_null() {
        return 0;
    }
    let mut result = ResourceLimitPlan::default();
    let mut begin = 0usize;
    while begin < length {
        let mut end = begin;
        while end < length && unsafe { source.add(end).read() } != b';' {
            end += 1;
        }
        let has_semicolon = end < length;
        let mut statement = core::ptr::null();
        let mut statement_length = 0usize;
        if unsafe { begin_statement(context, begin, end, &mut statement, &mut statement_length) }
            == 0
        {
            return 0;
        }
        if (statement.is_null() && statement_length != 0) || statement_length != end - begin {
            let _ = unsafe { end_statement(context) };
            return 0;
        }
        let mut equals = 0usize;
        while equals < statement_length && unsafe { statement.add(equals).read() } != b'=' {
            equals += 1;
        }
        let mut ok = true;
        if equals < statement_length {
            let mut name = core::ptr::null();
            let mut name_length = 0usize;
            if unsafe { identifier(context, equals, &mut name, &mut name_length) } == 0 {
                ok = false;
            } else if let Some((index, modern)) = command(name, name_length) {
                let bit = 1u8 << index;
                if modern {
                    result.modern_mask |= bit;
                } else {
                    let mut value = 0i32;
                    match unsafe { parse_literal(context, equals, &mut value) } {
                        2 => {
                            result.values[index] = value;
                            result.legacy_mask |= bit;
                        }
                        1 => {}
                        _ => ok = false,
                    }
                }
            }
        }
        if unsafe { end_statement(context) } == 0 {
            ok = false;
        }
        if !ok {
            return 0;
        }
        if !has_semicolon {
            break;
        }
        begin = end + 1;
    }
    result.apply_mask = result.legacy_mask & !result.modern_mask & 0x07;
    unsafe {
        output.write(result);
    }
    1
}

/// # Safety
/// `plan` points to one initialized plan, `applied` is writable and disjoint,
/// and `step` performs the native validation/write for each selected index.
/// The callback captures exceptions and returns zero on failure.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_resource_apply(
    plan: *const ResourceLimitPlan,
    context: *mut c_void,
    step: ApplyStep,
    applied: *mut u32,
) -> u8 {
    if plan.is_null() || applied.is_null() {
        return 0;
    }
    let plan = unsafe { plan.read() };
    let mut count = 0u32;
    for index in 0..3u32 {
        if plan.apply_mask & (1u8 << index) == 0 {
            continue;
        }
        if unsafe { step(context, index, plan.values[index as usize]) } == 0 {
            unsafe {
                applied.write(count);
            }
            return 0;
        }
        count += 1;
    }
    unsafe {
        applied.write(count);
    }
    1
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{string::String, vec::Vec};

    #[derive(Default)]
    struct Context {
        source: Vec<u8>,
        statement: Vec<u8>,
        name: Vec<u8>,
        trace: Vec<String>,
        fail_on: Option<u32>,
        fail_apply_on: Option<u32>,
        fail_identifier: bool,
        fail_literal: bool,
    }
    unsafe extern "C" fn root(context: *mut c_void, index: u32) -> u8 {
        let context = unsafe { &mut *context.cast::<Context>() };
        context.trace.push(format!("root:{index}"));
        u8::from(context.fail_on != Some(index))
    }
    unsafe extern "C" fn begin(
        context: *mut c_void,
        first: usize,
        last: usize,
        bytes: *mut *const u8,
        length: *mut usize,
    ) -> u8 {
        let context = unsafe { &mut *context.cast::<Context>() };
        context.statement = context.source[first..last].to_vec();
        context.trace.push(format!("begin:{first}:{last}"));
        unsafe {
            bytes.write(context.statement.as_ptr());
            length.write(context.statement.len());
        }
        1
    }
    unsafe extern "C" fn identifier(
        context: *mut c_void,
        equals: usize,
        bytes: *mut *const u8,
        length: *mut usize,
    ) -> u8 {
        let context = unsafe { &mut *context.cast::<Context>() };
        if context.fail_identifier {
            context.trace.push(format!("identifier-error:{equals}"));
            return 0;
        }
        let prefix = std::str::from_utf8(&context.statement[..equals]).unwrap();
        context.name = prefix.trim().as_bytes().to_vec();
        context.trace.push(format!("identifier:{equals}"));
        unsafe {
            bytes.write(context.name.as_ptr());
            length.write(context.name.len());
        }
        1
    }
    unsafe extern "C" fn parse(context: *mut c_void, equals: usize, value: *mut i32) -> u8 {
        let context = unsafe { &mut *context.cast::<Context>() };
        context.trace.push(format!("parse:{equals}"));
        if context.fail_literal {
            return 0;
        }
        let literal = std::str::from_utf8(&context.statement[equals + 1..])
            .unwrap()
            .trim();
        match literal.parse::<i32>() {
            Ok(number) if number >= 0 => {
                unsafe {
                    value.write(number);
                }
                2
            }
            _ => 1,
        }
    }
    unsafe extern "C" fn end(context: *mut c_void) -> u8 {
        let context = unsafe { &mut *context.cast::<Context>() };
        context.trace.push("end".into());
        context.name.clear();
        context.statement.clear();
        1
    }
    unsafe extern "C" fn apply(context: *mut c_void, index: u32, value: i32) -> u8 {
        let context = unsafe { &mut *context.cast::<Context>() };
        context.trace.push(format!("apply:{index}:{value}"));
        u8::from(context.fail_apply_on != Some(index))
    }
    fn parsed(source: &str) -> (ResourceLimitPlan, Context) {
        let mut context = Context {
            source: source.as_bytes().to_vec(),
            ..Default::default()
        };
        let mut result = ResourceLimitPlan::default();
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_resource_parse(
                    context.source.as_ptr(),
                    context.source.len(),
                    (&mut context as *mut Context).cast(),
                    begin,
                    identifier,
                    parse,
                    end,
                    &mut result,
                )
            },
            1
        );
        (result, context)
    }

    #[test]
    fn roots_are_ordered_bounded_and_stop_on_callback_failure() {
        let mut context = Context::default();
        let opaque = (&mut context as *mut Context).cast();
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_resource_scan_roots(0, opaque, root) },
            1
        );
        assert!(context.trace.is_empty());
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_resource_scan_roots(3, opaque, root) },
            1
        );
        assert_eq!(context.trace, ["root:0", "root:1", "root:2"]);
        context.trace.clear();
        context.fail_on = Some(1);
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_resource_scan_roots(3, opaque, root) },
            0
        );
        assert_eq!(context.trace, ["root:0", "root:1"]);
        context.trace.clear();
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_resource_scan_roots(4097, opaque, root) },
            0
        );
        assert!(context.trace.is_empty());
        context.fail_on = None;
        assert_eq!(
            unsafe { a2fo_rs_stage9_a1_resource_scan_roots(4096, opaque, root) },
            1
        );
        assert_eq!(context.trace.len(), 4096);
    }

    #[test]
    fn duplicate_valid_legacy_and_modern_suppression() {
        let (plan, context) = parsed("cfgMaxCrew=2;cfgMaxCrew=bad; cfgMaxCrew=7;MAX_CREW=invalid;");
        assert_eq!(plan.values, [0, 7, 0]);
        assert_eq!(
            (plan.legacy_mask, plan.modern_mask, plan.apply_mask),
            (2, 2, 0)
        );
        assert_eq!(
            context
                .trace
                .iter()
                .filter(|e| e.starts_with("parse:"))
                .count(),
            3
        );
        assert_eq!(
            context.trace.iter().filter(|e| e.as_str() == "end").count(),
            4
        );
    }

    #[test]
    fn ordered_statement_and_equals_calls_with_empty_segments() {
        let (plan, context) = parsed(";cfgMaxDilithium=4;unknown=9;;cfgMaxOfficers=6");
        assert_eq!(plan.values, [4, 0, 6]);
        assert_eq!(plan.apply_mask, 0b101);
        assert_eq!(context.trace[0..3], ["begin:0:0", "end", "begin:1:18"]);
        assert_eq!(
            context.trace.iter().filter(|e| e.as_str() == "end").count(),
            5
        );
        assert_eq!(
            context
                .trace
                .iter()
                .filter(|e| e.starts_with("parse:"))
                .count(),
            2
        );
    }

    #[test]
    fn modern_names_are_exact_and_legacy_names_case_sensitive() {
        let (plan, context) =
            parsed("MAX_DILITHIUM=0;max_crew=2;cfgMaxCrew=3;cfgMaxOfficers=5;MAX_OFFICERS=1");
        assert_eq!(plan.values, [0, 3, 5]);
        assert_eq!(
            (plan.legacy_mask, plan.modern_mask, plan.apply_mask),
            (0b110, 0b101, 0b010)
        );
        assert_eq!(
            context
                .trace
                .iter()
                .filter(|e| e.starts_with("parse:"))
                .count(),
            2
        );
    }

    #[test]
    fn apply_is_ordered_and_stops_after_native_failure() {
        let (plan, mut context) = parsed("cfgMaxDilithium=1;cfgMaxCrew=2;cfgMaxOfficers=3");
        context.trace.clear();
        let mut applied = 99;
        context.fail_apply_on = Some(1);
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_resource_apply(
                    &plan,
                    (&mut context as *mut Context).cast(),
                    apply,
                    &mut applied,
                )
            },
            0
        );
        assert_eq!(applied, 1);
        assert_eq!(context.trace, ["apply:0:1", "apply:1:2"]);
        context.fail_apply_on = None;
        context.trace.clear();
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_resource_apply(
                    &plan,
                    (&mut context as *mut Context).cast(),
                    apply,
                    &mut applied,
                )
            },
            1
        );
        assert_eq!(applied, 3);
        assert_eq!(context.trace, ["apply:0:1", "apply:1:2", "apply:2:3"]);
    }

    #[test]
    fn native_parse_failure_still_ends_live_statement_frame() {
        let mut context = Context {
            source: b"cfgMaxCrew=4;cfgMaxOfficers=5".to_vec(),
            fail_literal: true,
            ..Default::default()
        };
        let mut result = ResourceLimitPlan::default();
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_resource_parse(
                    context.source.as_ptr(),
                    context.source.len(),
                    (&mut context as *mut Context).cast(),
                    begin,
                    identifier,
                    parse,
                    end,
                    &mut result,
                )
            },
            0
        );
        assert_eq!(
            context.trace,
            ["begin:0:12", "identifier:10", "parse:10", "end"]
        );
        assert!(context.statement.is_empty());
        context.trace.clear();
        context.fail_literal = false;
        context.fail_identifier = true;
        assert_eq!(
            unsafe {
                a2fo_rs_stage9_a1_resource_parse(
                    context.source.as_ptr(),
                    context.source.len(),
                    (&mut context as *mut Context).cast(),
                    begin,
                    identifier,
                    parse,
                    end,
                    &mut result,
                )
            },
            0
        );
        assert_eq!(context.trace, ["begin:0:12", "identifier-error:10", "end"]);
        assert!(context.statement.is_empty());
    }
}

//! Remaining byte-level MissionSelector label and catalog policy decisions.
//! Native code keeps locale conversion, strings, maps, and engine reads.

#[repr(C)]
pub struct Fields {
    pub key_begin: usize,
    pub key_end: usize,
    pub value_begin: usize,
    pub value_end: usize,
    pub valid: bool,
}

#[repr(C)]
pub struct MissionFlags {
    pub unlocked: bool,
    pub replace_launch_filename: bool,
}

fn bytes<'a>(data: *const u8, size: usize) -> Option<&'a [u8]> {
    if data.is_null() {
        return (size == 0).then_some(&[]);
    }
    Some(unsafe { core::slice::from_raw_parts(data, size) })
}

/// # Safety
/// `data` is readable for `size` bytes. Input has already been lowercased by
/// the native CRT using its active locale.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_mission_label_header(
    data: *const u8,
    size: usize,
    mode: u32,
) -> bool {
    let Some(line) = bytes(data, size) else {
        return false;
    };
    line.starts_with(if mode == 1 {
        b"mission_select"
    } else {
        b"single_player"
    })
}

/// # Safety
/// `data` is readable for `size` bytes. `open` is the first opening brace
/// supplied by the existing parser; the last closing brace defines the value.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_mission_label_fields(
    data: *const u8,
    size: usize,
    open: usize,
) -> Fields {
    let invalid = Fields {
        key_begin: 0,
        key_end: 0,
        value_begin: 0,
        value_end: 0,
        valid: false,
    };
    let Some(line) = bytes(data, size) else {
        return invalid;
    };
    let Some(close) = line.iter().rposition(|byte| *byte == b'}') else {
        return invalid;
    };
    if line.first() != Some(&b'+') || open == 0 || open >= close || line[open] != b'{' {
        return invalid;
    }
    Fields {
        key_begin: 1,
        key_end: open,
        value_begin: open + 1,
        value_end: close,
        valid: true,
    }
}

/// # Safety
/// `data` is readable for `size` bytes and already lowercased by native CRT.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_mission_skip_label_key(
    data: *const u8,
    size: usize,
) -> bool {
    let Some(key) = bytes(data, size) else {
        return false;
    };
    key == b"title" || key == b"ok_button" || key == b"cancel_button"
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_mission_campaign_label_index(index: i32) -> i32 {
    if (0..4).contains(&index) { index } else { -1 }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_mission_native_campaign(index: i32, count: i32) -> bool {
    index < count
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_mission_native_slot(
    native_campaign: bool,
    mission_index: i32,
    count: i32,
) -> bool {
    native_campaign && mission_index < count
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_mission_flags(
    campaign_unlocked: bool,
    configured_unlocked: bool,
    configured_filename: bool,
    native_slot: bool,
) -> MissionFlags {
    MissionFlags {
        unlocked: campaign_unlocked && configured_unlocked,
        replace_launch_filename: configured_filename || !native_slot,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn label_slices_and_filters_preserve_first_open_last_close() {
        let line = b"+ Title {A {nested} value} tail";
        let fields = unsafe { a2fo_rs_stage10_mission_label_fields(line.as_ptr(), line.len(), 8) };
        assert!(fields.valid);
        assert_eq!(&line[fields.key_begin..fields.key_end], b" Title ");
        assert_eq!(
            &line[fields.value_begin..fields.value_end],
            b"A {nested} value"
        );
        assert!(unsafe { a2fo_rs_stage10_mission_skip_label_key(b"title".as_ptr(), 5) });
        assert!(!unsafe { a2fo_rs_stage10_mission_skip_label_key(b"title_extra".as_ptr(), 11) });
        assert!(
            !unsafe { a2fo_rs_stage10_mission_label_fields(line.as_ptr(), line.len(), 7) }.valid
        );
        assert!(unsafe {
            a2fo_rs_stage10_mission_label_header(b"mission_select_extra".as_ptr(), 20, 1)
        });
    }

    #[test]
    fn catalog_boundary_and_failure_flags() {
        assert_eq!(a2fo_rs_stage10_mission_campaign_label_index(-1), -1);
        assert_eq!(a2fo_rs_stage10_mission_campaign_label_index(3), 3);
        assert_eq!(a2fo_rs_stage10_mission_campaign_label_index(4), -1);
        assert!(a2fo_rs_stage10_mission_native_campaign(-1, 4));
        assert!(!a2fo_rs_stage10_mission_native_slot(false, 0, 10));
        assert!(!a2fo_rs_stage10_mission_native_slot(true, 10, 10));
        let flags = a2fo_rs_stage10_mission_flags(false, true, true, true);
        assert!(!flags.unlocked);
        assert!(flags.replace_launch_filename);
        let flags = a2fo_rs_stage10_mission_flags(true, true, false, true);
        assert!(flags.unlocked);
        assert!(!flags.replace_launch_filename);
    }

    #[test]
    fn label_field_mutations_match_frozen_first_open_last_close_rule() {
        for prefix in [b'+', b'-'] {
            for inner in [
                b"".as_slice(),
                b"{".as_slice(),
                b"}".as_slice(),
                b"{x}".as_slice(),
                b"x}tail".as_slice(),
            ] {
                let mut line = vec![prefix];
                line.extend_from_slice(b"key {");
                line.extend_from_slice(inner);
                line.extend_from_slice(b"} trailing");
                let open = line.iter().position(|byte| *byte == b'{').unwrap();
                let close = line.iter().rposition(|byte| *byte == b'}').unwrap();
                let fields = unsafe {
                    a2fo_rs_stage10_mission_label_fields(line.as_ptr(), line.len(), open)
                };
                assert_eq!(fields.valid, prefix == b'+' && close > open);
                if fields.valid {
                    assert_eq!(&line[fields.key_begin..fields.key_end], &line[1..open]);
                    assert_eq!(
                        &line[fields.value_begin..fields.value_end],
                        &line[open + 1..close]
                    );
                }
            }
        }
        let invalid = unsafe { a2fo_rs_stage10_mission_label_fields(core::ptr::null(), 1, 0) };
        assert!(!invalid.valid);
    }

    #[test]
    fn catalog_flags_cover_all_mutations() {
        for campaign in [false, true] {
            for configured in [false, true] {
                for has_file in [false, true] {
                    for native in [false, true] {
                        let flags =
                            a2fo_rs_stage10_mission_flags(campaign, configured, has_file, native);
                        assert_eq!(flags.unlocked, campaign && configured);
                        assert_eq!(flags.replace_launch_filename, has_file || !native);
                    }
                }
            }
        }
    }
}

/// Classifies a signed native incident counter for diagnostic admission.
/// Returns 1 when the detail is admitted, 2 for an optional cap+1 marker,
/// and 0 when no report should be emitted.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_reporting_admit(
    incident: i32,
    cap: i32,
    emit_suppression_marker: u8,
) -> u8 {
    if incident <= cap {
        1
    } else if emit_suppression_marker != 0 && incident == cap.saturating_add(1) {
        2
    } else {
        0
    }
}

#[cfg(test)]
mod tests {
    use super::a2fo_rs_stage9_a1_reporting_admit as admit;

    #[test]
    fn signed_cap_and_optional_marker_boundaries() {
        for (cap, marker) in [(4, 0), (8, 0), (16, 0), (32, 1), (64, 1), (128, 1)] {
            assert_eq!(admit(-1, cap, marker), 1);
            assert_eq!(admit(cap, cap, marker), 1);
            assert_eq!(admit(cap + 1, cap, marker), if marker != 0 { 2 } else { 0 });
            assert_eq!(admit(cap + 2, cap, marker), 0);
        }
        assert_eq!(admit(i32::MIN, 16, 0), 1);
        assert_eq!(admit(i32::MAX, 16, 1), 0);
        assert_eq!(admit(i32::MAX, i32::MAX, 1), 1);
        assert_eq!(admit(i32::MIN, i32::MIN, 1), 1);
        assert_eq!(admit(i32::MIN + 1, i32::MIN, 1), 2);
    }
}

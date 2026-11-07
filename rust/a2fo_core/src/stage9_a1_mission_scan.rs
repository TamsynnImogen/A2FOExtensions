//! Bounded choice of serialized A1 AiMission marker; native adapter owns reads.

use core::ffi::c_void;

const MAX_SCAN: u32 = 1024 * 1024;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Result {
    pub markers: u32,
    pub last_distance: u32,
}

pub type Probe = unsafe extern "C" fn(*mut c_void, u32) -> u8;

/// The caller supplies a live probe and writable result for this synchronous call.
/// No pointer or reference is retained across callback invocations.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_mission_scan(
    remaining: u32,
    record_size: u32,
    context: *mut c_void,
    probe: Probe,
    output: *mut Result,
) {
    let mut found = Result::default();
    // SAFETY: The native caller owns context and accepts each bounded distance.
    if unsafe { probe(context, 0) } != 0 {
        found.markers = 1;
        // SAFETY: The native caller owns output and keeps it live for this call.
        unsafe { *output = found };
        return;
    }
    let limit = remaining.min(MAX_SCAN);
    if record_size <= limit {
        for distance in 1..=limit - record_size {
            // SAFETY: The native callback validates the record at this distance.
            if unsafe { probe(context, distance) } == 0 {
                continue;
            }
            found.markers += 1;
            found.last_distance = distance;
            if found.markers > 1 {
                break;
            }
        }
    }
    // SAFETY: The native caller owns output and keeps it live for this call.
    unsafe { *output = found };
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::vec;
    use std::vec::Vec;

    struct Seen {
        hits: Vec<u32>,
        probes: Vec<u32>,
    }

    unsafe extern "C" fn probe(context: *mut c_void, distance: u32) -> u8 {
        // SAFETY: Each test passes a live Seen as the context.
        let seen = unsafe { &mut *context.cast::<Seen>() };
        seen.probes.push(distance);
        u8::from(seen.hits.contains(&distance))
    }

    unsafe extern "C" fn probe_two(context: *mut c_void, distance: u32) -> u8 {
        // SAFETY: The callback forwards the same live context as probe.
        if unsafe { probe(context, distance) } != 0 {
            2
        } else {
            0
        }
    }

    fn scan(remaining: u32, record_size: u32, hits: &[u32]) -> (Result, Vec<u32>) {
        let mut seen = Seen {
            hits: hits.to_vec(),
            probes: Vec::new(),
        };
        let mut result = Result::default();
        // SAFETY: Live context/result and callback match the ABI for this call.
        unsafe {
            a2fo_rs_stage9_a1_mission_scan(
                remaining,
                record_size,
                (&mut seen as *mut Seen).cast(),
                probe,
                &mut result,
            );
        }
        (result, seen.probes)
    }

    #[test]
    fn initial_hit_short_circuits_even_with_no_remaining_bytes() {
        let (result, probes) = scan(0, 8, &[0, 1]);
        assert_eq!(
            result,
            Result {
                markers: 1,
                last_distance: 0
            }
        );
        assert_eq!(probes, vec![0]);
    }

    #[test]
    fn miss_scans_in_order_and_returns_unique_distance() {
        let (result, probes) = scan(12, 8, &[4]);
        assert_eq!(
            result,
            Result {
                markers: 1,
                last_distance: 4
            }
        );
        assert_eq!(probes, vec![0, 1, 2, 3, 4]);
    }

    #[test]
    fn second_hit_stops_immediately() {
        let (result, probes) = scan(20, 8, &[2, 5, 8]);
        assert_eq!(
            result,
            Result {
                markers: 2,
                last_distance: 5
            }
        );
        assert_eq!(probes, vec![0, 1, 2, 3, 4, 5]);
    }

    #[test]
    fn record_boundary_and_empty_scan() {
        assert_eq!(scan(8, 8, &[1]).1, vec![0]);
        assert_eq!(scan(7, 8, &[1]).1, vec![0]);
        assert_eq!(
            scan(9, 8, &[1]).0,
            Result {
                markers: 1,
                last_distance: 1
            }
        );
        assert_eq!(scan(0, 0, &[1]).1, vec![0]);
    }

    #[test]
    fn cap_is_applied_before_last_legal_probe() {
        let edge = MAX_SCAN - 4;
        let (result, probes) = scan(MAX_SCAN + 500, 4, &[edge, edge + 1]);
        assert_eq!(
            result,
            Result {
                markers: 1,
                last_distance: edge
            }
        );
        assert_eq!(probes.first(), Some(&0));
        assert_eq!(probes.last(), Some(&edge));
        assert_eq!(probes.len(), edge as usize + 1);
    }

    #[test]
    fn no_hits_and_noncanonical_nonzero_hit() {
        let (empty, probes) = scan(5, 3, &[]);
        assert_eq!(empty, Result::default());
        assert_eq!(probes, vec![0, 1, 2]);
        let mut seen = Seen {
            hits: vec![1],
            probes: Vec::new(),
        };
        let mut result = Result::default();
        // SAFETY: Live context/result and callback match the ABI for this call.
        unsafe {
            a2fo_rs_stage9_a1_mission_scan(
                4,
                3,
                (&mut seen as *mut Seen).cast(),
                probe_two,
                &mut result,
            );
        }
        assert_eq!(
            result,
            Result {
                markers: 1,
                last_distance: 1
            }
        );
    }
}

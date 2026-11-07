//! Scorch publication admission and deterministic hardpoint selection.
//! The original GNU x87/floor numeric primitive and borrowed engine data stay native.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32) -> usize;
/// # Safety
/// Callback/context are live, synchronous and never unwind. Ops 0/1=native
/// craft/class,2=threshold rejection,3=target empty,4=effects empty,5=max rejected,
/// 6=raw u32 numeric band,7=published count,8=hardpoint count. Outputs are disjoint
/// writable initialized POD words. Native state is stable through these reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_texture_scorch(
    context: *mut c_void,
    step: Step,
    band: *mut u32,
    index: *mut usize,
) -> u8 {
    if unsafe { step(context, 0) } == 0 || unsafe { step(context, 1) } == 0 {
        return 0;
    }
    for op in 2..=5 {
        if unsafe { step(context, op) } != 0 {
            return 0;
        }
    }
    let value = unsafe { step(context, 6) } as u32;
    let published = unsafe { step(context, 7) } as u32;
    if value <= published || value == 0 {
        return 0;
    }
    let count = unsafe { step(context, 8) };
    if count == 0 {
        return 0;
    }
    unsafe {
        band.write(value);
        index.write(value.wrapping_sub(1) as usize % count);
    }
    1
}
#[cfg(test)]
mod tests {
    use super::*;
    use std::vec::Vec;
    struct Context {
        calls: Vec<u32>,
        reject: u32,
        band: u32,
        published: u32,
    }
    unsafe extern "C" fn step(p: *mut c_void, op: u32) -> usize {
        let c = unsafe { &mut *p.cast::<Context>() };
        c.calls.push(op);
        match op {
            0 | 1 => 1,
            2..=5 => usize::from(c.reject == op),
            6 => c.band as usize,
            7 => c.published as usize,
            8 => 3,
            _ => 0,
        }
    }
    #[test]
    fn guards_preserve_lazy_reads() {
        let mut c = Context {
            calls: Vec::new(),
            reject: 3,
            band: 5,
            published: 0,
        };
        let (mut band, mut index) = (77, 88);
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_texture_scorch(
                    (&mut c as *mut Context).cast(),
                    step,
                    &mut band,
                    &mut index,
                )
            },
            0
        );
        assert_eq!(c.calls, [0, 1, 2, 3]);
        assert_eq!((band, index), (77, 88));
    }
    #[test]
    fn raw_max_band_is_data_and_selects_modulo() {
        let mut c = Context {
            calls: Vec::new(),
            reject: 99,
            band: u32::MAX,
            published: 0,
        };
        let (mut band, mut index) = (0, 0);
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_texture_scorch(
                    (&mut c as *mut Context).cast(),
                    step,
                    &mut band,
                    &mut index,
                )
            },
            1
        );
        assert_eq!(band, u32::MAX);
        assert_eq!(index, (u32::MAX - 1) as usize % 3);
        assert_eq!(c.calls, [0, 1, 2, 3, 4, 5, 6, 7, 8]);
    }
    #[test]
    fn repeated_band_does_not_read_hardpoint_count() {
        let mut c = Context {
            calls: Vec::new(),
            reject: 99,
            band: 5,
            published: 5,
        };
        let (mut band, mut index) = (0, 0);
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_texture_scorch(
                    (&mut c as *mut Context).cast(),
                    step,
                    &mut band,
                    &mut index,
                )
            },
            0
        );
        assert_eq!(c.calls, [0, 1, 2, 3, 4, 5, 6, 7]);
    }
}

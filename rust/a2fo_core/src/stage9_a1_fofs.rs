//! Bounded A1 virtual-directory relative-prefix admission.
use core::ffi::c_void;
type Probe = unsafe extern "C" fn(*mut c_void, u32, usize) -> u8;
/// # Safety
/// Context and callback remain live synchronously. Op0 validates the three-byte
/// native span at offset; ops1,2,3 read its bytes only after successful validation.
/// No pointer is retained or dereferenced by Rust; the native adapter owns storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_fofs_prefix(
    context: *mut c_void,
    probe: Probe,
) -> usize {
    let mut offset = 0;
    for _ in 0..16 {
        if unsafe { probe(context, 0, offset) } == 0 || unsafe { probe(context, 1, offset) } != b'.'
        {
            break;
        }
        let separator = unsafe { probe(context, 2, offset) };
        if (separator != b'\\' && separator != b'/') || unsafe { probe(context, 3, offset) } == 0 {
            break;
        }
        offset += 2;
    }
    offset
}
#[cfg(test)]
mod tests {
    use super::*;
    struct Trace {
        data: std::vec::Vec<u8>,
        calls: std::vec::Vec<(u32, usize)>,
    }
    unsafe extern "C" fn probe(ctx: *mut c_void, op: u32, offset: usize) -> u8 {
        let c = unsafe { &mut *ctx.cast::<Trace>() };
        c.calls.push((op, offset));
        if op == 0 {
            u8::from(offset + 3 <= c.data.len())
        } else {
            c.data[offset + (op - 1) as usize]
        }
    }
    #[test]
    fn cap_and_short_circuit() {
        let mut t = Trace {
            data: b"./.\\path\0".to_vec(),
            calls: std::vec::Vec::new(),
        };
        let p = (&mut t as *mut Trace).cast();
        assert_eq!(unsafe { a2fo_rs_stage9_a1_fofs_prefix(p, probe) }, 4);
        assert_eq!(&t.calls[t.calls.len() - 2..], &[(0, 4), (1, 4)]);
        t.data = b"./".repeat(20);
        t.calls.clear();
        assert_eq!(unsafe { a2fo_rs_stage9_a1_fofs_prefix(p, probe) }, 32);
        assert_eq!(t.calls.len(), 64);
    }
    #[test]
    fn empty_tail_and_unreadable() {
        let mut t = Trace {
            data: b"./\0".to_vec(),
            calls: std::vec::Vec::new(),
        };
        let p = (&mut t as *mut Trace).cast();
        assert_eq!(unsafe { a2fo_rs_stage9_a1_fofs_prefix(p, probe) }, 0);
        assert_eq!(t.calls.len(), 4);
        t.data.clear();
        t.calls.clear();
        assert_eq!(unsafe { a2fo_rs_stage9_a1_fofs_prefix(p, probe) }, 0);
        assert_eq!(t.calls, [(0, 0)]);
    }
}

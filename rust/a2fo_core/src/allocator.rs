//! Rust-owned allocations use the host C runtime, already required by the
//! MinGW DLL. Avoid Rust std's newer Windows API and TLS dependencies.
//! Allocation headers preserve arbitrary Layout alignment on 32-bit CRTs.
use core::alloc::{GlobalAlloc, Layout};
use core::ffi::c_void;
use core::ptr;

unsafe extern "C" {
    fn malloc(size: usize) -> *mut c_void;
    fn free(pointer: *mut c_void);
    #[cfg(not(test))]
    fn abort() -> !;
}

struct CrtAllocator;
// SAFETY: malloc/free are thread safe. Each allocation owns its header; every
// returned range has the requested size/alignment and is released to the same
// allocator. No C++/Rust object ownership crosses this allocation boundary.
unsafe impl GlobalAlloc for CrtAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let header = core::mem::size_of::<*mut u8>();
        let alignment = layout.align().max(core::mem::align_of::<*mut u8>());
        let Some(total) = layout
            .size()
            .max(1)
            .checked_add(header)
            .and_then(|n| n.checked_add(alignment - 1))
            .filter(|&n| n <= isize::MAX as usize)
        else {
            return ptr::null_mut();
        };
        // SAFETY: total is nonzero and within pointer offset limits.
        let raw = unsafe { malloc(total).cast::<u8>() };
        if raw.is_null() {
            return raw;
        }
        // SAFETY: the allocation reserves the header plus alignment-1 bytes.
        // The header is itself aligned, even for layouts requesting align=1.
        unsafe {
            let start = raw.add(header);
            let aligned = start.add(start.align_offset(alignment));
            aligned.sub(header).cast::<*mut u8>().write(raw);
            aligned
        }
    }

    unsafe fn dealloc(&self, pointer: *mut u8, _layout: Layout) {
        // SAFETY: GlobalAlloc requires a live allocation returned by alloc.
        // Its private header retains exactly the malloc pointer for free.
        unsafe {
            let raw = pointer
                .sub(core::mem::size_of::<*mut u8>())
                .cast::<*mut u8>()
                .read();
            free(raw.cast());
        }
    }
}

#[cfg(all(not(test), not(feature = "std-host")))]
#[global_allocator]
static ALLOCATOR: CrtAllocator = CrtAllocator;

#[cfg(all(not(test), not(feature = "std-host")))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo<'_>) -> ! {
    // SAFETY: abort never returns and cannot unwind through the C ABI.
    unsafe { abort() }
}

// The distributed core/alloc rlibs contain DWARF unwind-table references even
// when our crate is panic=abort. On these two supported targets the personality
// ABI is Itanium/DWARF (Rust std: library/std/src/sys/personality/gcc.rs).
// An unexpected foreign unwind through Rust must terminate, never silently
// skip Rust cleanup or continue with an incompatible exception runtime.
#[cfg(all(
    not(test),
    not(any(
        all(target_os = "windows", target_env = "gnu", target_arch = "x86"),
        all(target_os = "linux", target_arch = "x86_64")
    ))
))]
compile_error!("Review the abort personality ABI before adding another target");

#[cfg(all(not(test), not(feature = "std-host")))]
#[unsafe(no_mangle)]
pub extern "C" fn rust_eh_personality(
    _version: i32,
    _actions: i32,
    _exception_class: u64,
    _exception_object: *mut c_void,
    _context: *mut c_void,
) -> i32 {
    // SAFETY: unexpected unwinding is a fatal boundary violation.
    unsafe { abort() }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn alignments_and_reallocation_preserve_data() {
        for alignment in [1, 2, 4, 8, 16, 32, 64, 256, 4096] {
            for size in [1, 7, 8, 17, 256, 4096] {
                let layout = Layout::from_size_align(size, alignment).unwrap();
                unsafe {
                    let p = CrtAllocator.alloc(layout);
                    assert!(!p.is_null());
                    assert!((p as usize).is_multiple_of(alignment));
                    p.write_bytes(0x5a, size);
                    let q = CrtAllocator.realloc(p, layout, size * 2);
                    assert!(!q.is_null());
                    assert!((q as usize).is_multiple_of(alignment));
                    assert!(
                        core::slice::from_raw_parts(q, size)
                            .iter()
                            .all(|&b| b == 0x5a)
                    );
                    CrtAllocator.dealloc(q, Layout::from_size_align(size * 2, alignment).unwrap());
                }
            }
        }
    }
    #[test]
    fn zeroed_and_impossible_allocation() {
        unsafe {
            let layout = Layout::from_size_align(257, 64).unwrap();
            let p = CrtAllocator.alloc_zeroed(layout);
            assert!(!p.is_null());
            assert!(core::slice::from_raw_parts(p, 257).iter().all(|&b| b == 0));
            CrtAllocator.dealloc(p, layout);
            // Header/alignment overhead cannot fit in a valid pointer range.
            let huge = Layout::from_size_align(isize::MAX as usize, 1).unwrap();
            assert!(CrtAllocator.alloc(huge).is_null());
        }
    }
}

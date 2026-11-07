//! Pure pixel loops for the native emissive texture path. Native code owns all
//! allocation, locked surfaces, and the order in which source images merge.

use core::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SparseEmissivePixel {
    pub index: u32,
    pub pixel: u32,
}

fn nonblack_count(mut pixel_at: impl FnMut(u32, u32) -> u32, width: u32, height: u32) -> usize {
    let mut count = 0usize;
    for y in 0..height {
        for x in 0..width {
            if pixel_at(x, y) & 0x00ff_ffff != 0 {
                count += 1;
            }
        }
    }
    count
}

fn sparse_is_smaller(nonblack: usize, pixel_count: usize) -> bool {
    // The production target is i686: both native sizeof products are size_t
    // expressions and therefore wrap at 32 bits, including in release builds.
    (nonblack as u32).wrapping_mul(8) < (pixel_count as u32).wrapping_mul(4)
}

fn fill_sparse(
    mut pixel_at: impl FnMut(u32, u32) -> u32,
    width: u32,
    height: u32,
    mut push: impl FnMut(SparseEmissivePixel) -> bool,
) -> bool {
    for y in 0..height {
        for x in 0..width {
            let pixel = pixel_at(x, y);
            if pixel & 0x00ff_ffff == 0 {
                continue;
            }
            if !push(SparseEmissivePixel {
                index: y.wrapping_mul(width).wrapping_add(x),
                pixel,
            }) {
                return false;
            }
        }
    }
    true
}

fn next_mip_pixel(
    mut input: impl FnMut(usize) -> u32,
    width: u32,
    height: u32,
    x: u32,
    y: u32,
) -> u32 {
    let mut samples = [0u32; 4];
    let mut index = 0;
    for dy in 0..2u32 {
        for dx in 0..2u32 {
            let source_x = width
                .wrapping_sub(1)
                .min(x.wrapping_mul(2).wrapping_add(dx));
            let source_y = height
                .wrapping_sub(1)
                .min(y.wrapping_mul(2).wrapping_add(dy));
            samples[index] = input(source_y as usize * width as usize + source_x as usize);
            index += 1;
        }
    }
    let mut averaged = 0;
    for shift in (0..32).step_by(8) {
        let channel = samples
            .iter()
            .map(|sample| (sample >> shift) & 0xff)
            .sum::<u32>();
        averaged |= ((channel + 2) / 4) << shift;
    }
    averaged
}

/// # Safety
/// `source` must address `height` readable rows of at least `width * 4`
/// bytes, each starting `stride_bytes` after the previous row. The row pixels
/// must be aligned for `u32`, as in the native `D3DLOCKED_RECT` loop.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_core_pixels_nonblack_count(
    source: *const u8,
    width: u32,
    height: u32,
    stride_bytes: usize,
) -> usize {
    nonblack_count(
        |x, y| unsafe {
            *((source.add(y as usize * stride_bytes))
                .cast::<u32>()
                .add(x as usize))
        },
        width,
        height,
    )
}

/// The native `sizeof(SparseEmissivePixel)` is eight and `sizeof(uint32_t)` is
/// four. Both size products use wrapping 32-bit `size_t` on the i686 target.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage10_core_pixels_sparse_is_smaller(
    nonblack: usize,
    pixel_count: usize,
) -> u8 {
    sparse_is_smaller(nonblack, pixel_count) as u8
}

/// # Safety
/// `source` has the same row contract as the count function. `push`
/// must be a valid synchronous callback accepting each nonblack pixel in native
/// row order. `context` must remain valid. The callback must catch native
/// exceptions and return zero on failure; Rust stops scanning immediately.
/// The callback may mutate later source pixels but must not invalidate the buffer.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_core_pixels_fill_sparse(
    source: *const u8,
    width: u32,
    height: u32,
    stride_bytes: usize,
    push: unsafe extern "C" fn(*mut c_void, u32, u32) -> u8,
    context: *mut c_void,
) -> u8 {
    fill_sparse(
        |x, y| unsafe {
            *((source.add(y as usize * stride_bytes))
                .cast::<u32>()
                .add(x as usize))
        },
        width,
        height,
        |point| unsafe { push(context, point.index, point.pixel) != 0 },
    ) as u8
}

/// # Safety
/// `source` addresses `width * height` readable pixels and `output`
/// addresses `max(1,width/2) * max(1,height/2)` writable pixels. Both dimensions
/// must be nonzero, and the input and output buffers must not overlap.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_core_pixels_next_mip(
    source: *const u32,
    width: u32,
    height: u32,
    output: *mut u32,
) {
    let next_width = 1.max(width / 2);
    let next_height = 1.max(height / 2);
    for y in 0..next_height {
        for x in 0..next_width {
            let pixel = next_mip_pixel(|index| unsafe { *source.add(index) }, width, height, x, y);
            unsafe { *output.add(y as usize * next_width as usize + x as usize) = pixel };
        }
    }
}

/// # Safety
/// `combined` addresses `count` writable pixels and `source` addresses
/// `count` readable pixels; the buffers must not overlap. Native code supplies
/// the source-specific intensity percent and preserves source iteration order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_core_pixels_merge_dense(
    combined: *mut u32,
    source: *const u32,
    count: usize,
    percent: u32,
) {
    for index in 0..count {
        let old = unsafe { *combined.add(index) };
        let pixel = unsafe { *source.add(index) };
        let merged = crate::emissive::a2fo_rs_emissive_merge(old, pixel, percent);
        unsafe { *combined.add(index) = merged };
    }
}

/// # Safety
/// `combined` addresses `count` writable pixels and `source` addresses
/// `sparse_count` readable POD entries; the buffers must not overlap. Out-of-
/// range indices are skipped, as in the native loop. Duplicate indices merge
/// sequentially.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_core_pixels_merge_sparse(
    combined: *mut u32,
    count: usize,
    source: *const SparseEmissivePixel,
    sparse_count: usize,
    percent: u32,
) {
    for index in 0..sparse_count {
        let point = unsafe { *source.add(index) };
        if point.index as usize >= count {
            continue;
        }
        let old = unsafe { *combined.add(point.index as usize) };
        let merged = crate::emissive::a2fo_rs_emissive_merge(old, point.pixel, percent);
        unsafe { *combined.add(point.index as usize) = merged };
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn pitched_rows_count_and_fill_in_row_major_order() {
        let rows = [0x0011_2233u32, 0xff00_0000, 999, 0, 0x8044_5566, 999];
        let ptr = rows.as_ptr().cast::<u8>();
        assert_eq!(
            unsafe { a2fo_rs_stage10_core_pixels_nonblack_count(ptr, 2, 2, 12) },
            2
        );
        unsafe extern "C" fn push(context: *mut c_void, index: u32, pixel: u32) -> u8 {
            unsafe { &mut *context.cast::<Vec<SparseEmissivePixel>>() }
                .push(SparseEmissivePixel { index, pixel });
            1
        }
        let mut sparse: Vec<SparseEmissivePixel> = Vec::new();
        assert_eq!(
            unsafe {
                a2fo_rs_stage10_core_pixels_fill_sparse(
                    ptr,
                    2,
                    2,
                    12,
                    push,
                    (&mut sparse as *mut Vec<_>).cast(),
                )
            },
            1
        );
        assert_eq!(
            sparse,
            [
                SparseEmissivePixel {
                    index: 0,
                    pixel: 0x0011_2233
                },
                SparseEmissivePixel {
                    index: 3,
                    pixel: 0x8044_5566
                }
            ]
        );
        let mut calls = 0;
        assert!(!fill_sparse(
            |_, _| 1,
            3,
            1,
            |_| {
                calls += 1;
                calls < 2
            }
        ));
        assert_eq!(calls, 2);
    }

    #[test]
    fn sparse_tie_and_i686_size_overflow() {
        assert_eq!(a2fo_rs_stage10_core_pixels_sparse_is_smaller(1, 2), 0);
        assert_eq!(a2fo_rs_stage10_core_pixels_sparse_is_smaller(1, 3), 1);
        assert_eq!(
            a2fo_rs_stage10_core_pixels_sparse_is_smaller(0x2000_0000, 1),
            1
        );
        assert_eq!(
            a2fo_rs_stage10_core_pixels_sparse_is_smaller(1, 0x4000_0000),
            0
        );
    }

    #[test]
    fn odd_mip_and_single_pixel_round_all_channels() {
        let input = [
            0x0101_0101,
            0x0202_0202,
            0x0303_0303,
            0x0404_0404,
            0xffff_ffff,
            0,
        ];
        let mut next = [0];
        unsafe { a2fo_rs_stage10_core_pixels_next_mip(input.as_ptr(), 3, 2, next.as_mut_ptr()) };
        assert_eq!(next, [0x4242_4242]);
        let mut single = [0];
        unsafe { a2fo_rs_stage10_core_pixels_next_mip(next.as_ptr(), 1, 1, single.as_mut_ptr()) };
        assert_eq!(single, next);
    }

    #[test]
    fn dense_and_sparse_merge_use_existing_percent_helper() {
        let mut combined = [0xff00_0000, 0xff10_1000, 0xff00_0000];
        let dense = [0x0001_0203, 0xff11_2233, 0];
        unsafe {
            a2fo_rs_stage10_core_pixels_merge_dense(combined.as_mut_ptr(), dense.as_ptr(), 3, 125)
        };
        assert_eq!(
            combined[0],
            crate::emissive::a2fo_rs_emissive_merge(0xff00_0000, dense[0], 125)
        );
        let sparse = [
            SparseEmissivePixel {
                index: 1,
                pixel: 0x0000_2000,
            },
            SparseEmissivePixel {
                index: 4,
                pixel: 0xffff_ffff,
            },
            SparseEmissivePixel {
                index: 1,
                pixel: 0x0000_4000,
            },
        ];
        unsafe {
            a2fo_rs_stage10_core_pixels_merge_sparse(
                combined.as_mut_ptr(),
                3,
                sparse.as_ptr(),
                sparse.len(),
                150,
            )
        };
        assert_eq!(
            combined[1],
            crate::emissive::a2fo_rs_emissive_merge(
                crate::emissive::a2fo_rs_emissive_merge(
                    crate::emissive::a2fo_rs_emissive_merge(0xff10_1000, dense[1], 125),
                    sparse[0].pixel,
                    150
                ),
                sparse[2].pixel,
                150
            )
        );
        assert_eq!(combined[2], 0xff00_0000);
    }
}

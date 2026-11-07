// Portable D3D8 game device and factory policy. Native adapters own resources.
#[repr(C)]
pub struct GameCaps {
    max_texture_width: u32,
    max_texture_height: u32,
    max_blend_stages: u32,
    max_textures: u32,
    max_lights: u32,
    max_streams: u32,
    max_stride: u32,
    vertex_shader_version: u32,
    max_vertex_consts: u32,
    pixel_shader_version: u32,
    max_blend_matrices: u32,
    max_blend_matrix_index: u32,
    texture_caps: u32,
    device_caps: u32,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_game_identity(matrix: *mut f32) {
    if matrix.is_null() {
        return;
    }
    for i in 0..16 {
        unsafe {
            *matrix.add(i) = if i % 5 == 0 { 1.0 } else { 0.0 };
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_game_defaults(render: *mut u32, stage: *mut u32, z_enable: bool) {
    if render.is_null() || stage.is_null() {
        return;
    }
    let values: &[(usize, u32)] = &[
        (7, u32::from(z_enable)),
        (14, 1),
        (8, 3),
        (9, 2),
        (19, 2),
        (20, 1),
        (22, 3),
        (23, 4),
        (25, 8),
        (26, 0),
        (16, 1),
        (53, 1),
        (54, 1),
        (55, 1),
        (56, 8),
        (58, u32::MAX),
        (59, u32::MAX),
        (60, u32::MAX),
        (136, 1),
        (137, 1),
        (141, 1),
        (142, 1),
        (145, 1),
        (146, 2),
        (147, 0),
        (148, 0),
        (161, 1),
        (162, u32::MAX),
        (168, 15),
        (171, 1),
        (37, 1.0f32.to_bits()),
        (38, 1.0f32.to_bits()),
        (154, 1.0f32.to_bits()),
        (155, 1.0f32.to_bits()),
        (158, 1.0f32.to_bits()),
    ];
    for &(at, value) in values {
        unsafe {
            *render.add(at) = value;
        }
    }
    for i in 0..8 {
        let p = unsafe { stage.add(i * 33) };
        unsafe {
            *p.add(1) = if i == 0 { 4 } else { 1 };
            *p.add(2) = 2;
            *p.add(3) = 1;
            *p.add(4) = if i == 0 { 2 } else { 1 };
            *p.add(5) = 2;
            *p.add(6) = 1;
            *p.add(11) = i as u32;
            *p.add(13) = 1;
            *p.add(14) = 1;
            *p.add(25) = 1;
            *p.add(16) = 1;
            *p.add(17) = 1;
            *p.add(18) = 0;
            *p.add(21) = 1;
            *p.add(28) = 1;
        }
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_matrix_slot(state: u32) -> i32 {
    match state {
        256 => 0,
        2 => 1,
        3 => 2,
        16..=23 => (state - 16 + 3) as i32,
        _ => -1,
    }
}
#[unsafe(no_mangle)]
// NaN passes the legacy C++ comparisons; keep that acceptance exactly.
#[allow(clippy::neg_cmp_op_on_partial_ord)]
pub extern "C" fn a2fo_game_viewport(w: u32, h: u32, min: f32, max: f32) -> bool {
    w != 0 && h != 0 && !(min < 0.0) && !(max > 1.0) && !(min > max)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_texture_admission(
    w: u32,
    h: u32,
    levels: u32,
    usage: u32,
    f: u32,
    pool: u32,
    pitch: bool,
) -> bool {
    if w == 0 || h == 0 || w > 16384 || h > 16384 || pool > 3 || !pitch || f == 41 {
        return false;
    }
    let mut n = 1;
    let mut size = w.max(h);
    while size > 1 {
        size /= 2;
        n += 1;
    }
    levels <= n && (usage & 1 == 0 || f == 21 || f == 22)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_buffer_admission(n: u32, pool: u32) -> bool {
    n != 0 && n <= 64 * 1024 * 1024 && pool <= 3
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_index_admission(n: u32, f: u32, pool: u32) -> bool {
    a2fo_game_buffer_admission(n, pool) && (f == 101 || f == 102)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_image_admission(w: u32, h: u32, pitch: u32) -> bool {
    pitch != 0 && w != 0 && h != 0 && w <= 16384 && h <= 16384
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_reset_valid(
    present: bool,
    recording: bool,
    in_scene: bool,
    ms: u32,
    _backs: u32,
) -> bool {
    present && !recording && !in_scene && ms == 0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_reset_resource_block(kind: u32, pool: u32, refs: i32) -> bool {
    refs > 0 && pool == 0 && matches!(kind, 0..=3)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_game_caps(c: *mut GameCaps) {
    if c.is_null() {
        return;
    }
    let c = unsafe { &mut *c };
    c.max_texture_width = c.max_texture_width.min(16384);
    c.max_texture_height = c.max_texture_height.min(16384);
    c.max_blend_stages = 8;
    c.max_textures = 8;
    c.max_lights = 8;
    c.max_streams = 16;
    c.max_stride = 4096;
    c.vertex_shader_version = 0xfffe0101;
    c.max_vertex_consts = 96;
    c.pixel_shader_version = 0;
    c.max_blend_matrices = 0;
    c.max_blend_matrix_index = 0;
    c.texture_caps &= !(0x800 | 0x10000 | 0x2000 | 0x8000);
    c.device_caps &= !0x1000000;
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_backbuffer(f: u32) -> bool {
    matches!(f, 0 | 21 | 22)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_depth(f: u32) -> bool {
    matches!(f, 80 | 75 | 77 | 71)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_depth_match(t: u32, d: u32) -> bool {
    matches!(t, 21 | 22) && a2fo_game_depth(d)
}
// 0: native delegate; 1: D3D_OK; 2: D3DERR_NOTAVAILABLE.
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_factory_format(usage: u32, r: u32, f: u32, supported: bool) -> u32 {
    if matches!(r, 2 | 4 | 5) {
        return 2;
    }
    if usage & 2 != 0 {
        return if a2fo_game_depth(f) { 1 } else { 2 };
    }
    if matches!(r, 3 | 1) {
        if !supported || usage & 1 != 0 && !matches!(f, 21 | 22) {
            return 2;
        }
        return 1;
    }
    0
}
#[cfg(all(target_arch = "x86", target_os = "windows"))]
unsafe fn matrix_cell(out: *mut f32, a: *const f32, b: *const f32) {
    use core::arch::asm;
    // SAFETY: A points to four contiguous row floats, B to four floats
    // spaced by four; out is one distinct temporary float. The x87 stack
    // remains balanced and the caller's control word is unchanged.
    unsafe {
        asm!(
            "fldz",
            "fld dword ptr [{a}]", "fmul dword ptr [{b}]", "faddp st(1), st(0)",
            "fld dword ptr [{a}+4]", "fmul dword ptr [{b}+16]", "faddp st(1), st(0)",
            "fld dword ptr [{a}+8]", "fmul dword ptr [{b}+32]", "faddp st(1), st(0)",
            "fld dword ptr [{a}+12]", "fmul dword ptr [{b}+48]", "faddp st(1), st(0)",
            "fstp dword ptr [{out}]",
            a = in(reg) a, b = in(reg) b, out = in(reg) out,
            out("st(0)") _, out("st(1)") _, out("st(2)") _, out("st(3)") _,
            out("st(4)") _, out("st(5)") _, out("st(6)") _, out("st(7)") _,
            options(nostack)
        );
    }
}
#[cfg(not(all(target_arch = "x86", target_os = "windows")))]
unsafe fn matrix_cell(out: *mut f32, a: *const f32, b: *const f32) {
    let mut value = 0.0f32;
    for k in 0..4 {
        unsafe {
            value += *a.add(k) * *b.add(k * 4);
        }
    }
    unsafe {
        *out = value;
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_game_multiply(out: *mut f32, a: *const f32, b: *const f32) {
    if out.is_null() || a.is_null() || b.is_null() {
        return;
    }
    let mut result = [0.0f32; 16];
    for y in 0..4 {
        for x in 0..4 {
            // SAFETY: bounded 4x4 matrix indices and one temporary output cell.
            unsafe {
                matrix_cell(result.as_mut_ptr().add(y * 4 + x), a.add(y * 4), b.add(x));
            }
        }
    }
    for (i, value) in result.into_iter().enumerate() {
        unsafe {
            *out.add(i) = value;
        }
    }
}
#[repr(C)]
pub struct PeLayout {
    dos_size: u32,
    nt_size: u32,
    import_size: u32,
    dos_magic_at: u32,
    dos_lfanew_at: u32,
    nt_signature_at: u32,
    nt_machine_at: u32,
    nt_magic_at: u32,
    nt_image_size_at: u32,
    nt_import_rva_at: u32,
    nt_import_size_at: u32,
    import_name_at: u32,
    import_original_at: u32,
    import_first_at: u32,
    dos_magic: u32,
    nt_signature: u32,
    machine: u32,
    nt_magic: u32,
    ordinal_mask: u32,
}
#[repr(C)]
pub struct PePatchPlan {
    slot: usize,
    previous: usize,
    index: u32,
    duplicate: u32,
}
pub type PeSpan = extern "C" fn(*mut core::ffi::c_void, usize, usize) -> bool;
pub type PeRead = extern "C" fn(*mut core::ffi::c_void, usize, *mut u8, usize);
unsafe fn pe_u16(read: PeRead, ctx: *mut core::ffi::c_void, address: usize) -> u16 {
    let mut data = [0u8; 2];
    read(ctx, address, data.as_mut_ptr(), 2);
    u16::from_le_bytes(data)
}
unsafe fn pe_u32(read: PeRead, ctx: *mut core::ffi::c_void, address: usize) -> u32 {
    let mut data = [0u8; 4];
    read(ctx, address, data.as_mut_ptr(), 4);
    u32::from_le_bytes(data)
}
fn pe_field(offset: u32, bytes: u32, size: u32) -> bool {
    offset <= size && bytes <= size - offset
}
fn pe_valid(
    base: usize,
    rva: u32,
    count: u32,
    size: u32,
    span: PeSpan,
    ctx: *mut core::ffi::c_void,
) -> bool {
    rva <= size && count <= size - rva && span(ctx, base.wrapping_add(rva as usize), count as usize)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_game_file_bridge_walk(
    base: usize,
    layout: *const PeLayout,
    span: PeSpan,
    read: PeRead,
    ctx: *mut core::ffi::c_void,
    slots: *const usize,
    callbacks: *const usize,
    plan: *mut PePatchPlan,
) -> bool {
    if base == 0 || layout.is_null() || slots.is_null() || callbacks.is_null() || plan.is_null() {
        return false;
    }
    let l = unsafe { &*layout };
    if !pe_field(l.dos_magic_at, 2, l.dos_size)
        || !pe_field(l.dos_lfanew_at, 4, l.dos_size)
        || !pe_field(l.nt_signature_at, 4, l.nt_size)
        || !pe_field(l.nt_machine_at, 2, l.nt_size)
        || !pe_field(l.nt_magic_at, 2, l.nt_size)
        || !pe_field(l.nt_image_size_at, 4, l.nt_size)
        || !pe_field(l.nt_import_rva_at, 4, l.nt_size)
        || !pe_field(l.nt_import_size_at, 4, l.nt_size)
        || !pe_field(l.import_name_at, 4, l.import_size)
        || !pe_field(l.import_original_at, 4, l.import_size)
        || !pe_field(l.import_first_at, 4, l.import_size)
    {
        return false;
    }
    if !span(ctx, base, l.dos_size as usize) {
        return false;
    }
    if unsafe { pe_u16(read, ctx, base.wrapping_add(l.dos_magic_at as usize)) } as u32
        != l.dos_magic
    {
        return false;
    }
    let lfanew = unsafe { pe_u32(read, ctx, base.wrapping_add(l.dos_lfanew_at as usize)) } as i32;
    if !(0..=0x100000).contains(&lfanew) {
        return false;
    }
    let nt = base.wrapping_add(lfanew as usize);
    if !span(ctx, nt, l.nt_size as usize) {
        return false;
    }
    if unsafe { pe_u32(read, ctx, nt.wrapping_add(l.nt_signature_at as usize)) } != l.nt_signature
        || unsafe { pe_u16(read, ctx, nt.wrapping_add(l.nt_machine_at as usize)) } as u32
            != l.machine
        || unsafe { pe_u16(read, ctx, nt.wrapping_add(l.nt_magic_at as usize)) } as u32
            != l.nt_magic
    {
        return false;
    }
    let size = unsafe { pe_u32(read, ctx, nt.wrapping_add(l.nt_image_size_at as usize)) };
    let dir = unsafe { pe_u32(read, ctx, nt.wrapping_add(l.nt_import_rva_at as usize)) };
    let dir_size = unsafe { pe_u32(read, ctx, nt.wrapping_add(l.nt_import_size_at as usize)) };
    if dir == 0 || !pe_valid(base, dir, dir_size, size, span, ctx) {
        return false;
    }
    let mut o = 0u32;
    while o <= dir_size && l.import_size <= dir_size - o {
        let entry = base.wrapping_add(dir as usize).wrapping_add(o as usize);
        let name = unsafe { pe_u32(read, ctx, entry.wrapping_add(l.import_name_at as usize)) };
        if name == 0 {
            break;
        }
        let original =
            unsafe { pe_u32(read, ctx, entry.wrapping_add(l.import_original_at as usize)) };
        let first = unsafe { pe_u32(read, ctx, entry.wrapping_add(l.import_first_at as usize)) };
        if original != 0 && first != 0 {
            let mut i = 0u32;
            loop {
                let a = original.wrapping_add(i.wrapping_mul(4));
                let b = first.wrapping_add(i.wrapping_mul(4));
                if !pe_valid(base, a, 4, size, span, ctx) || !pe_valid(base, b, 4, size, span, ctx)
                {
                    break;
                }
                let import = unsafe { pe_u32(read, ctx, base.wrapping_add(a as usize)) };
                if import == 0 {
                    break;
                }
                if import & l.ordinal_mask == 0
                    && pe_valid(base, import, 2, size, span, ctx)
                    && pe_valid(base, import.wrapping_add(2), 6, size, span, ctx)
                {
                    let mut name_bytes = [0u8; 6];
                    read(
                        ctx,
                        base.wrapping_add(import as usize).wrapping_add(2),
                        name_bytes.as_mut_ptr(),
                        6,
                    );
                    if &name_bytes == b"fopen\0" {
                        let slot = base.wrapping_add(b as usize);
                        let previous = unsafe { pe_u32(read, ctx, slot) } as usize;
                        let mut index = 0usize;
                        while index < 8 {
                            let held = unsafe { *slots.add(index) };
                            if held == slot {
                                unsafe {
                                    *plan = PePatchPlan {
                                        slot,
                                        previous,
                                        index: index as u32,
                                        duplicate: 1,
                                    };
                                }
                                return previous == unsafe { *callbacks.add(index) };
                            }
                            if held == 0 {
                                break;
                            }
                            index += 1;
                        }
                        if index == 8 || previous == 0 {
                            return false;
                        }
                        unsafe {
                            *plan = PePatchPlan {
                                slot,
                                previous,
                                index: index as u32,
                                duplicate: 0,
                            };
                        }
                        return true;
                    }
                }
                if i == u32::MAX {
                    break;
                }
                i += 1;
            }
        }
        if dir_size - o < l.import_size {
            break;
        }
        o += l.import_size;
    }
    false
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_supported_texture(format: u32, pitch: u32) -> bool {
    pitch != 0 && format != 41
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_factory_create_admission(
    adapter: u32,
    kind: u32,
    ms: u32,
    backs: u32,
) -> bool {
    adapter == 0 && kind == 1 && ms == 0 && backs <= 1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_reset_window(same: bool, backs: u32) -> bool {
    same && backs <= 1
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_depth_bytes(format: u32) -> u32 {
    if matches!(format, 80 | 70) { 2 } else { 4 }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_present_log(frames: u64) -> bool {
    frames == 1 || frames.is_multiple_of(600)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_state_index(category: u32, index: u32, secondary: u32) -> bool {
    match category {
        0 => index < 8,
        1 => index < 256,
        2 => index < 8 && secondary < 33,
        3 => index < 16 && secondary <= 4096,
        _ => false,
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_scene_transition(in_scene: bool, begin: bool) -> bool {
    in_scene != begin
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_surface_admission(ms: u32, lockable: bool) -> bool {
    ms == 0 && !lockable
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_game_basename_index(path: *const u8, size: usize) -> usize {
    if path.is_null() {
        return 0;
    }
    let mut at = 0;
    for i in 0..size {
        let b = unsafe { *path.add(i) };
        if b == b'\\' || b == b'/' {
            at = i + 1;
        }
    }
    at
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_game_read_mode(mode: *const u8, size: usize) -> bool {
    if mode.is_null() || size == 0 {
        return false;
    }
    if unsafe { *mode } != b'r' {
        return false;
    }
    for i in 0..size {
        if unsafe { *mode.add(i) } == b'+' {
            return false;
        }
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_color_backbuffer(f: u32) -> bool {
    matches!(f, 21 | 22)
}
#[repr(C)]
pub struct PresentationPlan {
    focus_window: usize,
    device_window: usize,
    width: u32,
    height: u32,
    format: u32,
    backbuffers: u32,
    multisample: u32,
}
pub type GameGetClient = extern "C" fn(*mut core::ffi::c_void, *mut u32, *mut u32) -> bool;
pub type GameStatus = extern "C" fn(*mut core::ffi::c_void) -> i32;
pub type GameCatalog = extern "C" fn(*mut core::ffi::c_void) -> u32;
pub type GameVoid = extern "C" fn(*mut core::ffi::c_void);
pub type GameUpload = extern "C" fn(*mut core::ffi::c_void, *mut i32, *mut bool);
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_game_create_device_workflow(
    ctx: *mut core::ffi::c_void,
    plan: *mut PresentationPlan,
    present: bool,
    adapter: u32,
    kind: u32,
    get_client: GameGetClient,
    initialize: GameStatus,
    publish: GameStatus,
    invalid: i32,
    unavailable: i32,
) -> i32 {
    if !present || plan.is_null() {
        return unavailable;
    }
    if !a2fo_game_factory_create_admission(adapter, kind, unsafe { (*plan).multisample }, unsafe {
        (*plan).backbuffers
    }) {
        return unavailable;
    }
    if unsafe { (*plan).device_window } == 0 {
        unsafe { (*plan).device_window = (*plan).focus_window };
    }
    if unsafe { (*plan).width } == 0 || unsafe { (*plan).height } == 0 {
        let mut width = 0;
        let mut height = 0;
        if !get_client(ctx, &mut width, &mut height) {
            return invalid;
        }
        unsafe {
            (*plan).width = width;
            (*plan).height = height
        };
    }
    if unsafe { (*plan).format } == 0 {
        unsafe { (*plan).format = 22 };
    }
    if !a2fo_game_color_backbuffer(unsafe { (*plan).format }) {
        return unavailable;
    }
    let hr = initialize(ctx);
    if hr < 0 {
        return hr;
    }
    publish(ctx)
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_create_factory_workflow(
    ctx: *mut core::ffi::c_void,
    native: bool,
    bc: bool,
    catalog: GameCatalog,
    log_failure: GameVoid,
    create: GameCatalog,
) -> bool {
    if !native {
        return false;
    }
    if bc {
        match catalog(ctx) {
            0 => {}
            1 => {
                log_failure(ctx);
                return false;
            }
            _ => return false,
        }
    }
    create(ctx) == 0
}
#[unsafe(no_mangle)]
pub extern "C" fn a2fo_game_load_texture_workflow(
    ctx: *mut core::ffi::c_void,
    load: GameCatalog,
    create: GameStatus,
    copy: GameCatalog,
    upload: GameUpload,
    release: GameVoid,
    publish: GameVoid,
    catalog_failure: GameStatus,
    not_found: i32,
    out_of_memory: i32,
) -> i32 {
    match load(ctx) {
        0 => return not_found,
        1 => return catalog_failure(ctx),
        2 => {}
        _ => return out_of_memory,
    }
    let hr = create(ctx);
    if hr < 0 {
        return hr;
    }
    if copy(ctx) != 0 {
        return out_of_memory;
    }
    let mut hr = 0;
    let mut threw = false;
    upload(ctx, &mut hr, &mut threw);
    if threw {
        return out_of_memory;
    }
    if hr < 0 {
        release(ctx);
        return hr;
    }
    publish(ctx);
    0
}
pub type GameResourceCount = extern "C" fn(*mut core::ffi::c_void) -> u32;
pub type GameResourceInfo =
    extern "C" fn(*mut core::ffi::c_void, u32, *mut u32, *mut u32, *mut i32);
pub type GameResetFail = extern "C" fn(*mut core::ffi::c_void, u32, i32) -> i32;
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_game_reset_workflow(
    ctx: *mut core::ffi::c_void,
    plan: *mut PresentationPlan,
    present: bool,
    recording: bool,
    in_scene: bool,
    old_window: usize,
    old_format: u32,
    get_client: GameGetClient,
    resource_count: GameResourceCount,
    resource_info: GameResourceInfo,
    clear: GameStatus,
    resize: GameUpload,
    commit: GameStatus,
    rebuild: GameStatus,
    fail: GameResetFail,
    invalid_call: i32,
) -> i32 {
    if !present || plan.is_null() || recording || in_scene || unsafe { (*plan).multisample } != 0 {
        return fail(ctx, 0, invalid_call);
    }
    if unsafe { (*plan).device_window } == 0 {
        unsafe { (*plan).device_window = old_window };
    }
    if unsafe { (*plan).device_window } != old_window || unsafe { (*plan).backbuffers } > 1 {
        return fail(ctx, 1, invalid_call);
    }
    if unsafe { (*plan).format } == 0 {
        unsafe { (*plan).format = old_format };
    }
    if !a2fo_game_color_backbuffer(unsafe { (*plan).format }) {
        return fail(ctx, 2, invalid_call);
    }
    if (unsafe { (*plan).width } == 0 || unsafe { (*plan).height } == 0) {
        let mut w = 0;
        let mut h = 0;
        if !get_client(ctx, &mut w, &mut h) {
            return invalid_call;
        }
        unsafe {
            (*plan).width = w;
            (*plan).height = h;
        }
    }
    for i in 0..resource_count(ctx) {
        let mut kind = 0;
        let mut pool = 0;
        let mut refs = 0;
        resource_info(ctx, i, &mut kind, &mut pool, &mut refs);
        if a2fo_game_reset_resource_block(kind, pool, refs) {
            return fail(
                ctx,
                match kind {
                    3 => 3,
                    1 | 2 => 4,
                    _ => 5,
                },
                invalid_call,
            );
        }
    }
    let cleared = clear(ctx);
    if cleared < 0 {
        return cleared;
    }
    let mut hr = 0;
    let mut threw = false;
    resize(ctx, &mut hr, &mut threw);
    if threw {
        return hr;
    }
    if hr >= 0 {
        let committed = commit(ctx);
        if committed < 0 {
            return committed;
        }
    }
    let surfaces = rebuild(ctx);
    if hr < 0 { fail(ctx, 6, hr) } else { surfaces }
}

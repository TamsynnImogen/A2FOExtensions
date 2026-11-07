//! NebulaRenderer path policy. Native CRT classification, filesystem, cache
//! storage and exception handling remain in the C++ adapter.
extern crate alloc;
use alloc::{format, vec::Vec};
use core::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    pub data: *const u8,
    pub size: usize,
}

type Step = unsafe extern "C" fn(*mut c_void, u32, *const u8, usize, u32, *mut Bytes) -> u32;
type Emit = unsafe extern "C" fn(*mut c_void, *const u8, usize);
type Fold = unsafe extern "C" fn(u8) -> u8;

fn extension_end(bytes: &[u8]) -> usize {
    let slash = bytes.iter().rposition(|b| matches!(*b, b'\\' | b'/'));
    let dot = bytes.iter().rposition(|b| *b == b'.');
    match dot {
        Some(dot) if slash.is_none_or(|s| dot > s + 1) => dot,
        _ => bytes.len(),
    }
}

/// # Safety
/// `input` is readable for `size` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_extension_end(input: *const u8, size: usize) -> usize {
    if input.is_null() {
        return 0;
    }
    extension_end(unsafe { core::slice::from_raw_parts(input, size) })
}

/// # Safety
/// `input` is readable for `size` bytes, `output` is writable for at least
/// `size` bytes, and `fold` is the native CRT tolower primitive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_material_key(
    input: *const u8,
    size: usize,
    fold: Option<Fold>,
    output: *mut u8,
) -> usize {
    let Some(fold) = fold else {
        return 0;
    };
    if input.is_null() || output.is_null() {
        return 0;
    }
    let input = unsafe { core::slice::from_raw_parts(input, size) };
    let end = extension_end(input);
    let start = input[..end]
        .iter()
        .rposition(|b| matches!(*b, b'\\' | b'/'))
        .map_or(0, |s| s + 1);
    for (index, byte) in input[start..end].iter().enumerate() {
        unsafe {
            *output.add(index) = fold(*byte);
        }
    }
    end - start
}

#[repr(C)]
pub struct Split {
    pub base_size: usize,
    pub suffix_index: usize,
}

/// # Safety
/// `input` and `suffixes` are borrowed readable bytes. The native fold
/// callback applies CRT tolower and does not throw.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_split_name(
    input: *const u8,
    size: usize,
    suffixes: *const Bytes,
    count: usize,
    fold: Option<Fold>,
) -> Split {
    let empty = Split {
        base_size: 0,
        suffix_index: count,
    };
    let Some(fold) = fold else {
        return empty;
    };
    if input.is_null() || (count != 0 && suffixes.is_null()) {
        return empty;
    }
    let input = unsafe { core::slice::from_raw_parts(input, size) };
    let end = extension_end(input);
    let start = input[..end]
        .iter()
        .rposition(|b| matches!(*b, b'\\' | b'/'))
        .map_or(0, |s| s + 1);
    let key: Vec<u8> = input[start..end]
        .iter()
        .map(|b| unsafe { fold(*b) })
        .collect();
    if key.is_empty() {
        return Split {
            base_size: end,
            suffix_index: count,
        };
    }
    let suffixes = if count == 0 {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(suffixes, count) }
    };
    for (index, suffix) in suffixes.iter().enumerate() {
        if suffix.size == 0 || suffix.data.is_null() || key.len() < suffix.size || end < suffix.size
        {
            continue;
        }
        let suffix = unsafe { core::slice::from_raw_parts(suffix.data, suffix.size) };
        let offset = key.len() - suffix.len();
        if key[offset..]
            .iter()
            .zip(suffix)
            .all(|(left, right)| unsafe { fold(*left) == fold(*right) })
        {
            return Split {
                base_size: end - suffix.len(),
                suffix_index: index,
            };
        }
    }
    Split {
        base_size: end,
        suffix_index: count,
    }
}

type MapStep =
    unsafe extern "C" fn(*mut c_void, u32, *const u8, usize, *mut Bytes, *mut Bytes) -> u32;
type MapEmit = unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize);
type BumpStep =
    unsafe extern "C" fn(*mut c_void, u32, usize, *const u8, usize, usize, *mut Bytes) -> usize;
type DamageStep = unsafe extern "C" fn(
    *mut c_void,
    u32,
    u32,
    usize,
    *const u8,
    usize,
    *mut Bytes,
    *mut f32,
) -> usize;
type IndexStep =
    unsafe extern "C" fn(*mut c_void, u32, usize, usize, *const u8, usize, *mut Bytes) -> usize;
type NativeStep = unsafe extern "C" fn(*mut c_void, u32, *const u8, usize, usize) -> usize;
type NeutralStep = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;
type ListStep = unsafe extern "C" fn(*mut c_void, u32, usize, *mut Bytes) -> usize;
type RootStep = unsafe extern "C" fn(*mut c_void, u32, usize, *mut Bytes) -> usize;
type FileStep = unsafe extern "C" fn(*mut c_void, u32, *mut i64) -> usize;
#[repr(C)]
pub struct ArtReport {
    pub valid: u32,
    pub invalid: u32,
}
type ArtStep = unsafe extern "C" fn(*mut c_void, u32, usize, *mut ArtReport) -> usize;
type SettingStep = unsafe extern "C" fn(*mut c_void, u32) -> usize;
type ClassStep = unsafe extern "C" fn(
    *mut c_void,
    u32,
    usize,
    usize,
    *const u8,
    usize,
    *mut Bytes,
    *mut Bytes,
) -> usize;
#[repr(C)]
pub struct ClassRow {
    pub diffuse: Bytes,
    pub paths: [Bytes; 6],
}
type ClassRegister = unsafe extern "C" fn(*mut c_void, u32, *const ClassRow, usize, usize) -> usize;
#[repr(C)]
pub struct WalkOutput {
    pub child: usize,
    pub sibling: usize,
    pub name: Bytes,
}
type WalkStep = unsafe extern "C" fn(*mut c_void, u32, usize, usize, *mut WalkOutput) -> usize;
type InitStep = unsafe extern "C" fn(*mut c_void, u32, isize) -> isize;
type RaceStep = unsafe extern "C" fn(*mut c_void, u32, usize, *mut Bytes) -> usize;

fn race_call(
    context: *mut c_void,
    step: RaceStep,
    op: u32,
    index: usize,
    output: *mut Bytes,
) -> usize {
    unsafe { step(context, op, index, output) }
}

/// # Safety
/// Native callback reads live SDK field POD, uses exact CRT name comparison,
/// owns map/string storage and captures exceptions before returning.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_race_loaded(
    count: usize,
    has_fields: bool,
    step: Option<RaceStep>,
    context: *mut c_void,
    flag: *const u8,
) {
    let Some(step) = step else {
        return;
    };
    if flag.is_null() {
        return;
    }
    let mut found = false;
    if has_fields {
        for index in 0..count {
            let mut name = Bytes {
                data: core::ptr::null(),
                size: 0,
            };
            race_call(context, step, 0, index, &mut name);
            if failed(flag) {
                return;
            }
            if name.size != b"factionTextureSuffix".len() || name.data.is_null() {
                continue;
            }
            if race_call(context, step, 1, index, core::ptr::null_mut()) == 0 || failed(flag) {
                if failed(flag) {
                    return;
                }
                continue;
            }
            let mut value = Bytes {
                data: core::ptr::null(),
                size: 0,
            };
            race_call(context, step, 2, index, &mut value);
            if failed(flag) {
                return;
            }
            if value.data.is_null() && value.size != 0 {
                continue;
            }
            race_call(context, step, 3, index, core::ptr::null_mut());
            if failed(flag) {
                return;
            }
            found = true;
            break;
        }
    }
    if !found {
        race_call(context, step, 8, 0, core::ptr::null_mut());
        return;
    }
    let mut suffix = Bytes {
        data: core::ptr::null(),
        size: 0,
    };
    if race_call(context, step, 4, 0, &mut suffix) == 0 || failed(flag) || suffix.size == 0 {
        if !failed(flag) {
            race_call(context, step, 8, 0, core::ptr::null_mut());
        }
        return;
    }
    race_call(context, step, 5, 0, core::ptr::null_mut());
    if failed(flag) {
        return;
    }
    if race_call(context, step, 6, 0, core::ptr::null_mut()) != 0 {
        if failed(flag) {
            return;
        }
        race_call(context, step, 7, 0, core::ptr::null_mut());
    }
}

fn init_call(context: *mut c_void, step: InitStep, op: u32, arg: isize) -> isize {
    unsafe { step(context, op, arg) }
}

/// # Safety
/// Callback owns module API/core exports, native signature checks, patching,
/// global storage and logs. It catches exceptions and reports them via flag.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_init(
    valid_api: bool,
    step: Option<InitStep>,
    context: *mut c_void,
    flag: *const u8,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    if !valid_api || flag.is_null() {
        return false;
    }
    init_call(context, step, 0, 0);
    if failed(flag) {
        return false;
    }
    if init_call(context, step, 1, 0) == 0 {
        if !failed(flag) {
            init_call(context, step, 30, 0);
        }
        return false;
    }
    if failed(flag) {
        return false;
    }
    for op in 2..=3 {
        init_call(context, step, op, 0);
        if failed(flag) {
            return false;
        }
    }
    let native_known = init_call(context, step, 4, 0) != 0;
    if failed(flag) {
        return false;
    }
    init_call(context, step, 5, 0);
    if failed(flag) {
        return false;
    }
    for (op, message) in [(6, 31), (7, 32), (8, 33)] {
        if init_call(context, step, op, 0) == 0 {
            if !failed(flag) {
                init_call(context, step, message, 0);
            }
            return false;
        }
        if failed(flag) {
            return false;
        }
    }
    init_call(context, step, 9, 0);
    if failed(flag) {
        return false;
    }
    init_call(context, step, 10, 0);
    if failed(flag) {
        return false;
    }
    let race_message = if init_call(context, step, 11, 0) != 0 {
        if failed(flag) {
            return false;
        }
        if init_call(context, step, 12, 0) != 0 {
            1
        } else {
            2
        }
    } else {
        0
    };
    if failed(flag) {
        return false;
    }
    init_call(context, step, 13, race_message);
    if failed(flag) {
        return false;
    }
    init_call(context, step, 14, 0);
    if failed(flag) {
        return false;
    }
    if init_call(context, step, 15, 0) == 0 {
        if failed(flag) {
            return false;
        }
        if init_call(context, step, 16, 0) != 0 {
            if failed(flag) {
                return false;
            }
            init_call(context, step, 17, 0);
        }
    }
    if failed(flag) {
        return false;
    }
    let status = init_call(context, step, 18, 0);
    if failed(flag) {
        return false;
    }
    init_call(context, step, 19, status);
    if failed(flag) {
        return false;
    }
    let status_message = match status {
        2 => 2,
        1 => 1,
        -1 => -1,
        _ => 0,
    };
    init_call(context, step, 20, status_message);
    if failed(flag) {
        return false;
    }
    let policy = init_call(context, step, 21, 0) as usize;
    if failed(flag) {
        return false;
    }
    let requested = policy & 1 != 0 && native_known && policy & 2 != 0;
    let enabled = init_call(context, step, 22, isize::from(requested)) != 0;
    if failed(flag) {
        return false;
    }
    if requested && !enabled {
        init_call(context, step, 23, 0);
        if failed(flag) {
            return false;
        }
    }
    let neutral = init_call(context, step, 24, isize::from(native_known)) != 0;
    if failed(flag) {
        return false;
    }
    let live_flags = init_call(context, step, 28, 0) as usize;
    if failed(flag) {
        return false;
    }
    if live_flags & 1 != 0 && !neutral {
        init_call(context, step, 25, 0);
        if failed(flag) {
            return false;
        }
    }
    let final_flags = init_call(context, step, 29, 0) as usize;
    if failed(flag) {
        return false;
    }
    let message = if final_flags & 1 == 0 {
        0
    } else if !native_known {
        1
    } else if final_flags & 2 == 0 {
        2
    } else if final_flags & 4 != 0 {
        3
    } else {
        4
    };
    init_call(context, step, 26, message);
    if failed(flag) {
        return false;
    }
    init_call(context, step, 27, 0);
    !failed(flag)
}

/// # Safety
/// Callback owns native core toggle and global storage teardown.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_shutdown(
    step: Option<InitStep>,
    context: *mut c_void,
    flag: *const u8,
) {
    let Some(step) = step else {
        return;
    };
    if flag.is_null() {
        return;
    }
    init_call(context, step, 40, 0);
    if failed(flag) {
        return;
    }
    for op in 41..=57 {
        init_call(context, step, op, 0);
        if failed(flag) {
            return;
        }
    }
}

/// # Safety
/// Nodes and texture handles are opaque native addresses; Rust never
/// dereferences them. Callback owns checked reads and native mesh calls.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_walk(
    root: usize,
    wanted: Bytes,
    find_mode: bool,
    fold: Option<Fold>,
    step: Option<WalkStep>,
    context: *mut c_void,
    flag: *const u8,
) -> usize {
    let (Some(fold), Some(step)) = (fold, step) else {
        return 0;
    };
    if root == 0 || flag.is_null() || (wanted.size != 0 && wanted.data.is_null()) {
        return 0;
    }
    let wanted = if wanted.size == 0 {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(wanted.data, wanted.size) }
    };
    let wanted: Vec<u8> = if find_mode {
        wanted.iter().map(|b| unsafe { fold(*b) }).collect()
    } else {
        Vec::new()
    };
    let mut pending = Vec::new();
    pending.push(root);
    let mut visited = Vec::new();
    while let Some(node) = pending.pop() {
        if visited.len() >= 4096 {
            break;
        }
        if node == 0 || visited.contains(&node) {
            continue;
        }
        visited.push(node);
        let mut output = WalkOutput {
            child: 0,
            sibling: 0,
            name: Bytes {
                data: core::ptr::null(),
                size: 0,
            },
        };
        unsafe { step(context, 0, node, 0, &mut output) };
        if failed(flag) {
            return 0;
        }
        if output.child != 0 {
            pending.push(output.child);
        }
        if output.sibling != 0 {
            pending.push(output.sibling);
        }
        if find_mode {
            if unsafe { step(context, 4, node, 0, &mut output) } != 0 {
                if failed(flag) {
                    return 0;
                }
                if output.name.size == wanted.len()
                    && (output.name.size == 0 || !output.name.data.is_null())
                {
                    let name = if output.name.size == 0 {
                        &[]
                    } else {
                        unsafe { core::slice::from_raw_parts(output.name.data, output.name.size) }
                    };
                    if name == wanted {
                        return node;
                    }
                }
            }
        } else if unsafe { step(context, 1, node, 0, &mut output) } != 0 {
            if failed(flag) {
                return 0;
            }
            let diffuse = unsafe { step(context, 2, node, 0, &mut output) };
            if failed(flag) {
                return 0;
            }
            if diffuse != 0 {
                unsafe { step(context, 3, node, diffuse, &mut output) };
                if failed(flag) {
                    return 0;
                }
            }
        }
        if failed(flag) {
            return 0;
        }
    }
    0
}
struct Indexed {
    diffuse: Vec<u8>,
    resolved: [Vec<u8>; 6],
    declared: [bool; 6],
}
impl Indexed {
    fn new(diffuse: Vec<u8>) -> Self {
        Self {
            diffuse,
            resolved: core::array::from_fn(|_| Vec::new()),
            declared: [false; 6],
        }
    }
}

#[allow(clippy::too_many_arguments)]
fn class_call(
    context: *mut c_void,
    step: ClassStep,
    op: u32,
    index: usize,
    system: usize,
    bytes: &[u8],
    first: *mut Bytes,
    second: *mut Bytes,
) -> usize {
    unsafe {
        step(
            context,
            op,
            index,
            system,
            bytes.as_ptr(),
            bytes.len(),
            first,
            second,
        )
    }
}

fn class_bytes(
    context: *mut c_void,
    step: ClassStep,
    flag: *const u8,
    op: u32,
    index: usize,
    system: usize,
    bytes: &[u8],
) -> Option<Vec<u8>> {
    let mut output = Bytes {
        data: core::ptr::null(),
        size: 0,
    };
    if class_call(
        context,
        step,
        op,
        index,
        system,
        bytes,
        &mut output,
        core::ptr::null_mut(),
    ) == 0
        || failed(flag)
    {
        return None;
    }
    if output.size == 0 {
        return Some(Vec::new());
    }
    if output.data.is_null() {
        return None;
    }
    Some(unsafe { core::slice::from_raw_parts(output.data, output.size) }.to_vec())
}

fn class_resolve_command(
    context: *mut c_void,
    step: ClassStep,
    flag: *const u8,
    command: &[u8],
) -> (bool, Vec<u8>) {
    let Some(requested) = class_bytes(context, step, flag, 6, 0, 0, command) else {
        return (false, Vec::new());
    };
    let resolved = class_bytes(context, step, flag, 7, 0, 0, &requested).unwrap_or_default();
    (true, resolved)
}

fn class_split(
    context: *mut c_void,
    step: ClassStep,
    flag: *const u8,
    diffuse: &[u8],
) -> Option<(Vec<u8>, Vec<u8>)> {
    let mut base = Bytes {
        data: core::ptr::null(),
        size: 0,
    };
    let mut faction = Bytes {
        data: core::ptr::null(),
        size: 0,
    };
    if class_call(context, step, 11, 0, 0, diffuse, &mut base, &mut faction) == 0 || failed(flag) {
        return None;
    }
    if (base.size != 0 && base.data.is_null()) || (faction.size != 0 && faction.data.is_null()) {
        return None;
    }
    let base = if base.size == 0 {
        Vec::new()
    } else {
        unsafe { core::slice::from_raw_parts(base.data, base.size) }.to_vec()
    };
    let faction = if faction.size == 0 {
        Vec::new()
    } else {
        unsafe { core::slice::from_raw_parts(faction.data, faction.size) }.to_vec()
    };
    Some((base, faction))
}

fn class_global(
    context: *mut c_void,
    step: ClassStep,
    flag: *const u8,
    diffuse: &[u8],
    system: usize,
    faction: &[u8],
    fold: Fold,
) -> Vec<u8> {
    const TOKENS: [&[u8]; 6] = [
        b"warp", b"impulse", b"shields", b"life", b"sensor", b"weapons",
    ];
    if system >= 6 || (!faction.is_empty() && diffuse.is_empty()) {
        return Vec::new();
    }
    let Some(suffix) = class_bytes(context, step, flag, 10, 0, 0, &[]) else {
        return Vec::new();
    };
    if suffix.is_empty() || failed(flag) {
        return Vec::new();
    }
    let mut combined = suffix;
    combined.extend_from_slice(TOKENS[system]);
    let base = &diffuse[..extension_end(diffuse)];
    let requested = with_suffix(&with_suffix(base, &combined), faction);
    let key = key_bytes(&requested, fold);
    if class_call(
        context,
        step,
        8,
        0,
        0,
        &key,
        core::ptr::null_mut(),
        core::ptr::null_mut(),
    ) == 0
        || failed(flag)
    {
        return Vec::new();
    }
    class_bytes(context, step, flag, 9, 0, 0, &requested).unwrap_or_default()
}

fn class_global_for_live(
    context: *mut c_void,
    step: ClassStep,
    flag: *const u8,
    diffuse: &[u8],
    system: usize,
    fold: Fold,
) -> Vec<u8> {
    let Some((base, faction)) = class_split(context, step, flag, diffuse) else {
        return Vec::new();
    };
    if failed(flag) {
        return Vec::new();
    }
    if !faction.is_empty() {
        let selected = class_global(context, step, flag, &base, system, &faction, fold);
        if failed(flag) || !selected.is_empty() {
            return selected;
        }
    }
    class_global(context, step, flag, &base, system, &[], fold)
}

fn class_ends_with(key: &[u8], suffix: &[u8], fold: Fold) -> bool {
    if suffix.is_empty() || key.len() < suffix.len() {
        return false;
    }
    key[key.len() - suffix.len()..]
        .iter()
        .zip(suffix)
        .all(|(left, right)| unsafe { fold(*left) == fold(*right) })
}

fn class_span(bytes: &[u8]) -> Bytes {
    Bytes {
        data: bytes.as_ptr(),
        size: bytes.len(),
    }
}

fn class_register_rows(
    context: *mut c_void,
    register: ClassRegister,
    flag: *const u8,
    op: u32,
    rows: &[Indexed],
) {
    let mut texture_count = 0;
    let packed: Vec<ClassRow> = rows
        .iter()
        .map(|row| {
            let paths = core::array::from_fn(|index| {
                if !row.resolved[index].is_empty() {
                    texture_count += 1;
                }
                class_span(&row.resolved[index])
            });
            ClassRow {
                diffuse: class_span(&row.diffuse),
                paths,
            }
        })
        .collect();
    unsafe { register(context, op, packed.as_ptr(), packed.len(), texture_count) };
    let _ = failed(flag);
}

/// # Safety
/// Callback owns native class/ParameterDB and material storage, exact suffix
/// sorting, registration, logging and exception capture. Borrowed bytes are
/// copied before any subsequent native callback can invalidate them.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_register_class(
    ready: bool,
    has_parameter_db: bool,
    fold: Option<Fold>,
    step: Option<ClassStep>,
    register: Option<ClassRegister>,
    context: *mut c_void,
    flag: *const u8,
) {
    let (Some(fold), Some(step), Some(register)) = (fold, step, register) else {
        return;
    };
    if flag.is_null() || !ready {
        return;
    }
    for op in 0..=2 {
        class_call(
            context,
            step,
            op,
            0,
            0,
            &[],
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
        if failed(flag) {
            return;
        }
    }
    if !has_parameter_db {
        class_call(
            context,
            step,
            3,
            0,
            0,
            &[],
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
        if failed(flag) {
            return;
        }
    }
    for op in 4..=5 {
        class_call(
            context,
            step,
            op,
            0,
            0,
            &[],
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
        if failed(flag) {
            return;
        }
    }
    const SUFFIXES: [&str; 6] = [
        "Warp",
        "Impulse",
        "Shields",
        "LifeSupport",
        "Sensors",
        "Weapons",
    ];
    const COMMANDS: [&[u8]; 6] = [
        b"emissiveWarp",
        b"emissiveImpulse",
        b"emissiveShields",
        b"emissiveLifeSupport",
        b"emissiveSensors",
        b"emissiveWeapons",
    ];
    let mut indexed = Vec::new();
    let mut odf_indexed = false;
    for odf_index in 0..64 {
        let command = format!("texture{odf_index}");
        let Some(diffuse) = class_bytes(context, step, flag, 6, 0, 0, command.as_bytes()) else {
            if failed(flag) {
                return;
            }
            continue;
        };
        odf_indexed = true;
        let mut material = Indexed::new(diffuse.clone());
        for (system, suffix) in SUFFIXES.iter().enumerate() {
            let command = format!("emissive{odf_index}{suffix}");
            let (declared, resolved) =
                class_resolve_command(context, step, flag, command.as_bytes());
            if failed(flag) {
                return;
            }
            material.declared[system] = declared;
            material.resolved[system] = if declared {
                resolved
            } else {
                class_global_for_live(context, step, flag, &diffuse, system, fold)
            };
            if failed(flag) {
                return;
            }
        }
        indexed.push(material);
    }
    if !odf_indexed {
        let mut wildcard = Indexed::new(Vec::new());
        let mut any_declared = false;
        let mut has_policy = false;
        for (index, command) in COMMANDS.iter().enumerate() {
            let (declared, resolved) = class_resolve_command(context, step, flag, command);
            if failed(flag) {
                return;
            }
            if !resolved.is_empty() {
                has_policy = true;
            }
            any_declared |= declared;
            wildcard.resolved[index] = resolved;
        }
        if any_declared {
            if has_policy {
                class_register_rows(context, register, flag, 0, &[wildcard]);
            }
            return;
        }
    }
    let enabled =
        class_bytes(context, step, flag, 10, 0, 0, &[]).is_some_and(|suffix| !suffix.is_empty());
    if failed(flag) {
        return;
    }
    if enabled {
        let mut indexed_keys: Vec<Vec<u8>> = indexed
            .iter()
            .map(|material| key_bytes(&material.diffuse, fold))
            .collect();
        let count = class_call(
            context,
            step,
            12,
            0,
            0,
            &[],
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
        if failed(flag) {
            return;
        }
        for index in 0..count {
            let Some(diffuse) = class_bytes(context, step, flag, 13, index, 0, &[]) else {
                if failed(flag) {
                    return;
                }
                continue;
            };
            let key = key_bytes(&diffuse, fold);
            if key.is_empty() || indexed_keys.contains(&key) {
                continue;
            }
            indexed_keys.push(key);
            let mut material = Indexed::new(diffuse.clone());
            for system in 0..6 {
                material.resolved[system] =
                    class_global_for_live(context, step, flag, &diffuse, system, fold);
                if failed(flag) {
                    return;
                }
            }
            indexed.push(material);
        }
    }
    if !indexed.is_empty() {
        let mut registered: Vec<Vec<u8>> = indexed
            .iter()
            .map(|material| key_bytes(&material.diffuse, fold))
            .filter(|key| !key.is_empty())
            .collect();
        let suffix_count = class_call(
            context,
            step,
            14,
            0,
            0,
            &[],
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
        if failed(flag) {
            return;
        }
        let mut factions = Vec::new();
        for index in 0..suffix_count {
            let Some(suffix) = class_bytes(context, step, flag, 15, index, 0, &[]) else {
                if failed(flag) {
                    return;
                }
                continue;
            };
            factions.push(suffix);
        }
        let base_count = indexed.len();
        for material_index in 0..base_count {
            let base = &indexed[material_index];
            let key = key_bytes(&base.diffuse, fold);
            if key.is_empty() {
                continue;
            }
            let Some((_, existing_faction)) = class_split(context, step, flag, &base.diffuse)
            else {
                if failed(flag) {
                    return;
                }
                continue;
            };
            if !existing_faction.is_empty() {
                continue;
            }
            let base = Indexed {
                diffuse: base.diffuse.clone(),
                resolved: base.resolved.clone(),
                declared: base.declared,
            };
            for faction in &factions {
                if faction.is_empty() || class_ends_with(&key, faction, fold) {
                    continue;
                }
                let diffuse = with_suffix(&base.diffuse, faction);
                let variant_key = key_bytes(&diffuse, fold);
                if variant_key.is_empty() || registered.contains(&variant_key) {
                    continue;
                }
                registered.push(variant_key);
                let mut variant = Indexed {
                    diffuse,
                    resolved: base.resolved.clone(),
                    declared: base.declared,
                };
                if enabled {
                    for system in 0..6 {
                        if variant.declared[system] {
                            continue;
                        }
                        let selected =
                            class_global(context, step, flag, &base.diffuse, system, faction, fold);
                        if failed(flag) {
                            return;
                        }
                        if !selected.is_empty() {
                            variant.resolved[system] = selected;
                        }
                    }
                }
                indexed.push(variant);
            }
        }
    }
    indexed.retain(|row| row.resolved.iter().any(|path| !path.is_empty()));
    if (odf_indexed || !indexed.is_empty()) && !indexed.is_empty() {
        class_register_rows(context, register, flag, 1, &indexed);
    }
}

/// # Safety
/// Native callback owns Win32 INI operations and selected-root storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_load_neutral_policy(
    step: Option<SettingStep>,
    context: *mut c_void,
    flag: *const u8,
) {
    let Some(step) = step else {
        return;
    };
    if flag.is_null() {
        return;
    }
    unsafe { step(context, 0) };
    if failed(flag) {
        return;
    }
    if unsafe { step(context, 1) } == 0 || failed(flag) {
        return;
    }
    unsafe { step(context, 2) };
}

/// # Safety
/// Native callback owns module API directory reads, file strings, XML parser
/// wrapper, disabled output and logs; exceptions are caught in the adapter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_load_native_bump(
    step: Option<SettingStep>,
    context: *mut c_void,
    flag: *const u8,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    if flag.is_null() {
        return false;
    }
    if unsafe { step(context, 0) } != 0 {
        if failed(flag) {
            return false;
        }
        if unsafe { step(context, 1) } != 0 {
            if failed(flag) {
                return false;
            }
            if unsafe { step(context, 2) } != 0 {
                return !failed(flag);
            }
        }
        if failed(flag) {
            return false;
        }
    }
    if unsafe { step(context, 3) } == 0 || failed(flag) {
        return false;
    }
    (unsafe { step(context, 4) }) != 0 && !failed(flag)
}

/// # Safety
/// Callback owns native root/config strings, file reads, Rust-backed ART
/// parser wrapper and logs. It captures native exceptions before return.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_load_art(
    step: Option<ArtStep>,
    context: *mut c_void,
    flag: *const u8,
) {
    let Some(step) = step else {
        return;
    };
    if flag.is_null() {
        return;
    }
    unsafe { step(context, 0, 0, core::ptr::null_mut()) };
    if failed(flag) {
        return;
    }
    let count = unsafe { step(context, 1, 0, core::ptr::null_mut()) };
    if failed(flag) {
        return;
    }
    for index in 0..count {
        if unsafe { step(context, 2, index, core::ptr::null_mut()) } == 0 {
            if failed(flag) {
                return;
            }
            continue;
        }
        if failed(flag) {
            return;
        }
        let mut report = ArtReport {
            valid: 0,
            invalid: 0,
        };
        unsafe { step(context, 3, index, &mut report) };
        if failed(flag) {
            return;
        }
        if report.valid != 0 {
            unsafe { step(context, 4, index, core::ptr::null_mut()) };
            if failed(flag) {
                return;
            }
        }
        if report.invalid != 0 {
            unsafe { step(context, 5, index, core::ptr::null_mut()) };
            if failed(flag) {
                return;
            }
        }
    }
    let any = unsafe { step(context, 6, 0, core::ptr::null_mut()) } != 0;
    if failed(flag) {
        return;
    }
    if any {
        unsafe { step(context, 7, 0, core::ptr::null_mut()) };
    }
}

/// # Safety
/// Callback owns native ifstream/ostringstream and path storage. It catches
/// exceptions and reports them before returning across the Rust boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_read_text(
    step: Option<FileStep>,
    context: *mut c_void,
    flag: *const u8,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    if flag.is_null() {
        return false;
    }
    if unsafe { step(context, 0, core::ptr::null_mut()) } == 0 || failed(flag) {
        return false;
    }
    let mut size = 0_i64;
    unsafe { step(context, 1, &mut size) };
    if failed(flag) {
        return false;
    }
    if !(0..=2 * 1024 * 1024).contains(&size) {
        unsafe { step(context, 2, core::ptr::null_mut()) };
        return false;
    }
    unsafe { step(context, 3, core::ptr::null_mut()) };
    if failed(flag) {
        return false;
    }
    unsafe { step(context, 4, core::ptr::null_mut()) };
    if failed(flag) {
        return false;
    }
    (unsafe { step(context, 5, core::ptr::null_mut()) }) != 0 && !failed(flag)
}

/// # Safety
/// Callback reads live module API roots and owns native vector storage. It
/// catches native exceptions and reports them through `flag`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_cache_roots(
    available: bool,
    step: Option<RootStep>,
    context: *mut c_void,
    flag: *const u8,
) {
    let Some(step) = step else {
        return;
    };
    if flag.is_null() || !available {
        return;
    }
    let count = unsafe { step(context, 0, 0, core::ptr::null_mut()) };
    if failed(flag) {
        return;
    }
    for index in 0..count {
        let mut root = Bytes {
            data: core::ptr::null(),
            size: 0,
        };
        if unsafe { step(context, 1, index, &mut root) } == 0 || failed(flag) {
            if failed(flag) {
                return;
            }
            continue;
        }
        if root.size == 0 || root.data.is_null() {
            continue;
        }
        let root = unsafe { core::slice::from_raw_parts(root.data, root.size) }.to_vec();
        unsafe {
            step(
                context,
                2,
                index,
                &mut Bytes {
                    data: root.as_ptr(),
                    size: root.len(),
                },
            )
        };
        if failed(flag) {
            return;
        }
    }
}

fn list_call(
    context: *mut c_void,
    step: ListStep,
    op: u32,
    index: usize,
    output: *mut Bytes,
) -> usize {
    unsafe { step(context, op, index, output) }
}

/// # Safety
/// Begin/end are native string-vector addresses, not Rust pointers. Callback
/// owns row reads, vector storage and engine deletion, and reports caught
/// native copy failures separately from fatal delete failures.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_parameter_list(
    begin: usize,
    end: usize,
    preserve_empty: bool,
    step: Option<ListStep>,
    context: *mut c_void,
    copy_failed: *const u8,
    fatal: *const u8,
) {
    let Some(step) = step else {
        return;
    };
    if copy_failed.is_null() || fatal.is_null() {
        return;
    }
    let count = if end >= begin && (end - begin).is_multiple_of(core::mem::size_of::<usize>()) {
        (end - begin) / core::mem::size_of::<usize>()
    } else {
        0
    };
    if count <= 256 {
        let readable = count == 0 || list_call(context, step, 0, count, core::ptr::null_mut()) != 0;
        if unsafe { *fatal } != 0 {
            return;
        }
        if readable {
            list_call(context, step, 1, count, core::ptr::null_mut());
            if unsafe { *copy_failed } == 0 {
                for index in 0..count {
                    let mut value = Bytes {
                        data: core::ptr::null(),
                        size: 0,
                    };
                    list_call(context, step, 2, index, &mut value);
                    if unsafe { *copy_failed } != 0 {
                        break;
                    }
                    if preserve_empty || value.size != 0 {
                        list_call(context, step, 3, index, core::ptr::null_mut());
                        if unsafe { *copy_failed } != 0 {
                            break;
                        }
                    }
                }
            }
            if unsafe { *copy_failed } != 0 {
                list_call(context, step, 4, 0, core::ptr::null_mut());
            }
        }
        let engine_delete = list_call(context, step, 7, 0, core::ptr::null_mut()) != 0;
        if unsafe { *fatal } != 0 {
            return;
        }
        if engine_delete {
            for index in 0..count {
                list_call(context, step, 5, index, core::ptr::null_mut());
                if unsafe { *fatal } != 0 {
                    return;
                }
            }
            list_call(context, step, 6, 0, core::ptr::null_mut());
        }
    }
}

fn neutral_call(context: *mut c_void, step: NeutralStep, op: u32, value: usize) -> usize {
    unsafe { step(context, op, value) }
}

/// # Safety
/// Callback owns the native checked patch, graphics pointer, memory reads,
/// memory writes and logs; it captures native exceptions before returning.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_arm_neutral(
    requested: bool,
    disabled: bool,
    core: bool,
    fast: bool,
    step: Option<NeutralStep>,
    context: *mut c_void,
    flag: *const u8,
) -> bool {
    let Some(step) = step else {
        return false;
    };
    if flag.is_null() || !requested || !disabled {
        return false;
    }
    if !core || !fast {
        neutral_call(context, step, 0, 0);
        return false;
    }
    if neutral_call(context, step, 1, 0) != 0 {
        if failed(flag) {
            return false;
        }
        neutral_call(context, step, 2, 0);
        return !failed(flag);
    }
    if failed(flag) {
        return false;
    }
    let current = neutral_call(context, step, 3, 0);
    if failed(flag) {
        return false;
    }
    if current == 0xff {
        neutral_call(context, step, 4, 0);
        return false;
    }
    if current != 0 && neutral_call(context, step, 5, 0) == 0 {
        if failed(flag) {
            return false;
        }
        neutral_call(context, step, 6, 0);
        return false;
    }
    if failed(flag) {
        return false;
    }
    let updated = neutral_call(context, step, 7, 0);
    if failed(flag) {
        return false;
    }
    neutral_call(context, step, 8, current);
    !failed(flag) && updated == 0
}

fn native_call(context: *mut c_void, step: NativeStep, op: u32, bytes: &[u8], arg: usize) -> usize {
    unsafe { step(context, op, bytes.as_ptr(), bytes.len(), arg) }
}

fn asset_path(name: &[u8], extension: &[u8], fold: Fold) -> Vec<u8> {
    let mut base = name[..extension_end(name)].to_vec();
    for byte in &mut base {
        if *byte == b'/' {
            *byte = b'\\';
        }
    }
    let prefixed = base.len() >= b"textures\\".len()
        && base
            .iter()
            .take(9)
            .zip(b"textures\\")
            .all(|(a, b)| unsafe { fold(*a) == *b });
    let mut result = Vec::new();
    if !prefixed {
        result.extend_from_slice(b"Textures\\");
    }
    result.extend_from_slice(&base);
    result.extend_from_slice(extension);
    result
}

/// # Safety
/// Name/extension spans remain readable. Callback owns native cache and
/// texture-file existence API and catches all native exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_native_exists(
    name: Bytes,
    extension: Bytes,
    available: bool,
    fold: Option<Fold>,
    step: Option<NativeStep>,
    context: *mut c_void,
    flag: *const u8,
) -> bool {
    let (Some(fold), Some(step)) = (fold, step) else {
        return false;
    };
    if flag.is_null()
        || !available
        || name.size == 0
        || name.data.is_null()
        || extension.data.is_null()
    {
        return false;
    }
    let name = unsafe { core::slice::from_raw_parts(name.data, name.size) };
    let extension = unsafe { core::slice::from_raw_parts(extension.data, extension.size) };
    let path = asset_path(name, extension, fold);
    let cached = native_call(context, step, 0, &path, 0);
    if failed(flag) {
        return false;
    }
    if cached != 0 {
        return cached == 2;
    }
    let exists = native_call(context, step, 1, &path, 0) != 0;
    if failed(flag) {
        return false;
    }
    native_call(context, step, 2, &path, usize::from(exists));
    !failed(flag) && exists
}

/// # Safety
/// Request bytes remain readable. Callback performs native texture lookup,
/// raw flag reads and preserves pointer/handle ownership in C++.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_native_variant(
    requested: Bytes,
    eligible: bool,
    step: Option<NativeStep>,
    context: *mut c_void,
    flag: *const u8,
) -> usize {
    let Some(step) = step else {
        return 0;
    };
    if flag.is_null() || !eligible || requested.size == 0 || requested.data.is_null() {
        return 0;
    }
    let requested = unsafe { core::slice::from_raw_parts(requested.data, requested.size) };
    let base = requested[..extension_end(requested)].to_vec();
    if native_call(context, step, 0, &base, 0) == 0 {
        if failed(flag) {
            return 0;
        }
        if native_call(context, step, 0, &base, 1) == 0 || failed(flag) {
            return 0;
        }
    }
    if failed(flag) {
        return 0;
    }
    let flags = native_call(context, step, 1, &[], 0);
    if failed(flag) {
        return 0;
    }
    // Armada 2 v1.93 requires 0x200 for the native bump texture slot.
    native_call(context, step, 2, &base, flags | 0x200)
}

fn index_call(
    context: *mut c_void,
    step: IndexStep,
    op: u32,
    root: usize,
    directory: usize,
    bytes: &[u8],
    output: *mut Bytes,
) -> usize {
    unsafe {
        step(
            context,
            op,
            root,
            directory,
            bytes.as_ptr(),
            bytes.len(),
            output,
        )
    }
}

/// # Safety
/// Suffix bytes remain readable for the call. The callback owns Win32 search
/// handles and native key storage and catches native exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_index_loose(
    emissive: Bytes,
    specular: Bytes,
    root_count: usize,
    fold: Option<Fold>,
    step: Option<IndexStep>,
    context: *mut c_void,
    flag: *const u8,
) {
    let (Some(fold), Some(step)) = (fold, step) else {
        return;
    };
    if flag.is_null()
        || (emissive.size != 0 && emissive.data.is_null())
        || (specular.size != 0 && specular.data.is_null())
    {
        return;
    }
    if emissive.size == 0 && specular.size == 0 {
        return;
    }
    let mut suffixes = [Vec::new(), Vec::new()];
    for (index, span) in [emissive, specular].iter().enumerate() {
        if span.size != 0 {
            let input = unsafe { core::slice::from_raw_parts(span.data, span.size) };
            suffixes[index] = input.iter().map(|b| unsafe { fold(*b) }).collect();
        }
    }
    for root in 0..root_count {
        for directory in 0..4 {
            if index_call(
                context,
                step,
                0,
                root,
                directory,
                &[],
                core::ptr::null_mut(),
            ) == 0
            {
                if failed(flag) {
                    return;
                }
                continue;
            }
            if failed(flag) {
                return;
            }
            loop {
                let mut name = Bytes {
                    data: core::ptr::null(),
                    size: 0,
                };
                let kind = index_call(context, step, 1, root, directory, &[], &mut name);
                if failed(flag) {
                    return;
                }
                if kind == 1 && (name.size == 0 || !name.data.is_null()) {
                    let filename = if name.size == 0 {
                        &[]
                    } else {
                        unsafe { core::slice::from_raw_parts(name.data, name.size) }
                    };
                    let key = key_bytes(filename, fold);
                    let mapped = suffixes.iter().any(|suffix| {
                        !suffix.is_empty() && key.windows(suffix.len()).any(|part| part == suffix)
                    });
                    if !key.is_empty() && mapped {
                        index_call(
                            context,
                            step,
                            4,
                            root,
                            directory,
                            &key,
                            core::ptr::null_mut(),
                        );
                        if failed(flag) {
                            return;
                        }
                    }
                }
                if index_call(
                    context,
                    step,
                    2,
                    root,
                    directory,
                    &[],
                    core::ptr::null_mut(),
                ) == 0
                {
                    break;
                }
                if failed(flag) {
                    return;
                }
            }
            index_call(
                context,
                step,
                3,
                root,
                directory,
                &[],
                core::ptr::null_mut(),
            );
            if failed(flag) {
                return;
            }
        }
    }
    index_call(context, step, 5, 0, 0, &[], core::ptr::null_mut());
}

// Mirrors the native POD callback so each ParameterDB read keeps its timing.
#[allow(clippy::too_many_arguments)]
fn damage_call(
    context: *mut c_void,
    step: DamageStep,
    op: u32,
    system: u32,
    index: usize,
    bytes: &[u8],
    output: *mut Bytes,
    number: *mut f32,
) -> usize {
    unsafe {
        step(
            context,
            op,
            system,
            index,
            bytes.as_ptr(),
            bytes.len(),
            output,
            number,
        )
    }
}

fn damage_string(
    context: *mut c_void,
    step: DamageStep,
    flag: *const u8,
    op: u32,
    system: u32,
    index: usize,
    input: &[u8],
) -> Option<Vec<u8>> {
    let mut output = Bytes {
        data: core::ptr::null(),
        size: 0,
    };
    if damage_call(
        context,
        step,
        op,
        system,
        index,
        input,
        &mut output,
        core::ptr::null_mut(),
    ) == 0
        || failed(flag)
    {
        return None;
    }
    if output.size == 0 {
        return Some(Vec::new());
    }
    if output.data.is_null() {
        return None;
    }
    Some(unsafe { core::slice::from_raw_parts(output.data, output.size) }.to_vec())
}

/// # Safety
/// The callback keeps native ParameterDB, class-node and descriptor storage,
/// captures exceptions, and returns borrowed string bytes until its next call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_damage(
    step: Option<DamageStep>,
    context: *mut c_void,
    flag: *const u8,
) {
    let Some(step) = step else {
        return;
    };
    if flag.is_null() {
        return;
    }
    let mut scale = 1.0;
    damage_call(
        context,
        step,
        0,
        0,
        0,
        &[],
        core::ptr::null_mut(),
        &mut scale,
    );
    if failed(flag) {
        return;
    }
    if !scale.is_finite() || scale <= 0.0 {
        scale = 1.0;
    }
    let mut threshold = 0.1;
    damage_call(
        context,
        step,
        1,
        0,
        0,
        &[],
        core::ptr::null_mut(),
        &mut threshold,
    );
    if failed(flag) || !threshold.is_finite() || threshold <= 0.0 || threshold > 1.0 {
        return;
    }
    let mut preview = 0.0;
    damage_call(
        context,
        step,
        2,
        0,
        0,
        &[],
        core::ptr::null_mut(),
        &mut preview,
    );
    if failed(flag) {
        return;
    }
    let preview = preview != 0.0;
    damage_call(
        context,
        step,
        14,
        u32::from(preview),
        0,
        &[],
        core::ptr::null_mut(),
        &mut scale,
    );
    if failed(flag) {
        return;
    }
    let mut global = Vec::new();
    for index in 1..=64 {
        let command = format!("scorchTexture{index}");
        let Some(requested) = damage_string(context, step, flag, 3, 0, index, command.as_bytes())
        else {
            if failed(flag) {
                return;
            }
            continue;
        };
        if let Some(resolved) = damage_string(context, step, flag, 4, 0, index, &requested)
            && !resolved.is_empty()
        {
            global.push(resolved);
        }
        if failed(flag) {
            return;
        }
    }
    const PREFIXES: [&str; 6] = [
        "sensors",
        "engines",
        "weapons",
        "lifeSupport",
        "shieldGenerator",
        "hull",
    ];
    const TARGETS: [&[u8]; 6] = [
        b"sensorsTargetHardpoints",
        b"enginesTargetHardpoints",
        b"weaponsTargetHardpoints",
        b"lifeSupportTargetHardpoints",
        b"shieldGeneratorTargetHardpoints",
        b"hullTargetHardpoints",
    ];
    let mut pending = 0;
    for system in 0..6 {
        let mut explicit = false;
        for index in 1..=64 {
            let base = format!("{}Scorch{index}", PREFIXES[system]);
            let Some(requested) = damage_string(
                context,
                step,
                flag,
                3,
                system as u32,
                index,
                base.as_bytes(),
            ) else {
                if failed(flag) {
                    return;
                }
                continue;
            };
            explicit = true;
            let hardpoint_command = format!("{base}Hardpoint");
            let Some(hardpoint) = damage_string(
                context,
                step,
                flag,
                3,
                system as u32,
                index,
                hardpoint_command.as_bytes(),
            ) else {
                if failed(flag) {
                    return;
                }
                continue;
            };
            let node = damage_call(
                context,
                step,
                5,
                system as u32,
                index,
                &hardpoint,
                core::ptr::null_mut(),
                core::ptr::null_mut(),
            ) != 0;
            if failed(flag) {
                return;
            }
            let resolved = damage_string(context, step, flag, 4, system as u32, index, &requested);
            if failed(flag) {
                return;
            }
            if !node || resolved.as_ref().is_none_or(Vec::is_empty) {
                damage_call(
                    context,
                    step,
                    11,
                    system as u32,
                    index,
                    base.as_bytes(),
                    core::ptr::null_mut(),
                    core::ptr::null_mut(),
                );
                if failed(flag) {
                    return;
                }
                continue;
            }
            damage_call(
                context,
                step,
                8,
                system as u32,
                index,
                base.as_bytes(),
                core::ptr::null_mut(),
                core::ptr::null_mut(),
            );
            if failed(flag) {
                return;
            }
            pending += 1;
        }
        if explicit || global.is_empty() {
            continue;
        }
        let count = damage_call(
            context,
            step,
            6,
            system as u32,
            0,
            TARGETS[system],
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
        if failed(flag) {
            return;
        }
        for index in 0..count.min(64) {
            let Some(target) = damage_string(context, step, flag, 7, system as u32, index, &[])
            else {
                if failed(flag) {
                    return;
                }
                continue;
            };
            if damage_call(
                context,
                step,
                5,
                system as u32,
                index,
                &target,
                core::ptr::null_mut(),
                core::ptr::null_mut(),
            ) == 0
            {
                continue;
            }
            if failed(flag) {
                return;
            }
            damage_call(
                context,
                step,
                10,
                system as u32,
                index,
                &global[index % global.len()],
                core::ptr::null_mut(),
                core::ptr::null_mut(),
            );
            if failed(flag) {
                return;
            }
            damage_call(
                context,
                step,
                9,
                system as u32,
                index,
                &[],
                core::ptr::null_mut(),
                core::ptr::null_mut(),
            );
            if failed(flag) {
                return;
            }
            pending += 1;
        }
    }
    if pending != 0 {
        damage_call(
            context,
            step,
            12,
            0,
            pending,
            &[],
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
    }
}

fn logo_suffix(name: &[u8], suffix: &[u8]) -> Vec<u8> {
    with_suffix(name, suffix)
}

/// # Safety
/// The callback owns native ParameterDB, descriptor, texture and registration
/// storage. It catches exceptions and exposes borrowed strings only until its
/// next invocation. Numeric CRT/x87 conversion remains in the native adapter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_logos(
    step: Option<DamageStep>,
    context: *mut c_void,
    flag: *const u8,
) {
    let Some(step) = step else {
        return;
    };
    if flag.is_null() {
        return;
    }
    let mut scale = 1.0;
    damage_call(
        context,
        step,
        0,
        0,
        0,
        &[],
        core::ptr::null_mut(),
        &mut scale,
    );
    if failed(flag) {
        return;
    }
    if !scale.is_finite() || scale <= 0.0 {
        scale = 1.0;
    }
    damage_call(
        context,
        step,
        1,
        0,
        0,
        &[],
        core::ptr::null_mut(),
        &mut scale,
    );
    if failed(flag) {
        return;
    }
    let mut logo_names_loaded = false;
    let mut logo_names_count = 0;
    let mut pending = 0;
    for index in 1..=64 {
        let base = format!("logoDecal{index}");
        let hardpoint_command = format!("{base}Hardpoint");
        let Some(hardpoint) = damage_string(
            context,
            step,
            flag,
            2,
            0,
            index,
            hardpoint_command.as_bytes(),
        ) else {
            if failed(flag) {
                return;
            }
            continue;
        };
        if damage_call(
            context,
            step,
            3,
            0,
            index,
            &hardpoint,
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        ) == 0
        {
            if failed(flag) {
                return;
            }
            damage_call(
                context,
                step,
                4,
                0,
                index,
                base.as_bytes(),
                core::ptr::null_mut(),
                core::ptr::null_mut(),
            );
            if failed(flag) {
                return;
            }
            continue;
        }
        if failed(flag) {
            return;
        }
        damage_call(
            context,
            step,
            5,
            0,
            index,
            base.as_bytes(),
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
        if failed(flag) {
            return;
        }
        damage_call(
            context,
            step,
            6,
            0,
            index,
            base.as_bytes(),
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
        if failed(flag) {
            return;
        }
        let mut flip = 0.0;
        damage_call(
            context,
            step,
            7,
            0,
            index,
            base.as_bytes(),
            core::ptr::null_mut(),
            &mut flip,
        );
        if failed(flag) {
            return;
        }
        damage_call(
            context,
            step,
            16,
            u32::from(flip != 0.0),
            index,
            &[],
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
        if failed(flag) {
            return;
        }
        let suffix_command = format!("{base}Suffix");
        let suffix = damage_string(context, step, flag, 8, 0, index, suffix_command.as_bytes())
            .unwrap_or_default();
        if failed(flag) {
            return;
        }
        if !logo_names_loaded {
            logo_names_count = damage_call(
                context,
                step,
                9,
                0,
                0,
                b"logoFileNames",
                core::ptr::null_mut(),
                core::ptr::null_mut(),
            );
            if failed(flag) {
                return;
            }
            logo_names_loaded = true;
        }
        if !suffix.is_empty() && logo_names_count == 0 {
            damage_call(
                context,
                step,
                15,
                0,
                index,
                base.as_bytes(),
                core::ptr::null_mut(),
                core::ptr::null_mut(),
            );
            if failed(flag) {
                return;
            }
            continue;
        }
        if logo_names_count != 0 {
            let mut resolved_count = 0;
            for logo_index in 0..logo_names_count {
                let Some(name) = damage_string(context, step, flag, 10, 0, logo_index, &[]) else {
                    if failed(flag) {
                        return;
                    }
                    continue;
                };
                let resolved = if name.is_empty() {
                    Vec::new()
                } else {
                    let requested = logo_suffix(&name, &suffix);
                    damage_string(context, step, flag, 11, 0, logo_index, &requested)
                        .unwrap_or_default()
                };
                if failed(flag) {
                    return;
                }
                if !resolved.is_empty() {
                    resolved_count += 1;
                }
                damage_call(
                    context,
                    step,
                    12,
                    0,
                    logo_index,
                    &resolved,
                    core::ptr::null_mut(),
                    core::ptr::null_mut(),
                );
                if failed(flag) {
                    return;
                }
            }
            if !suffix.is_empty() && resolved_count == 0 {
                damage_call(
                    context,
                    step,
                    17,
                    0,
                    index,
                    base.as_bytes(),
                    core::ptr::null_mut(),
                    core::ptr::null_mut(),
                );
                if failed(flag) {
                    return;
                }
                continue;
            }
            if resolved_count == 0 {
                damage_call(
                    context,
                    step,
                    18,
                    0,
                    index,
                    &[],
                    core::ptr::null_mut(),
                    core::ptr::null_mut(),
                );
                if failed(flag) {
                    return;
                }
            }
        }
        damage_call(
            context,
            step,
            13,
            0,
            index,
            &[],
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
        if failed(flag) {
            return;
        }
        pending += 1;
    }
    if pending != 0 {
        damage_call(
            context,
            step,
            14,
            0,
            pending,
            &[],
            core::ptr::null_mut(),
            core::ptr::null_mut(),
        );
    }
}

fn bump_call(
    context: *mut c_void,
    step: BumpStep,
    op: u32,
    index: usize,
    bytes: &[u8],
    arg: usize,
    output: *mut Bytes,
) -> usize {
    unsafe { step(context, op, index, bytes.as_ptr(), bytes.len(), arg, output) }
}

/// # Safety
/// Native callback owns mesh/texture pointers and catches exceptions. The
/// returned diffuse name is borrowed until the next callback; Rust copies it.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_apply_bump(
    suffix_enabled: bool,
    supported: bool,
    material_count: usize,
    step: Option<BumpStep>,
    context: *mut c_void,
    flag: *const u8,
) -> u32 {
    let Some(step) = step else {
        return 0;
    };
    if flag.is_null() || !suffix_enabled || !supported {
        return 0;
    }
    let mut applied = 0;
    for index in 0..material_count {
        let mut diffuse = Bytes {
            data: core::ptr::null(),
            size: 0,
        };
        if bump_call(context, step, 0, index, &[], 0, &mut diffuse) == 0 || failed(flag) {
            if failed(flag) {
                return applied;
            }
            continue;
        }
        if diffuse.size != 0 && diffuse.data.is_null() {
            return applied;
        }
        let diffuse = if diffuse.size == 0 {
            Vec::new()
        } else {
            unsafe { core::slice::from_raw_parts(diffuse.data, diffuse.size) }.to_vec()
        };
        if bump_call(context, step, 1, index, &[], 0, core::ptr::null_mut()) != 0 || failed(flag) {
            if failed(flag) {
                return applied;
            }
            continue;
        }
        let mut suffix = Bytes {
            data: core::ptr::null(),
            size: 0,
        };
        if bump_call(context, step, 10, index, &[], 0, &mut suffix) == 0
            || failed(flag)
            || (suffix.size != 0 && suffix.data.is_null())
        {
            return applied;
        }
        let suffix = if suffix.size == 0 {
            Vec::new()
        } else {
            unsafe { core::slice::from_raw_parts(suffix.data, suffix.size) }.to_vec()
        };
        let request = with_suffix(&diffuse, &suffix);
        if bump_call(context, step, 2, index, &request, 0, core::ptr::null_mut()) == 0
            || failed(flag)
        {
            if failed(flag) {
                return applied;
            }
            continue;
        }
        let flags = bump_call(context, step, 3, index, &[], 0, core::ptr::null_mut());
        if failed(flag) {
            return applied;
        }
        if bump_call(
            context,
            step,
            4,
            index,
            &[],
            flags | 2,
            core::ptr::null_mut(),
        ) == 0
            || failed(flag)
        {
            if failed(flag) {
                return applied;
            }
            continue;
        }
        bump_call(context, step, 5, index, &[], 0, core::ptr::null_mut());
        if failed(flag) {
            return applied;
        }
        let failed_update = bump_call(context, step, 6, index, &[], 0, core::ptr::null_mut()) != 0;
        if failed(flag) {
            return applied;
        }
        if failed_update {
            bump_call(context, step, 7, index, &[], 0, core::ptr::null_mut());
            if failed(flag) {
                return applied;
            }
            bump_call(context, step, 8, index, &[], flags, core::ptr::null_mut());
            if failed(flag) {
                return applied;
            }
            bump_call(context, step, 9, index, &[], 0, core::ptr::null_mut());
            if failed(flag) {
                return applied;
            }
            continue;
        }
        applied += 1;
    }
    applied
}

fn key_bytes(input: &[u8], fold: Fold) -> Vec<u8> {
    let end = extension_end(input);
    let start = input[..end]
        .iter()
        .rposition(|b| matches!(*b, b'\\' | b'/'))
        .map_or(0, |s| s + 1);
    input[start..end]
        .iter()
        .map(|b| unsafe { fold(*b) })
        .collect()
}

fn with_suffix(name: &[u8], suffix: &[u8]) -> Vec<u8> {
    if suffix.is_empty() {
        return name.to_vec();
    }
    let at = extension_end(name);
    let mut result = Vec::with_capacity(name.len() + suffix.len());
    result.extend_from_slice(&name[..at]);
    result.extend_from_slice(suffix);
    result.extend_from_slice(&name[at..]);
    result
}

fn map_call(
    context: *mut c_void,
    step: MapStep,
    op: u32,
    bytes: &[u8],
    first: *mut Bytes,
    second: *mut Bytes,
) -> u32 {
    unsafe { step(context, op, bytes.as_ptr(), bytes.len(), first, second) }
}

fn map_resolve(
    context: *mut c_void,
    step: MapStep,
    flag: *const u8,
    name: &[u8],
    faction: &[u8],
    fold: Fold,
) -> Option<Vec<u8>> {
    if !faction.is_empty() && name.is_empty() {
        return None;
    }
    let mut suffix = Bytes {
        data: core::ptr::null(),
        size: 0,
    };
    if map_call(context, step, 3, &[], &mut suffix, core::ptr::null_mut()) == 0
        || failed(flag)
        || (suffix.size != 0 && suffix.data.is_null())
    {
        return None;
    }
    let suffix = if suffix.size == 0 {
        Vec::new()
    } else {
        unsafe { core::slice::from_raw_parts(suffix.data, suffix.size) }.to_vec()
    };
    let request = with_suffix(&with_suffix(name, &suffix), faction);
    let key = key_bytes(&request, fold);
    if map_call(
        context,
        step,
        0,
        &key,
        core::ptr::null_mut(),
        core::ptr::null_mut(),
    ) == 0
        || failed(flag)
    {
        return None;
    }
    let mut result = Bytes {
        data: core::ptr::null(),
        size: 0,
    };
    if map_call(
        context,
        step,
        1,
        &request,
        &mut result,
        core::ptr::null_mut(),
    ) == 0
        || failed(flag)
        || result.size == 0
        || result.data.is_null()
    {
        return None;
    }
    Some(unsafe { core::slice::from_raw_parts(result.data, result.size) }.to_vec())
}

fn map_emit(context: *mut c_void, emit: MapEmit, diffuse: &[u8], path: &[u8]) {
    unsafe {
        emit(
            context,
            diffuse.as_ptr(),
            diffuse.len(),
            path.as_ptr(),
            path.len(),
        );
    }
}

/// # Safety
/// Material and suffix spans remain readable for the call. Native callbacks
/// retain loose-name/cache STL storage, resolve paths, perform the exact
/// native suffix ordering primitive, and catch all C++ exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_specular(
    materials: *const Bytes,
    material_count: usize,
    factions: *const Bytes,
    faction_count: usize,
    suffix_enabled: bool,
    fold: Option<Fold>,
    step: Option<MapStep>,
    emit: Option<MapEmit>,
    context: *mut c_void,
    flag: *const u8,
) {
    let (Some(fold), Some(step), Some(emit)) = (fold, step, emit) else {
        return;
    };
    if (material_count != 0 && materials.is_null())
        || (faction_count != 0 && factions.is_null())
        || !suffix_enabled
        || flag.is_null()
    {
        return;
    }
    let materials = if material_count == 0 {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(materials, material_count) }
    };
    let factions = if faction_count == 0 {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(factions, faction_count) }
    };
    let factions: Vec<Vec<u8>> = factions
        .iter()
        .map(|span| {
            if span.size == 0 || span.data.is_null() {
                Vec::new()
            } else {
                unsafe { core::slice::from_raw_parts(span.data, span.size) }.to_vec()
            }
        })
        .collect();
    let mut registered: Vec<Vec<u8>> = Vec::new();
    for material in materials {
        if failed(flag) {
            return;
        }
        if material.size != 0 && material.data.is_null() {
            return;
        }
        let diffuse = if material.size == 0 {
            Vec::new()
        } else {
            unsafe { core::slice::from_raw_parts(material.data, material.size) }.to_vec()
        };
        let live_key = key_bytes(&diffuse, fold);
        if live_key.is_empty() {
            continue;
        }
        let mut base = Bytes {
            data: core::ptr::null(),
            size: 0,
        };
        let mut faction = Bytes {
            data: core::ptr::null(),
            size: 0,
        };
        if map_call(context, step, 2, &diffuse, &mut base, &mut faction) == 0 || failed(flag) {
            return;
        }
        if (base.size != 0 && base.data.is_null()) || (faction.size != 0 && faction.data.is_null())
        {
            return;
        }
        let base = if base.size == 0 {
            Vec::new()
        } else {
            unsafe { core::slice::from_raw_parts(base.data, base.size) }.to_vec()
        };
        let faction = if faction.size == 0 {
            Vec::new()
        } else {
            unsafe { core::slice::from_raw_parts(faction.data, faction.size) }.to_vec()
        };
        let base_specular = map_resolve(context, step, flag, &base, &[], fold);
        if failed(flag) {
            return;
        }
        if !faction.is_empty() {
            let resolved =
                map_resolve(context, step, flag, &base, &faction, fold).or(base_specular);
            if failed(flag) {
                return;
            }
            if let Some(path) = resolved
                && !registered.contains(&live_key)
            {
                registered.push(live_key);
                map_emit(context, emit, &diffuse, &path);
            }
            continue;
        }
        if let Some(ref path) = base_specular
            && !registered.contains(&live_key)
        {
            registered.push(live_key.clone());
            map_emit(context, emit, &diffuse, path);
        }
        if failed(flag) {
            return;
        }
        for faction_suffix in &factions {
            if faction_suffix.is_empty() {
                continue;
            }
            let variant = with_suffix(&base, faction_suffix);
            let variant_key = key_bytes(&variant, fold);
            if variant_key.is_empty()
                || variant_key == live_key
                || registered.contains(&variant_key)
            {
                continue;
            }
            registered.push(variant_key.clone());
            let resolved = map_resolve(context, step, flag, &base, faction_suffix, fold)
                .or_else(|| base_specular.clone());
            if failed(flag) {
                return;
            }
            if let Some(path) = resolved {
                map_emit(context, emit, &variant, &path);
                if failed(flag) {
                    return;
                }
            } else {
                registered.retain(|key| key != &variant_key);
            }
        }
    }
}

/// # Safety
/// `values` is a readable array of borrowed byte spans. The emit callback
/// catches native allocation exceptions and sets `flag` before returning.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_suffixes(
    values: *const Bytes,
    count: usize,
    emit: Option<Emit>,
    context: *mut c_void,
    flag: *const u8,
) {
    let Some(emit) = emit else {
        return;
    };
    if (count != 0 && values.is_null()) || flag.is_null() {
        return;
    }
    let values = if count == 0 {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(values, count) }
    };
    let mut seen: Vec<Vec<u8>> = Vec::new();
    seen.push(b"_b".to_vec());
    unsafe {
        emit(context, b"_b".as_ptr(), 2);
    }
    if failed(flag) {
        return;
    }
    for value in values {
        if value.size == 0 || value.data.is_null() {
            continue;
        }
        let bytes = unsafe { core::slice::from_raw_parts(value.data, value.size) };
        if seen.iter().any(|entry| entry == bytes) {
            continue;
        }
        seen.push(bytes.to_vec());
        unsafe {
            emit(context, bytes.as_ptr(), bytes.len());
        }
        if failed(flag) {
            return;
        }
    }
}

fn call(
    context: *mut c_void,
    step: Step,
    op: u32,
    bytes: &[u8],
    index: u32,
    output: *mut Bytes,
) -> u32 {
    unsafe { step(context, op, bytes.as_ptr(), bytes.len(), index, output) }
}

fn failed(flag: *const u8) -> bool {
    unsafe { *flag != 0 }
}

fn join(left: &[u8], right: &[u8]) -> Vec<u8> {
    if left.is_empty() {
        return right.to_vec();
    }
    if right.is_empty() {
        return left.to_vec();
    }
    let mut result = left.to_vec();
    if !matches!(result.last(), Some(b'\\' | b'/')) {
        result.push(b'\\');
    }
    result.extend_from_slice(right);
    result
}

fn has_extension(value: &[u8]) -> bool {
    let slash = value.iter().rposition(|b| *b == b'\\' || *b == b'/');
    let dot = value.iter().rposition(|b| *b == b'.');
    matches!(dot, Some(pos) if slash.is_none_or(|s| pos > s))
}

fn candidate(
    context: *mut c_void,
    step: Step,
    flag: *const u8,
    base: &[u8],
    append_extension: bool,
) -> Option<Vec<u8>> {
    const EXTENSIONS: [&[u8]; 4] = [b".dds", b".tga", b".png", b".bmp"];
    if append_extension {
        for extension in EXTENSIONS {
            let mut path = base.to_vec();
            path.extend_from_slice(extension);
            if call(context, step, 4, &path, 0, core::ptr::null_mut()) != 0 {
                return Some(path);
            }
            if failed(flag) {
                return None;
            }
        }
    } else {
        // The frozen empty-extension entry probes an explicit filename once
        // in the loop and once again at the unconditional base fallback.
        if call(context, step, 4, base, 0, core::ptr::null_mut()) != 0 {
            return Some(base.to_vec());
        }
        if failed(flag) {
            return None;
        }
    }
    if call(context, step, 4, base, 0, core::ptr::null_mut()) != 0 {
        Some(base.to_vec())
    } else {
        None
    }
}

fn remember(context: *mut c_void, step: Step, value: &[u8]) {
    call(context, step, 5, value, 0, core::ptr::null_mut());
}

/// # Safety
/// `requested` is readable for `size` bytes, `flag` is a live caller-owned
/// exception indicator, and the callback returns valid borrowed bytes until
/// the next callback. It catches every native exception before returning.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_nebula_resolve_texture(
    context: *mut c_void,
    requested: *const u8,
    size: usize,
    step: Option<Step>,
    flag: *const u8,
) {
    let Some(step) = step else {
        return;
    };
    if requested.is_null() || flag.is_null() {
        return;
    }
    let raw = unsafe { core::slice::from_raw_parts(requested, size) };
    let mut first = 0;
    while first < raw.len()
        && call(
            context,
            step,
            0,
            &raw[first..first + 1],
            0,
            core::ptr::null_mut(),
        ) != 0
    {
        first += 1;
        if failed(flag) {
            return;
        }
    }
    if failed(flag) {
        return;
    }
    let mut last = raw.len();
    while last > first
        && call(
            context,
            step,
            0,
            &raw[last - 1..last],
            0,
            core::ptr::null_mut(),
        ) != 0
    {
        last -= 1;
        if failed(flag) {
            return;
        }
    }
    if failed(flag) || first == last {
        return;
    }
    // The frozen trim() normalizes separators before lookup and cache access.
    let mut value = raw[first..last].to_vec();
    for byte in &mut value {
        if *byte == b'/' {
            *byte = b'\\';
        }
    }
    if call(context, step, 1, &value, 0, core::ptr::null_mut()) != 0 || failed(flag) {
        return;
    }
    let append_extension = !has_extension(&value);
    let drive_path = value.len() >= 3
        && call(context, step, 6, &value[..1], 0, core::ptr::null_mut()) != 0
        && value[1] == b':'
        && matches!(value[2], b'\\' | b'/');
    if failed(flag) {
        return;
    }
    let unc_path = value.starts_with(b"\\\\");
    if drive_path || unc_path {
        remember(
            context,
            step,
            &candidate(context, step, flag, &value, append_extension).unwrap_or_default(),
        );
        return;
    }
    let count = call(context, step, 2, &[], 0, core::ptr::null_mut());
    if failed(flag) {
        return;
    }
    for index in 0..count {
        let mut root = Bytes {
            data: core::ptr::null(),
            size: 0,
        };
        if call(context, step, 3, &[], index, &mut root) == 0 || failed(flag) {
            return;
        }
        let root = if root.size == 0 {
            Vec::new()
        } else {
            if root.data.is_null() {
                return;
            }
            unsafe { core::slice::from_raw_parts(root.data, root.size) }.to_vec()
        };
        let mut prefixed = true;
        for (index, byte) in b"textures\\".iter().enumerate() {
            if index >= value.len()
                || call(
                    context,
                    step,
                    7,
                    &value[index..index + 1],
                    0,
                    core::ptr::null_mut(),
                ) != u32::from(*byte)
            {
                prefixed = false;
                break;
            }
            if failed(flag) {
                return;
            }
        }
        if failed(flag) {
            return;
        }
        if prefixed {
            if let Some(path) =
                candidate(context, step, flag, &join(&root, &value), append_extension)
            {
                remember(context, step, &path);
                return;
            }
            if failed(flag) {
                return;
            }
        } else {
            let textures = join(&root, b"Textures");
            for directory in [b"".as_slice(), b"RGB", b"Index8", b"Compressed"] {
                let folder = if directory.is_empty() {
                    textures.clone()
                } else {
                    join(&textures, directory)
                };
                if let Some(path) = candidate(
                    context,
                    step,
                    flag,
                    &join(&folder, &value),
                    append_extension,
                ) {
                    remember(context, step, &path);
                    return;
                }
                if failed(flag) {
                    return;
                }
            }
            if let Some(path) =
                candidate(context, step, flag, &join(&root, &value), append_extension)
            {
                remember(context, step, &path);
                return;
            }
            if failed(flag) {
                return;
            }
        }
    }
    remember(context, step, &[]);
}

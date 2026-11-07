use alloc::{format, vec::Vec};
use core::ffi::c_void;
#[repr(C)]
#[derive(Default)]
pub struct Data {
    words: [usize; 16],
    values: [u32; 16],
    text: *const u8,
    text_size: usize,
}
#[repr(C)]
pub struct Ops {
    context: *mut c_void,
    readable: unsafe extern "C" fn(*mut c_void, usize, usize) -> u8,
    read: unsafe extern "C" fn(*mut c_void, usize, *mut c_void, usize),
    equals: unsafe extern "C" fn(*mut c_void, usize, *const u8, usize, u8) -> u8,
    action: unsafe extern "C" fn(*mut c_void, u32, u32, *mut Data, *const u8, usize) -> usize,
}
#[repr(C)]
pub struct Layout {
    dos_size: u32,
    nt_size: u32,
    entry_size: u32,
    lfanew: u32,
    signature: u32,
    machine: u32,
    magic: u32,
    image_size: u32,
    import_rva: u32,
    import_size: u32,
    entry_name: u32,
    entry_original: u32,
    entry_first: u32,
}
impl Ops {
    fn call(&self, op: u32, index: u32, d: &mut Data, s: &[u8]) -> usize {
        unsafe { (self.action)(self.context, op, index, d, s.as_ptr(), s.len()) }
    }
    fn good(&self) -> bool {
        self.call(17, 0, &mut Data::default(), &[]) == 0
    }
    fn readable(&self, p: usize, n: usize) -> bool {
        unsafe { (self.readable)(self.context, p, n) != 0 }
    }
    fn u16(&self, p: usize) -> u16 {
        let mut n = 0u16;
        unsafe { (self.read)(self.context, p, (&mut n as *mut u16).cast(), 2) };
        n
    }
    fn u32(&self, p: usize) -> u32 {
        let mut n = 0u32;
        unsafe { (self.read)(self.context, p, (&mut n as *mut u32).cast(), 4) };
        n
    }
    fn pointer(&self, p: usize) -> usize {
        let mut n = 0usize;
        unsafe {
            (self.read)(
                self.context,
                p,
                (&mut n as *mut usize).cast(),
                core::mem::size_of::<usize>(),
            )
        };
        n
    }
}
fn add(p: usize, n: usize) -> usize {
    p.wrapping_add(n)
}
fn valid(a: &Ops, base: usize, size: usize, rva: usize, n: usize) -> bool {
    rva <= size && n <= size - rva && a.readable(add(base, rva), n)
}
fn equals(a: &Ops, base: usize, size: usize, rva: usize, text: &[u8], insensitive: bool) -> bool {
    valid(a, base, size, rva, text.len())
        && unsafe {
            (a.equals)(
                a.context,
                add(base, rva),
                text.as_ptr(),
                text.len(),
                insensitive as u8,
            ) != 0
        }
}
fn cb(a: &Ops, index: u32, info: bool) -> usize {
    let mut d = Data::default();
    d.values[0] = info as u32;
    a.call(2, index, &mut d, &[])
}
fn exchange(a: &Ops, index: u32, slot: usize, value: usize) -> bool {
    let mut d = Data::default();
    d.words[0] = slot;
    d.words[1] = value;
    a.call(3, index, &mut d, &[]) != 0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3dx_install(a: *const Ops, base: usize, l: *const Layout) -> u8 {
    let (a, l) = unsafe { (&*a, &*l) };
    if !a.readable(base, l.dos_size as usize) || a.u16(base) != 0x5a4d {
        return 0;
    }
    let offset = a.u32(add(base, l.lfanew as usize));
    if offset > 0x100000 || !a.readable(add(base, offset as usize), l.nt_size as usize) {
        return 0;
    }
    let nt = add(base, offset as usize);
    if a.u32(add(nt, l.signature as usize)) != 0x4550
        || a.u16(add(nt, l.machine as usize)) != 0x14c
        || a.u16(add(nt, l.magic as usize)) != 0x10b
    {
        return 0;
    }
    let size = a.u32(add(nt, l.image_size as usize)) as usize;
    let dir = a.u32(add(nt, l.import_rva as usize)) as usize;
    let len = a.u32(add(nt, l.import_size as usize)) as usize;
    if dir == 0 || !valid(a, base, size, dir, len) {
        return 0;
    }
    let (mut info, mut create) = (0, 0);
    let mut offset = 0;
    while offset + l.entry_size as usize <= len {
        let e = add(base, dir + offset);
        let name = a.u32(add(e, l.entry_name as usize)) as usize;
        if name == 0 {
            break;
        }
        let original = a.u32(add(e, l.entry_original as usize)) as usize;
        let first = a.u32(add(e, l.entry_first as usize)) as usize;
        if equals(a, base, size, name, b"D3DX81ab.dll\0", true) && original != 0 && first != 0 {
            let mut i = 0usize;
            loop {
                let x = original.wrapping_add(i.wrapping_mul(4));
                let y = first.wrapping_add(i.wrapping_mul(4));
                if !valid(a, base, size, x, 4) || !valid(a, base, size, y, 4) {
                    return 0;
                }
                let name = a.u32(add(base, x)) as usize;
                if name == 0 {
                    break;
                }
                if name & 0x80000000 == 0 && valid(a, base, size, name, 2) {
                    if equals(
                        a,
                        base,
                        size,
                        name + 2,
                        b"D3DXGetImageInfoFromFileA\0",
                        false,
                    ) {
                        if info != 0 {
                            return 0;
                        }
                        info = add(base, y)
                    } else if equals(
                        a,
                        base,
                        size,
                        name + 2,
                        b"D3DXCreateTextureFromFileExA\0",
                        false,
                    ) {
                        if create != 0 {
                            return 0;
                        }
                        create = add(base, y)
                    }
                }
                i = i.wrapping_add(1);
            }
        }
        offset += l.entry_size as usize;
    }
    if info == 0 || create == 0 || a.pointer(info) == 0 || a.pointer(create) == 0 {
        return 0;
    }
    let mut index = 0;
    while index < 8 {
        let mut d = Data::default();
        a.call(0, index, &mut d, &[]);
        if d.words[0] == info || d.words[1] == create {
            return (d.values[0] != 0
                && d.words[0] == info
                && d.words[1] == create
                && a.pointer(info) == cb(a, index, true)
                && a.pointer(create) == cb(a, index, false)) as u8;
        }
        if d.words[0] == 0 {
            break;
        }
        index += 1;
    }
    if index == 8 {
        return 0;
    }
    let mut d = Data::default();
    d.words[0] = info;
    d.words[1] = create;
    a.call(1, index, &mut d, &[]);
    if !exchange(a, index, info, cb(a, index, true)) {
        return 0;
    }
    if !exchange(a, index, create, cb(a, index, false)) {
        let original = a.call(16, index, &mut Data::default(), &[]);
        exchange(a, index, info, original);
        return 0;
    }
    a.call(4, index, &mut Data::default(), &[]);
    a.call(
        5,
        index,
        &mut Data::default(),
        b"[D3D8Compat] Named D3DX81ab file bridge installed; D3DX9 imports unchanged",
    );
    a.good() as u8
}
fn text(d: &Data) -> &[u8] {
    if d.text_size == 0 {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(d.text, d.text_size) }
    }
}
fn report(a: &Ops, index: u32, path: &[u8], api: &[u8], hr: i32, info: bool) {
    let mut d = Data::default();
    a.call(0, index, &mut d, &[]);
    if d.values[1] == 0 {
        return;
    }
    d.values[0] = info as u32;
    if a.call(6, index, &mut d, &[]) as i32 > 16 {
        return;
    }
    let name = path
        .rsplit(|c| matches!(c, b'/' | b'\\'))
        .next()
        .unwrap_or(&[]);
    let mut line = b"[D3D8Compat] D3DX8 catalog file bridge ".to_vec();
    line.extend_from_slice(api);
    line.extend_from_slice(b" asset=");
    line.extend_from_slice(name);
    line.extend_from_slice(format!(" HRESULT=0x{:08x}", hr as u32).as_bytes());
    a.call(5, index, &mut d, &line);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3dx_info(a: *const Ops, index: u32) -> i32 {
    let a = unsafe { &*a };
    let mut d = Data::default();
    a.call(7, index, &mut d, &[]);
    if !a.good() {
        return 0x8007000eu32 as i32;
    }
    let path = text(&d).to_vec();
    d.values[0] = (!path.is_empty()) as u32;
    let hr = a.call(8, index, &mut d, &[]) as i32;
    if !a.good() {
        return 0x8007000eu32 as i32;
    }
    if !path.is_empty() {
        report(a, index, &path, b"GetImageInfoFromFileA", hr, true)
    }
    hr
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_d3dx_create(a: *const Ops, index: u32, input: *mut Data) -> i32 {
    let a = unsafe { &*a };
    let input = unsafe { &mut *input };
    let [width, height, mips, usage, format, key, ..] = input.values;
    let mut d = Data::default();
    let mut path = Vec::new();
    if a.call(9, index, &mut d, &[]) != 0 {
        a.call(7, index, &mut d, &[]);
        if !a.good() {
            return 0x8007000eu32 as i32;
        }
        path = text(&d).to_vec();
    }
    let mut source = mips;
    if !path.is_empty() {
        if a.call(10, index, &mut d, &[]) == 0 {
            if !a.good() {
                return 0x8007000eu32 as i32;
            }
            let mut message = b"canonical BC validation failed: ".to_vec();
            message.extend_from_slice(text(&d));
            a.call(11, index, &mut d, &[]);
            report(a, index, &path, &message, 0x8876086cu32 as i32, false);
            return 0x8876086cu32 as i32;
        }
        if mips == 0 || mips == u32::MAX {
            source = d.values[2]
        }
        if (width != u32::MAX && width != d.values[0])
            || (height != u32::MAX && height != d.values[1])
            || source > d.values[2]
            || (format != 0 && format != 0x34545844)
            || usage != 0
        {
            a.call(11, index, &mut d, &[]);
            report(
                a,
                index,
                &path,
                b"unsupported BC resize/reformat/mip generation/usage",
                0x8876086au32 as i32,
                false,
            );
            return 0x8876086au32 as i32;
        }
    }
    d.values[0] = (!path.is_empty()) as u32;
    d.values[1] = source;
    let mut hr = a.call(12, index, &mut d, &[]) as i32;
    if !a.good() {
        return 0x8007000eu32 as i32;
    }
    if !path.is_empty()
        && hr >= 0
        && (a.call(13, index, &mut d, &[]) == 0 || a.call(14, index, &mut d, &[]) == 0)
    {
        a.call(15, index, &mut d, &[]);
        hr = 0x8876086au32 as i32
    }
    if !path.is_empty() {
        let api = format!(
            "CreateTextureFromFileExA width={width} height={height} mips={mips} supplied_mips={source} format={format} key={key}"
        );
        report(a, index, &path, api.as_bytes(), hr, false)
    }
    hr
}

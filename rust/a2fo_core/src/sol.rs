//! Legacy SOL tagged-record parser. Native Map storage stays with the C++ caller.
use alloc::{format, vec::Vec};
use core::{ffi::c_void, slice};

const PREFIX: &[u8] = b"version [1] =\r\n";
const MARKER: &[u8] = b"\r\nBinaryMode [1] =\r\ntrue\r\n";
const EMPTY_MISSION: &[u8] = b"EmptyMission\0";

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Event {
    kind: u32,
    value: usize,
    data: *const u8,
    length: usize,
    extra: *const u8,
    extra_length: usize,
    position: [u32; 3],
    transform: [u32; 12],
    id: u32,
    team: u32,
    opaque_id: u32,
}
impl Event {
    fn new(kind: u32) -> Self {
        Self {
            kind,
            value: 0,
            data: core::ptr::null(),
            length: 0,
            extra: core::ptr::null(),
            extra_length: 0,
            position: [0; 3],
            transform: [0; 12],
            id: 0,
            team: 0,
            opaque_id: 0,
        }
    }
    fn bytes(kind: u32, b: &[u8]) -> Self {
        let mut event = Self::new(kind);
        event.data = b.as_ptr();
        event.length = b.len();
        event
    }
    fn value(kind: u32, value: usize) -> Self {
        let mut event = Self::new(kind);
        event.value = value;
        event
    }
}
type Emit = unsafe extern "C" fn(*mut c_void, *const Event) -> i32;
type Version = unsafe extern "C" fn(*mut c_void, *const u8, usize, *mut u32) -> i32;

struct Sink {
    context: *mut c_void,
    emit: Emit,
    version: Version,
}
impl Sink {
    fn send(&self, event: Event) -> Result<(), ()> {
        if unsafe { (self.emit)(self.context, &event) } != 0 {
            Ok(())
        } else {
            Err(())
        }
    }
    fn fail(&self, message: &[u8]) -> Result<(), ()> {
        self.send(Event::bytes(0, message))?;
        Err(())
    }
}

#[derive(Clone, Copy)]
struct Record<'a> {
    tag: u32,
    data: &'a [u8],
    offset: usize,
}
fn scalar<const N: usize>(record: Record<'_>, tag: u32) -> Option<[u8; N]> {
    if record.tag != tag {
        return None;
    }
    record.data.try_into().ok()
}
fn u32_at(record: Record<'_>) -> Option<u32> {
    Some(u32::from_ne_bytes(scalar::<4>(record, 4)?))
}
fn u16_at(record: Record<'_>) -> Option<u16> {
    Some(u16::from_ne_bytes(scalar::<2>(record, 3)?))
}
fn words<const N: usize>(record: Record<'_>, tag: u32) -> Option<[u32; N]> {
    if record.tag != tag || record.data.len() != N * 4 {
        return None;
    }
    let mut out = [0; N];
    for (slot, chunk) in out.iter_mut().zip(record.data.chunks_exact(4)) {
        *slot = u32::from_ne_bytes(chunk.try_into().ok()?);
    }
    Some(out)
}
fn c_string(record: Record<'_>) -> &[u8] {
    if record.tag != 2 {
        return &[];
    }
    let end = record
        .data
        .iter()
        .position(|&b| b == 0)
        .unwrap_or(record.data.len());
    &record.data[..end]
}
fn pair<'a>(records: &'a [Record<'a>], i: usize) -> Option<&'a [u8]> {
    let length = u32_at(*records.get(i)?)?;
    let text = *records.get(i + 1)?;
    if text.tag != 2 || text.data.len() != length as usize {
        return None;
    }
    Some(c_string(text))
}
fn locate(bytes: &[u8], sink: &Sink) -> Result<usize, ()> {
    let bad = b"not a supported BinaryMode Legacy SOL file";
    if !bytes.starts_with(PREFIX) {
        return sink.fail(bad).map(|()| 0);
    }
    let text = &bytes[..bytes.len().min(256)];
    let Some(version_end) = text[PREFIX.len()..]
        .windows(2)
        .position(|b| b == b"\r\n")
        .map(|i| i + PREFIX.len())
    else {
        return sink.fail(bad).map(|()| 0);
    };
    let version_text = &text[PREFIX.len()..version_end];
    let mut version = 0u32;
    if unsafe {
        (sink.version)(
            sink.context,
            version_text.as_ptr(),
            version_text.len(),
            &mut version,
        )
    } == 0
    {
        return sink.fail(bad).map(|()| 0);
    }
    sink.send(Event::value(1, version as usize))?;
    let Some(marker) = text[version_end..]
        .windows(MARKER.len())
        .position(|b| b == MARKER)
        .map(|i| i + version_end)
    else {
        return sink.fail(bad).map(|()| 0);
    };
    let offset = marker + MARKER.len();
    if offset > bytes.len() {
        return sink.fail(bad).map(|()| 0);
    }
    Ok(offset)
}
fn tokenize<'a>(bytes: &'a [u8], mut offset: usize, sink: &Sink) -> Result<Vec<Record<'a>>, ()> {
    let mut records = Vec::new();
    while offset < bytes.len() {
        if bytes.len() - offset < 8 {
            return sink
                .fail(b"truncated SOL tagged-record header")
                .map(|()| records);
        }
        let tag = u32::from_ne_bytes(bytes[offset..offset + 4].try_into().unwrap());
        let size = u32::from_ne_bytes(bytes[offset + 4..offset + 8].try_into().unwrap()) as usize;
        let payload = offset + 8;
        if size > bytes.len() - payload {
            let message = format!("SOL record at byte {offset} extends past end of file");
            return sink.fail(message.as_bytes()).map(|()| records);
        }
        records.push(Record {
            tag,
            data: &bytes[payload..payload + size],
            offset,
        });
        offset = payload + size;
    }
    Ok(records)
}
fn prefix(records: &[Record<'_>], i: usize, require_odf: bool) -> Option<Event> {
    if i + 7 >= records.len() {
        return None;
    }
    let length = u16_at(records[i])?;
    if length == 0 || records[i + 1].tag != 2 || records[i + 1].data.len() != length as usize {
        return None;
    }
    let id = u32_at(records[i + 2])?;
    let position = words::<3>(records[i + 3], 9)?;
    let team = u32_at(records[i + 4])?;
    if records[i + 5].tag != 2 || records[i + 5].data.len() != 40 {
        return None;
    }
    let opaque_id = u32::from_ne_bytes(scalar::<4>(records[i + 6], 8)?);
    let transform = words::<12>(records[i + 7], 11)?;
    let odf = c_string(records[i + 1]);
    if require_odf && odf.is_empty() {
        return None;
    }
    let mut event = Event::bytes(13, odf);
    let instance = c_string(records[i + 5]);
    event.extra = instance.as_ptr();
    event.extra_length = instance.len();
    event.id = id;
    event.team = team;
    event.opaque_id = opaque_id;
    event.position = position;
    event.transform = transform;
    Some(event)
}
fn parse(bytes: &[u8], sink: &Sink) -> Result<(), ()> {
    let offset = locate(bytes, sink)?;
    let records = tokenize(bytes, offset, sink)?;
    if records.len() < 40 {
        return sink.fail(b"unsupported or corrupt Legacy SOL MapDetails prefix");
    }
    for (index, kind) in [(4, 2), (6, 3), (8, 4), (10, 5), (12, 6)] {
        let Some(value) = pair(&records, index) else {
            return sink.fail(b"unsupported or corrupt Legacy SOL MapDetails prefix");
        };
        sink.send(Event::bytes(kind, value))?;
    }
    for (index, kind) in [(15, 7), (16, 8)] {
        let Some(position) = words::<3>(records[index], 9) else {
            return sink.fail(b"unsupported or corrupt Legacy SOL MapDetails prefix");
        };
        let mut event = Event::new(kind);
        event.position = position;
        sink.send(event)?;
    }
    if records.len() > 19
        && let Some(length) = u32_at(records[19])
        && length != 0
        && records.len() > 20
        && records[20].tag == 2
        && records[20].data.len() == length as usize
    {
        sink.send(Event::bytes(9, c_string(records[20])))?;
    }
    let Some(start_count) = u32_at(records[27]) else {
        return sink.fail(b"invalid SOL start-location count");
    };
    if start_count > 64 {
        return sink.fail(b"invalid SOL start-location count");
    }
    sink.send(Event::value(10, start_count as usize))?;
    for i in 0..start_count as usize {
        let base = 28 + i * 12;
        if base + 2 >= records.len() {
            return sink.fail(b"truncated SOL start-location table");
        }
        let Some(position) = words::<3>(records[base], 9) else {
            return sink.fail(b"invalid SOL start-location record");
        };
        let Some(length) = u32_at(records[base + 1]) else {
            return sink.fail(b"invalid SOL start-location record");
        };
        if records[base + 2].tag != 2 || records[base + 2].data.len() != length as usize {
            return sink.fail(b"invalid SOL start-location record");
        }
        let mut event = Event::bytes(11, c_string(records[base + 2]));
        event.position = position;
        sink.send(event)?;
    }
    let start = 28 + start_count as usize * 12;
    let mut count_index = None;
    for i in start..records.len().saturating_sub(9) {
        let Some(count) = u32_at(records[i]) else {
            continue;
        };
        if count == 0 || count > 100_000 {
            continue;
        }
        if prefix(&records, i + 1, false).is_some() {
            count_index = Some(i);
            break;
        }
    }
    let Some(index) = count_index else {
        return sink.fail(b"SOL object table was not found");
    };
    let Some(expected) = u32_at(records[index]) else {
        return Err(());
    };
    let mut clear = Event::value(12, records[index].offset);
    clear.id = expected;
    sink.send(clear)?;
    let mut decoded = 0u32;
    let mut i = index + 1;
    while i < records.len() && decoded < expected {
        if let Some(event) = prefix(&records, i, true) {
            sink.send(event)?;
            decoded += 1;
            i += 8;
        } else {
            i += 1;
        }
    }
    if decoded != expected {
        let message = format!(
            "SOL declared {expected} map objects but only {decoded} common object records were decoded"
        );
        return sink.fail(message.as_bytes());
    }
    for record in &records {
        if record.tag == 2 && record.data.len() == 40 && record.data.starts_with(EMPTY_MISSION) {
            sink.send(Event::value(14, record.offset))?;
            break;
        }
    }
    Ok(())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_sol_parse(
    data: *const u8,
    len: usize,
    context: *mut c_void,
    emit: Emit,
    version: Version,
) -> i32 {
    let bytes = if len == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(data, len) }
    };
    let sink = Sink {
        context,
        emit,
        version,
    };
    if parse(bytes, &sink).is_ok() { 1 } else { 0 }
}

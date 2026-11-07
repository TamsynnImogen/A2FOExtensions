use std::io::Cursor;

use oxideav_core::{Demuxer, ReadSeek};
use oxideav_mkv::ebml::{write_element_id, write_vint};
use oxideav_mkv::ids;

fn elem_uint(id: u32, value: u64) -> Vec<u8> {
    let n = if value == 0 {
        1
    } else {
        (64 - value.leading_zeros()).div_ceil(8) as usize
    };
    let mut out = Vec::new();
    out.extend_from_slice(&write_element_id(id));
    out.extend_from_slice(&write_vint(n as u64, 0));
    for i in (0..n).rev() {
        out.push(((value >> (i * 8)) & 0xFF) as u8);
    }
    out
}

fn elem_int(id: u32, value: i64) -> Vec<u8> {
    let mut n = 1usize;
    while n < 8 {
        let min = -(1i64 << (n * 8 - 1));
        let max = (1i64 << (n * 8 - 1)) - 1;
        if (min..=max).contains(&value) {
            break;
        }
        n += 1;
    }
    let mut out = Vec::new();
    out.extend_from_slice(&write_element_id(id));
    out.extend_from_slice(&write_vint(n as u64, 0));
    out.extend_from_slice(&value.to_be_bytes()[8 - n..]);
    out
}

fn elem_bytes(id: u32, bytes: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    out.extend_from_slice(&write_element_id(id));
    out.extend_from_slice(&write_vint(bytes.len() as u64, 0));
    out.extend_from_slice(bytes);
    out
}

fn elem_str(id: u32, value: &str) -> Vec<u8> {
    elem_bytes(id, value.as_bytes())
}

fn elem_master(id: u32, body: &[u8]) -> Vec<u8> {
    elem_bytes(id, body)
}

fn ebml_header() -> Vec<u8> {
    let mut body = Vec::new();
    body.extend_from_slice(&elem_uint(ids::EBML_VERSION, 1));
    body.extend_from_slice(&elem_uint(ids::EBML_READ_VERSION, 1));
    body.extend_from_slice(&elem_uint(ids::EBML_MAX_ID_LENGTH, 4));
    body.extend_from_slice(&elem_uint(ids::EBML_MAX_SIZE_LENGTH, 8));
    body.extend_from_slice(&elem_str(ids::EBML_DOC_TYPE, "matroska"));
    body.extend_from_slice(&elem_uint(ids::EBML_DOC_TYPE_VERSION, 4));
    body.extend_from_slice(&elem_uint(ids::EBML_DOC_TYPE_READ_VERSION, 2));
    elem_master(ids::EBML_HEADER, &body)
}

fn block(track: u8, timecode: i16, flags: u8, frames: &[u8]) -> Vec<u8> {
    let mut body = Vec::new();
    body.extend_from_slice(&write_vint(track as u64, 0));
    body.extend_from_slice(&timecode.to_be_bytes());
    body.push(flags);
    body.extend_from_slice(frames);
    body
}

fn fixture() -> Vec<u8> {
    let mut track = Vec::new();
    track.extend_from_slice(&elem_uint(ids::TRACK_NUMBER, 1));
    track.extend_from_slice(&elem_uint(ids::TRACK_UID, 0xA1));
    track.extend_from_slice(&elem_uint(ids::TRACK_TYPE, ids::TRACK_TYPE_AUDIO));
    track.extend_from_slice(&elem_str(ids::CODEC_ID, "A_PCM/INT/LIT"));
    let mut audio = Vec::new();
    audio.extend_from_slice(&[0xB5, 0x84]); // SamplingFrequency = 48,000.0
    audio.extend_from_slice(&48_000.0f32.to_be_bytes());
    audio.extend_from_slice(&elem_uint(ids::CHANNELS, 1));
    track.extend_from_slice(&elem_master(ids::AUDIO, &audio));
    let tracks = elem_master(ids::TRACKS, &elem_master(ids::TRACK_ENTRY, &track));

    // Xiph lacing: three one-byte frames (frame count minus one, then the
    // first two frame sizes, followed by the three arbitrary payload bytes).
    let lace = [2, 1, 1, 0x11, 0x22, 0x33];
    let laced_block = elem_bytes(ids::BLOCK, &block(1, 0, 0x02, &lace));
    let mut group = laced_block;
    group.extend_from_slice(&elem_int(ids::DISCARD_PADDING, -480));
    let block_group = elem_master(ids::BLOCK_GROUP, &group);

    // An un-laced SimpleBlock after the BlockGroup must start a fresh 0/1
    // lace position and must not inherit the group's padding metadata.
    let simple = elem_bytes(ids::SIMPLE_BLOCK, &block(1, 10, 0x80, &[0x44]));
    let mut cluster = elem_uint(ids::TIMECODE, 0);
    cluster.extend_from_slice(&block_group);
    cluster.extend_from_slice(&simple);

    let info = elem_uint(ids::TIMECODE_SCALE, 1_000_000);
    let info = elem_master(ids::INFO, &info);
    let mut segment = info;
    segment.extend_from_slice(&tracks);
    segment.extend_from_slice(&elem_master(ids::CLUSTER, &cluster));

    let mut file = ebml_header();
    file.extend_from_slice(&elem_master(ids::SEGMENT, &segment));
    file
}

#[test]
fn block_group_padding_is_shared_across_laces_and_next_block_resets_position() {
    let input: Box<dyn ReadSeek> = Box::new(Cursor::new(fixture()));
    let mut demux = oxideav_mkv::demux::open_typed(input, &oxideav_core::NullCodecResolver)
        .expect("open fixture");

    for index in 0..3 {
        demux.next_packet().expect("laced packet");
        assert_eq!(demux.block_lace_position(), Some((index, 3)));
        assert_eq!(
            demux
                .block_group_meta()
                .and_then(|meta| meta.discard_padding()),
            Some(-480),
            "DiscardPadding must stay attached to each packet from its BlockGroup"
        );
    }

    demux.next_packet().expect("following un-laced packet");
    assert_eq!(demux.block_lace_position(), Some((0, 1)));
    assert_eq!(demux.block_group_meta(), None);
}

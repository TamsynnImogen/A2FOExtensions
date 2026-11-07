//! Isolated, experimental file or immutable-memory Bink decoder session. No game ABI.
use infinitier_bik_decoder::{BikAudioChunk, BikFrame, BikHeader, BikStreamingDecoder};
use std::fs::File;
use std::io::{self, Cursor, Read, Seek, SeekFrom};
use std::panic::{AssertUnwindSafe, catch_unwind};
use std::path::Path;
use std::sync::Arc;

pub type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;
const MAX_FILE: u64 = 1 << 30;
const MAX_PIXELS: u64 = 8_388_608;
const MAX_PACKET: u32 = 16 << 20;
const MAX_FRAMES: u32 = 100_000;
const MAX_PREROLL_PCM_SAMPLES: usize = 4_000_000;

pub struct Session {
    source: SourceReader,
    decoder: BikStreamingDecoder<SourceReader>,
    next_index: u32,
    fault: Option<String>,
}

enum SourceReader {
    File(File),
    Memory(Cursor<Arc<[u8]>>),
}

impl SourceReader {
    fn try_clone(&self) -> io::Result<Self> {
        match self {
            Self::File(file) => Ok(Self::File(file.try_clone()?)),
            Self::Memory(cursor) => Ok(Self::Memory(cursor.clone())),
        }
    }

    fn len(&self) -> io::Result<u64> {
        match self {
            Self::File(file) => Ok(file.metadata()?.len()),
            Self::Memory(cursor) => Ok(cursor.get_ref().len() as u64),
        }
    }
}

impl Read for SourceReader {
    fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        match self {
            Self::File(file) => file.read(buffer),
            Self::Memory(cursor) => cursor.read(buffer),
        }
    }
}

impl Seek for SourceReader {
    fn seek(&mut self, position: SeekFrom) -> io::Result<u64> {
        match self {
            Self::File(file) => file.seek(position),
            Self::Memory(cursor) => cursor.seek(position),
        }
    }
}

impl Session {
    pub fn open(path: impl AsRef<Path>) -> Result<Self> {
        let path = path.as_ref();
        let file = File::open(path)?;
        Self::from_reader(SourceReader::File(file), path.display().to_string())
    }

    /// Open an immutable in-memory Bink source without copying its bytes.
    pub fn from_bytes(bytes: Arc<[u8]>) -> Result<Self> {
        Self::from_reader(
            SourceReader::Memory(Cursor::new(bytes)),
            "<memory>".to_owned(),
        )
    }

    fn from_reader(mut source: SourceReader, label: String) -> Result<Self> {
        let source_len = source.len()?;
        preflight(&mut source, source_len)?;
        // Clones share a seek position. Every decoder frame read seeks to its
        // absolute indexed offset, as does the guard immediately before it.
        // Memory cursors have independent positions and share immutable bytes.
        let decoder_source = source.try_clone()?;
        let decoder = catch_unwind(AssertUnwindSafe(|| {
            BikStreamingDecoder::new(decoder_source, label)
        }))
        .map_err(|_| "upstream decoder panicked during open")??;
        let header = decoder.header();
        check_header(header, source_len)?;
        Ok(Self {
            source,
            decoder,
            next_index: 0,
            fault: None,
        })
    }
    pub fn header(&self) -> &BikHeader {
        self.decoder.header()
    }
    pub fn next_index(&self) -> u32 {
        self.next_index
    }
    pub fn next_frame(&mut self) -> Result<Option<BikFrame>> {
        if let Some(fault) = &self.fault {
            return Err(format!("session is faulted: {fault}").into());
        }
        match self.next_frame_inner() {
            Ok(frame) => Ok(frame),
            Err(error) => {
                let message = error.to_string();
                self.fault = Some(message.clone());
                Err(message.into())
            }
        }
    }

    fn next_frame_inner(&mut self) -> Result<Option<BikFrame>> {
        if let Some(entry) = self.header().frames.get(self.next_index as usize).copied() {
            if !self.header().audio_tracks.is_empty() {
                // Upstream indexes a packet's audio length without bounds checking.
                self.source.seek(SeekFrom::Start(entry.pos as u64))?;
                let mut bytes = [0u8; 4];
                self.source.read_exact(&mut bytes)?;
                let audio_len = u32::from_le_bytes(bytes);
                if !audio_prefix_fits(entry.size, audio_len) {
                    return Err("audio prefix exceeds frame packet".into());
                }
            }
        }
        let frame = catch_unwind(AssertUnwindSafe(|| self.decoder.next_frame()))
            .map_err(|_| "upstream decoder panicked during frame decode")??;
        if frame.is_some() {
            self.next_index += 1;
        }
        Ok(frame)
    }
    pub fn rewind(&mut self) -> Result<()> {
        self.seek_by_decode(0)
    }
    /// Reopen and sequentially decode all earlier frames; no keyframe seek is assumed.
    pub fn seek_by_decode(&mut self, index: u32) -> Result<()> {
        if index > self.header().frame_count {
            return Err("seek beyond frame count".into());
        }
        let label = self.decoder.name().to_owned();
        let fresh_source = self.source.try_clone()?;
        let mut fresh = Self::from_reader(fresh_source, label)?;
        for _ in 0..index {
            if fresh.next_frame()?.is_none() {
                return Err("early end during seek".into());
            }
        }
        *self = fresh;
        Ok(())
    }

    /// Rebuild decoder state by decoding from frame zero, and return audio at
    /// or after the target video time. Audio decoded while prerolling is
    /// cropped by sample-frame position rather than discarded wholesale.
    pub fn seek_by_decode_with_audio(&mut self, index: u32) -> Result<Vec<BikAudioChunk>> {
        if index > self.header().frame_count {
            return Err("seek beyond frame count".into());
        }
        let label = self.decoder.name().to_owned();
        let fresh_source = self.source.try_clone()?;
        let mut fresh = Self::from_reader(fresh_source, label)?;
        let header = fresh.header();
        let target_sample_frame = if let Some(track) = header.audio_tracks.first() {
            if header.fps_num == 0 {
                return Err("invalid zero frame-rate numerator".into());
            }
            (index as u128)
                .checked_mul(header.fps_den as u128)
                .and_then(|v| v.checked_mul(track.sample_rate as u128))
                .ok_or("audio preroll target overflow")?
                / header.fps_num as u128
        } else {
            0
        };
        let Some(track) = header.audio_tracks.first() else {
            for _ in 0..index {
                if fresh.next_frame()?.is_none() {
                    return Err("early end during seek".into());
                }
            }
            *self = fresh;
            return Ok(Vec::new());
        };
        let expected_rate = track.sample_rate as u32;
        let expected_channels = track.flags.channels() as u8;
        let mut emitted_sample_frames = 0u128;
        let mut retained_samples = 0usize;
        let mut retained = Vec::new();
        for _ in 0..index {
            let frame = fresh
                .next_frame()?
                .ok_or("early end during audio preroll seek")?;
            for chunk in frame.audio {
                if chunk.sample_rate != expected_rate || chunk.channels != expected_channels {
                    return Err("invalid audio chunk format during preroll seek".into());
                }
                let chunk_start = emitted_sample_frames;
                let chunk_frames = (chunk.samples.len() / expected_channels as usize) as u128;
                let cropped =
                    audio_chunk_after_sample_frame(&chunk, chunk_start, target_sample_frame)?;
                if let Some(kept) = cropped {
                    let count = kept.samples.len();
                    if count > MAX_PREROLL_PCM_SAMPLES - retained_samples {
                        return Err("audio preroll exceeds retained PCM sample bound".into());
                    }
                    retained_samples += count;
                    retained.push(kept);
                }
                emitted_sample_frames = emitted_sample_frames
                    .checked_add(chunk_frames)
                    .ok_or("audio preroll sample-frame count overflow")?;
            }
        }
        *self = fresh;
        Ok(retained)
    }
}

fn audio_chunk_after_sample_frame(
    chunk: &BikAudioChunk,
    chunk_start: u128,
    target: u128,
) -> Result<Option<BikAudioChunk>> {
    let channels = chunk.channels as usize;
    if channels == 0 || chunk.samples.len() % channels != 0 {
        return Err("invalid interleaved audio chunk during preroll seek".into());
    }
    let chunk_frames = (chunk.samples.len() / channels) as u128;
    let chunk_end = chunk_start
        .checked_add(chunk_frames)
        .ok_or("audio chunk sample-frame range overflow")?;
    if target >= chunk_end {
        return Ok(None);
    }
    let skip_frames = target.saturating_sub(chunk_start);
    let skip_samples = usize::try_from(skip_frames)
        .ok()
        .and_then(|frames| frames.checked_mul(channels))
        .ok_or("audio preroll crop offset overflow")?;
    Ok(Some(BikAudioChunk {
        channels: chunk.channels,
        sample_rate: chunk.sample_rate,
        samples: chunk.samples[skip_samples..].to_vec(),
    }))
}

fn audio_prefix_fits(packet_size: u32, audio_len: u32) -> bool {
    packet_size >= 4 && audio_len <= packet_size - 4
}

fn preflight<R: Read + Seek>(file: &mut R, len: u64) -> Result<()> {
    if !(44..=MAX_FILE).contains(&len) {
        return Err("source file size outside candidate bounds".into());
    }
    // try_clone() shares a file cursor. Always position the specific handle
    // before reading preflight fields, including when rebuilding a decoder.
    file.seek(SeekFrom::Start(0))?;
    let mut raw = [0u8; 44];
    file.read_exact(&mut raw)?;
    let field = |offset: usize| u32::from_le_bytes(raw[offset..offset + 4].try_into().unwrap());
    let frames = field(8);
    let max_packet = field(12);
    let width = field(20);
    let height = field(24);
    let tracks = field(40);
    let index_end = 44u64 + 12 * (tracks as u64) + 4 * (frames as u64 + 1);
    if frames == 0
        || frames > MAX_FRAMES
        || max_packet == 0
        || max_packet > MAX_PACKET
        || width == 0
        || height == 0
        || (width as u64) * (height as u64) > MAX_PIXELS
        || tracks > 1
        || index_end > len
    {
        return Err("header outside candidate bounds (frames, packet, pixels or tracks)".into());
    }
    if tracks == 1 {
        file.seek(SeekFrom::Start(48))?;
        let mut track = [0u8; 4];
        file.read_exact(&mut track)?;
        let rate = u16::from_le_bytes([track[0], track[1]]);
        if !(8_000..=65_535).contains(&rate) {
            return Err("audio rate outside candidate bounds".into());
        }
    }
    file.seek(SeekFrom::Start(0))?;
    Ok(())
}

fn check_header(header: &BikHeader, file_len: u64) -> Result<()> {
    if header.frames.len() != header.frame_count as usize || header.audio_tracks.len() > 1 {
        return Err("unsupported index or audio track count".into());
    }
    let index_end =
        44u64 + 12 * header.audio_tracks.len() as u64 + 4 * (header.frame_count as u64 + 1);
    for entry in &header.frames {
        if entry.size == 0
            || entry.size > MAX_PACKET
            || (entry.pos as u64) < index_end
            || (entry.pos as u64)
                .checked_add(entry.size as u64)
                .is_none_or(|end| end > file_len)
        {
            return Err("frame index outside candidate bounds".into());
        }
    }
    Ok(())
}

pub fn visible_plane(plane: &infinitier_bik_decoder::Plane) -> Result<Vec<u8>> {
    let width = plane.width as usize;
    let height = plane.height as usize;
    let len = width
        .checked_mul(height)
        .ok_or("plane dimensions overflow")?;
    if len as u64 > MAX_PIXELS || plane.stride < width {
        return Err("plane outside candidate bounds".into());
    }
    let mut out = Vec::with_capacity(len);
    for row in 0..height {
        let start = row
            .checked_mul(plane.stride)
            .ok_or("plane stride overflow")?;
        out.extend_from_slice(
            plane
                .data
                .get(start..start + width)
                .ok_or("short plane data")?,
        );
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use infinitier_bik_decoder::{BikAudioChunk, Plane};
    #[test]
    fn preroll_crop_keeps_interleaved_samples_at_target_time() {
        let chunk = BikAudioChunk {
            channels: 2,
            sample_rate: 22_050,
            samples: vec![10, 11, 20, 21, 30, 31, 40, 41],
        };
        let kept = audio_chunk_after_sample_frame(&chunk, 0, 2)
            .unwrap()
            .unwrap();
        assert_eq!(kept.channels, 2);
        assert_eq!(kept.sample_rate, 22_050);
        assert_eq!(kept.samples, vec![30, 31, 40, 41]);
        let later = audio_chunk_after_sample_frame(&chunk, 4, 2)
            .unwrap()
            .unwrap();
        assert_eq!(later.samples, chunk.samples);
        assert!(
            audio_chunk_after_sample_frame(&chunk, 0, 4)
                .unwrap()
                .is_none()
        );
    }
    #[test]
    fn strips_padding_without_swapping_planes() {
        let p = Plane {
            width: 3,
            height: 2,
            stride: 5,
            data: vec![1, 2, 3, 99, 99, 4, 5, 6, 99, 99],
        };
        assert_eq!(visible_plane(&p).unwrap(), vec![1, 2, 3, 4, 5, 6]);
    }
    #[test]
    fn malformed_header_is_explicit() {
        let path = std::env::temp_dir().join(format!("bink-candidate-test-{}", std::process::id()));
        std::fs::write(&path, vec![0u8; 44]).unwrap();
        assert!(Session::open(&path).is_err());
        std::fs::remove_file(path).unwrap();
    }
    #[test]
    fn preflight_rejects_oversized_header_fields() {
        use std::io::Cursor;
        let base = {
            let mut b = vec![0u8; 100];
            b[0..4].copy_from_slice(b"BIKi");
            b[8..12].copy_from_slice(&1u32.to_le_bytes());
            b[12..16].copy_from_slice(&64u32.to_le_bytes());
            b[20..24].copy_from_slice(&16u32.to_le_bytes());
            b[24..28].copy_from_slice(&16u32.to_le_bytes());
            b
        };
        let mut oversized_pixels = base.clone();
        oversized_pixels[20..24].copy_from_slice(&32768u32.to_le_bytes());
        oversized_pixels[24..28].copy_from_slice(&32768u32.to_le_bytes());
        assert!(preflight(&mut Cursor::new(oversized_pixels), 100).is_err());
        let mut tracks = base.clone();
        tracks[40..44].copy_from_slice(&1000u32.to_le_bytes());
        assert!(preflight(&mut Cursor::new(tracks), 100).is_err());
        let mut index = base;
        index[8..12].copy_from_slice(&100u32.to_le_bytes());
        assert!(preflight(&mut Cursor::new(index), 100).is_err());
    }
    #[test]
    fn audio_prefix_must_fit_packet() {
        assert!(!audio_prefix_fits(3, 0));
        assert!(!audio_prefix_fits(8, 5));
        assert!(audio_prefix_fits(8, 4));
    }
    #[test]
    fn postparse_rejects_index_beyond_file() {
        use infinitier_bik_decoder::FrameEntry;
        let mut header = BikHeader {
            signature: *b"BIKi",
            file_size: 100,
            frame_count: 1,
            max_frame_size: 64,
            width: 16,
            height: 16,
            fps_num: 30,
            fps_den: 1,
            video_flags: 0,
            audio_tracks: vec![],
            frames: vec![FrameEntry {
                pos: 90,
                size: 20,
                keyframe: true,
            }],
        };
        assert!(check_header(&header, 100).is_err());
        header.frames[0].pos = 60;
        assert!(check_header(&header, 100).is_ok());
    }
}

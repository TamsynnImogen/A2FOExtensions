//! Bounded media readers for the pinned pure-Rust codec set.

use std::collections::VecDeque;
use std::fs::File;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};

use oxideav_core::{
    AudioFrame, CodecParameters, ColorRange, Decoder, Demuxer, Error, ExecutionContext, Frame,
    MediaType, Packet, PixelFormat, RuntimeContext, SampleFormat, StreamInfo, TimeBase, VideoFrame,
};

use crate::audio::{AudioSpec, Converter, PcmFormat};
use crate::pixels::{ChromaLocation, Image, Layout, Matrix, Plane};

pub struct SourceInfo {
    pub width: u32,
    pub height: u32,
    pub duration_us: i64,
    pub has_audio: bool,
}

pub struct VideoPicture {
    pub frame: VideoFrame,
    pub params: CodecParameters,
    pub pts_us: i64,
}

impl VideoPicture {
    pub fn image(&self) -> Result<Image<'_>, String> {
        let format = self
            .params
            .pixel_format
            .ok_or("video pixel format unknown")?;
        let layout = match format {
            PixelFormat::Yuv420P => Layout::Yuv420,
            PixelFormat::Yuv422P => Layout::Yuv422,
            PixelFormat::Yuv444P => Layout::Yuv444,
            PixelFormat::Bgra => Layout::Bgra,
            PixelFormat::Rgba => Layout::Rgba,
            _ => return Err(format!("unsupported decoded pixel format: {format:?}")),
        };
        let signal = self
            .frame
            .color_signal()
            .unwrap_or(self.params.color_signal);
        let matrix_code = if signal.matrix.0 == 2 {
            self.params.color_signal.matrix.0
        } else {
            signal.matrix.0
        };
        let matrix = match matrix_code {
            1 => Matrix::Bt709,
            9 | 10 => Matrix::Bt2020,
            _ => Matrix::Bt601,
        };
        let range = if signal.range == ColorRange::Unspecified {
            self.params.resolved_color_range()
        } else {
            signal.range
        };
        let planes = self
            .frame
            .image_planes()
            .iter()
            .map(|p| Plane {
                data: p.data.as_slice(),
                stride: p.stride,
            })
            .collect();
        Ok(Image {
            width: self.params.width.ok_or("video width unknown")? as usize,
            height: self.params.height.ok_or("video height unknown")? as usize,
            layout,
            matrix,
            full_range: range == ColorRange::Full,
            chroma_location: if self.params.codec_id.to_string() == "h264" {
                ChromaLocation::Left
            } else {
                ChromaLocation::Center
            },
            planes,
        })
    }
}

enum Container {
    Mp4(oxideav_mp4::demux::Mp4Demuxer),
    Mkv(oxideav_mkv::demux::MkvDemuxer),
}

impl Container {
    fn demux(&self) -> &dyn Demuxer {
        match self {
            Self::Mp4(d) => d,
            Self::Mkv(d) => d,
        }
    }
    fn demux_mut(&mut self) -> &mut dyn Demuxer {
        match self {
            Self::Mp4(d) => d,
            Self::Mkv(d) => d,
        }
    }
    fn next_packet(
        &mut self,
    ) -> oxideav_core::Result<(Packet, Option<(usize, usize)>, Option<i64>)> {
        let packet = self.demux_mut().next_packet()?;
        let (lace, padding) = match self {
            Self::Mkv(d) => (
                d.block_lace_position(),
                d.block_group_meta().and_then(|m| m.discard_padding()),
            ),
            Self::Mp4(_) => (None, None),
        };
        Ok((packet, lace, padding))
    }
}

fn registry() -> RuntimeContext {
    let mut ctx = RuntimeContext::new();
    oxideav_h264::register(&mut ctx);
    oxideav_vp9::register(&mut ctx);
    oxideav_aac::register(&mut ctx);
    oxideav_opus::register(&mut ctx);
    oxideav_vorbis::register(&mut ctx);
    ctx
}

fn open_container(path: &Path, ctx: &RuntimeContext) -> Result<Container, String> {
    let ext = path
        .extension()
        .and_then(|e| e.to_str())
        .unwrap_or("")
        .to_ascii_lowercase();
    let file = || {
        File::open(path)
            .map(|f| Box::new(f) as Box<dyn oxideav_core::ReadSeek>)
            .map_err(|e| e.to_string())
    };
    match ext.as_str() {
        "mp4" | "m4v" | "m4a" | "mov" => oxideav_mp4::demux::open_typed(file()?, &ctx.codecs)
            .map(Container::Mp4)
            .map_err(|e| e.to_string()),
        "mkv" | "webm" | "mka" => oxideav_mkv::demux::open_typed(file()?, &ctx.codecs)
            .map(Container::Mkv)
            .map_err(|e| e.to_string()),
        _ => Err("unsupported media container".into()),
    }
}

fn micros(ticks: i64, base: TimeBase) -> Result<i64, String> {
    if !base.is_valid() {
        return Err("invalid media time base".into());
    }
    let n = ticks as i128 * base.num() as i128 * 1_000_000;
    i64::try_from(n / base.den() as i128).map_err(|_| "media timestamp overflow".into())
}

fn ticks(us: i64, base: TimeBase) -> Result<i64, String> {
    if !base.is_valid() {
        return Err("invalid media time base".into());
    }
    let n = us as i128 * base.den() as i128;
    i64::try_from(n / (base.num() as i128 * 1_000_000))
        .map_err(|_| "media timestamp overflow".into())
}

fn select_stream(container: &Container, media: MediaType) -> Option<StreamInfo> {
    container
        .demux()
        .streams()
        .iter()
        .find(|s| s.params.media_type == media)
        .cloned()
}

pub struct VideoReader {
    path: PathBuf,
    threads: usize,
    container: Container,
    decoder: Box<dyn Decoder>,
    stream: StreamInfo,
    info: SourceInfo,
    pending: VecDeque<VideoPicture>,
    flushed: bool,
    eof: bool,
    frame_index: i64,
    fallback_origin_us: i64,
    seek_target: i64,
    cancel: Option<Arc<AtomicBool>>,
}

impl VideoReader {
    pub fn open(path: &Path, threads: usize) -> Result<Self, String> {
        let ctx = registry();
        let mut container = open_container(path, &ctx)?;
        let stream = select_stream(&container, MediaType::Video).ok_or("no video stream")?;
        let id = stream.params.codec_id.to_string();
        if id != "h264" && id != "vp9" {
            return Err(format!("unsupported video codec: {id}"));
        }
        let mut decoder = ctx
            .codecs
            .first_decoder(&stream.params)
            .map_err(|e| e.to_string())?;
        decoder.set_execution_context(&ExecutionContext::with_threads(threads));
        let duration_us = container
            .demux()
            .duration_micros()
            .or_else(|| {
                stream
                    .duration
                    .and_then(|d| micros(d, stream.time_base).ok())
            })
            .unwrap_or(0);
        let info = SourceInfo {
            width: stream.params.width.unwrap_or(0),
            height: stream.params.height.unwrap_or(0),
            duration_us,
            has_audio: select_stream(&container, MediaType::Audio).is_some(),
        };
        container.demux_mut().set_active_streams(&[stream.index]);
        Ok(Self {
            path: path.to_path_buf(),
            threads,
            container,
            decoder,
            stream,
            info,
            pending: VecDeque::new(),
            flushed: false,
            eof: false,
            frame_index: 0,
            fallback_origin_us: 0,
            seek_target: 0,
            cancel: None,
        })
    }

    pub fn set_cancel(&mut self, flag: Arc<AtomicBool>) {
        self.cancel = Some(flag);
    }

    pub fn info(&self) -> &SourceInfo {
        &self.info
    }

    pub fn next(&mut self) -> Result<Option<VideoPicture>, String> {
        loop {
            if self
                .cancel
                .as_ref()
                .is_some_and(|flag| flag.load(Ordering::Acquire))
            {
                return Err("media cancelled".into());
            }
            if let Some(picture) = self.pending.pop_front() {
                if picture.pts_us >= self.seek_target {
                    return Ok(Some(picture));
                }
                continue;
            }
            if self.eof {
                return Ok(None);
            }
            match self.container.next_packet() {
                Ok((packet, _, _)) => {
                    if packet.stream_index != self.stream.index {
                        continue;
                    }
                    self.decoder
                        .send_packet(&packet)
                        .map_err(|e| e.to_string())?;
                    self.drain()?;
                }
                Err(Error::Eof) => {
                    if !self.flushed {
                        self.decoder.flush().map_err(|e| e.to_string())?;
                        self.flushed = true;
                        self.drain()?;
                    }
                    self.eof = true;
                }
                Err(e) => return Err(e.to_string()),
            }
        }
    }

    pub fn seek(&mut self, target_us: i64) -> Result<(), String> {
        let target_us = target_us.max(0);
        let seek_result = self
            .container
            .demux_mut()
            .seek_to(self.stream.index, ticks(target_us, self.stream.time_base)?);
        let actual = match seek_result {
            Ok(actual) => actual,
            Err(Error::Unsupported(_)) => {
                let cancel = self.cancel.clone();
                let mut fresh = Self::open(&self.path, self.threads)?;
                fresh.cancel = cancel;
                fresh.seek_target = target_us;
                *self = fresh;
                return Ok(());
            }
            Err(error) => return Err(error.to_string()),
        };
        self.decoder.reset().map_err(|e| e.to_string())?;
        self.pending.clear();
        self.flushed = false;
        self.eof = false;
        self.seek_target = target_us;
        self.frame_index = 0;
        self.fallback_origin_us = micros(actual, self.stream.time_base)?;
        Ok(())
    }

    fn drain(&mut self) -> Result<(), String> {
        loop {
            match self.decoder.receive_frame() {
                Ok(Frame::Video(frame)) => {
                    let mut params = self.stream.params.clone();
                    if let Some(out) = self.decoder.output_params() {
                        params.merge_output_params(&out);
                    }
                    let pts_us = if let Some(pts) = frame.pts {
                        micros(pts, self.stream.time_base)?
                    } else if let Some(fps) = params.frame_rate {
                        if fps.num <= 0 || fps.den <= 0 {
                            return Err("invalid fallback frame rate".into());
                        }
                        i64::try_from(
                            self.fallback_origin_us as i128
                                + self.frame_index as i128 * fps.den as i128 * 1_000_000
                                    / fps.num as i128,
                        )
                        .map_err(|_| "fallback video timestamp overflow")?
                    } else {
                        return Err("video frame has no PTS or frame rate".into());
                    };
                    self.frame_index += 1;
                    self.pending.push_back(VideoPicture {
                        frame,
                        params,
                        pts_us,
                    });
                }
                Ok(_) => return Err("video decoder produced non-video frame".into()),
                Err(Error::NeedMore) | Err(Error::Eof) => break,
                Err(e) => return Err(e.to_string()),
            }
        }
        Ok(())
    }
}

struct AudioGroup {
    next_lace: usize,
    count: usize,
    padding_ns: Option<i64>,
    frames: Vec<AudioFrame>,
    samples: usize,
}

pub struct AudioReader {
    container: Container,
    decoder: Box<dyn Decoder>,
    stream: StreamInfo,
    converter: Option<Converter>,
    source_spec: Option<(u32, usize, SampleFormat)>,
    group: Option<AudioGroup>,
    gapless: Option<oxideav_mp4::demux::GaplessTiming>,
    raw_samples: u64,
    skip_output: u64,
    flushed: bool,
    eof: bool,
    cancel: Option<Arc<AtomicBool>>,
}

impl AudioReader {
    pub fn open(path: &Path, target_us: i64) -> Result<Option<Self>, String> {
        let ctx = registry();
        let mut container = open_container(path, &ctx)?;
        let Some(mut stream) = select_stream(&container, MediaType::Audio) else {
            return Ok(None);
        };
        let id = stream.params.codec_id.to_string();
        if id != "aac" && id != "opus" && id != "vorbis" {
            return Err(format!("unsupported audio codec: {id}"));
        }
        if id == "opus" {
            stream.params.sample_rate = Some(48_000);
        }
        if let Some(channels) = stream.params.resolved_channels() {
            if channels != 1 && channels != 2 {
                return Err(format!("unsupported audio channel count: {channels}"));
            }
        }
        let decoder = ctx
            .codecs
            .first_decoder(&stream.params)
            .map_err(|e| e.to_string())?;
        let gapless = match &container {
            Container::Mp4(d) if id == "aac" => d.gapless_timing(stream.index),
            _ => None,
        };
        container.demux_mut().set_active_streams(&[stream.index]);
        let skip_output = (target_us.max(0) as u128 * 48_000)
            .div_ceil(1_000_000)
            .min(u64::MAX as u128) as u64;
        Ok(Some(Self {
            container,
            decoder,
            stream,
            converter: None,
            source_spec: None,
            group: None,
            gapless,
            raw_samples: 0,
            skip_output,
            flushed: false,
            eof: false,
            cancel: None,
        }))
    }

    pub fn set_cancel(&mut self, flag: Arc<AtomicBool>) {
        self.cancel = Some(flag);
    }

    pub fn next_pcm(&mut self) -> Result<Option<Vec<i16>>, String> {
        loop {
            if self
                .cancel
                .as_ref()
                .is_some_and(|flag| flag.load(Ordering::Acquire))
            {
                return Err("media cancelled".into());
            }
            if self.eof {
                return Ok(None);
            }
            match self.container.next_packet() {
                Ok((packet, lace, padding)) => {
                    if packet.stream_index != self.stream.index {
                        continue;
                    }
                    let (index, count) = lace.unwrap_or((0, 1));
                    if count == 0 || index >= count {
                        return Err("invalid MKV lace position".into());
                    }
                    if index == 0 {
                        if self.group.is_some() {
                            return Err("incomplete audio lace group".into());
                        }
                        self.group = Some(AudioGroup {
                            next_lace: 0,
                            count,
                            padding_ns: padding,
                            frames: Vec::new(),
                            samples: 0,
                        });
                    }
                    let group = self
                        .group
                        .as_mut()
                        .ok_or("audio lace continuation without start")?;
                    if group.next_lace != index
                        || group.count != count
                        || group.padding_ns != padding
                    {
                        return Err("inconsistent audio lace group".into());
                    }
                    self.decoder
                        .send_packet(&packet)
                        .map_err(|e| e.to_string())?;
                    loop {
                        match self.decoder.receive_frame() {
                            Ok(Frame::Audio(frame)) => {
                                group.samples = group
                                    .samples
                                    .checked_add(frame.samples as usize)
                                    .ok_or("audio group overflow")?;
                                if group.samples > 1_000_000 {
                                    return Err("audio lace group exceeds sample limit".into());
                                }
                                group.frames.push(frame);
                            }
                            Ok(_) => return Err("audio decoder produced non-audio frame".into()),
                            Err(Error::NeedMore) | Err(Error::Eof) => break,
                            Err(e) => return Err(e.to_string()),
                        }
                    }
                    group.next_lace += 1;
                    if group.next_lace == count {
                        let output = self.commit_group()?;
                        if let Some(output) = self.skip_prefix(output) {
                            return Ok(Some(output));
                        }
                    }
                }
                Err(Error::Eof) => {
                    if self.group.is_some() {
                        return Err("incomplete audio lace group at EOF".into());
                    }
                    if !self.flushed {
                        self.decoder.flush().map_err(|e| e.to_string())?;
                        self.flushed = true;
                        match self.decoder.receive_frame() {
                            Err(Error::Eof) | Err(Error::NeedMore) => {}
                            Ok(_) => return Err("unexpected delayed audio at EOF".into()),
                            Err(e) => return Err(e.to_string()),
                        }
                    }
                    self.eof = true;
                    let output = match self.converter.as_mut() {
                        Some(c) => c.finish()?,
                        None => Vec::new(),
                    };
                    return Ok(self.skip_prefix(output));
                }
                Err(e) => return Err(e.to_string()),
            }
        }
    }

    fn commit_group(&mut self) -> Result<Vec<i16>, String> {
        let group = self.group.take().ok_or("missing audio group")?;
        if !group.frames.is_empty() {
            let mut params = self.stream.params.clone();
            if let Some(out) = self.decoder.output_params() {
                params.merge_output_params(&out);
            }
            let rate = params.sample_rate.ok_or("decoded audio rate unknown")?;
            let channels = params
                .resolved_channels()
                .ok_or("decoded audio channels unknown")? as usize;
            let expected = if self.stream.params.codec_id.to_string() == "vorbis" {
                SampleFormat::F32P
            } else {
                SampleFormat::S16
            };
            if params
                .sample_format
                .is_some_and(|actual| actual != expected)
            {
                return Err(format!(
                    "unsupported decoded audio format: {:?}",
                    params.sample_format
                ));
            }
            let current = (rate, channels, expected);
            if self.source_spec.is_some_and(|previous| previous != current) {
                return Err("decoded audio format changed mid-stream".into());
            }
            self.source_spec = Some(current);
            let format = match expected {
                SampleFormat::S16 => PcmFormat::S16,
                SampleFormat::F32P => PcmFormat::F32Planar,
                _ => unreachable!(),
            };
            if self.converter.is_none() {
                self.converter = Some(Converter::new(AudioSpec {
                    rate,
                    channels,
                    format,
                })?);
            }
        }
        let rate = self.stream.params.sample_rate.ok_or("audio rate unknown")? as i128;
        let padding = group.padding_ns.unwrap_or(0) as i128 * rate;
        let trim = ((padding.abs() + 500_000_000) / 1_000_000_000) as usize;
        if trim > group.samples {
            return Err("audio discard padding exceeds group".into());
        }
        let mut start = if padding < 0 { trim } else { 0 };
        let mut end = group.samples - if padding > 0 { trim } else { 0 };
        if let Some(gapless) = self.gapless {
            let raw_start = self.raw_samples;
            let raw_end = raw_start
                .checked_add(group.samples as u64)
                .ok_or("audio sample count overflow")?;
            let valid_end = gapless
                .priming
                .checked_add(gapless.valid)
                .ok_or("gapless count overflow")?;
            start = start.max(
                gapless
                    .priming
                    .saturating_sub(raw_start)
                    .min(group.samples as u64) as usize,
            );
            end = end.min(
                valid_end
                    .saturating_sub(raw_start)
                    .min(group.samples as u64) as usize,
            );
            self.raw_samples = raw_end;
        }
        if end < start {
            return Err("conflicting audio trims".into());
        }
        let mut output = Vec::new();
        let mut offset = 0;
        for frame in group.frames {
            let n = frame.samples as usize;
            let begin = start.saturating_sub(offset).min(n);
            let finish = end.saturating_sub(offset).min(n);
            if finish > begin {
                output.extend(
                    self.converter
                        .as_mut()
                        .ok_or("missing audio converter")?
                        .push(&frame.data, n, begin, finish)?,
                );
            }
            offset += n;
        }
        Ok(output)
    }

    fn skip_prefix(&mut self, output: Vec<i16>) -> Option<Vec<i16>> {
        let frames = output.len() / 2;
        let skip = self.skip_output.min(frames as u64) as usize;
        self.skip_output -= skip as u64;
        if skip == frames {
            None
        } else {
            Some(output[skip * 2..].to_vec())
        }
    }
}

//! Safe per-movie state. The exported Bink ABI and platform audio sink live elsewhere.
use a2fo_bink_candidate::Session;
use infinitier_bik_decoder::{BikAudioChunk, BikPixels, VideoFrame};
use std::time::Instant;

/// The platform sink reports played sample frames, not submitted samples.
pub trait AudioSink: Send {
    fn submit(&mut self, samples: &[i16]) -> Result<(), String>;
    fn position_frames(&mut self) -> Result<u64, String>;
    fn reset(&mut self) -> Result<(), String>;
    fn set_volume(&mut self, linear: u32) -> Result<(), String>;
    fn service(&mut self) -> Result<(), String>;
}

/// Only the thirteen native-observed leading DWORDs are represented here.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Prefix {
    pub width: u32,
    pub height: u32,
    pub frames: u32,
    pub frame_num: u32,
    pub last_frame_num: u32,
    pub frame_rate: u32,
    pub frame_rate_div: u32,
    pub read_error: u32,
    pub open_flags: u32,
    pub bink_type: u32,
    pub size: u32,
    pub frame_size: u32,
    pub snd_size: u32,
}

impl Prefix {
    fn from_session(session: &Session, flags: u32) -> Result<Self, String> {
        let h = session.header();
        if h.frame_count == 0 || h.fps_num == 0 || h.fps_den == 0 {
            return Err("invalid Bink frame count or rate".into());
        }
        Ok(Self {
            width: h.width,
            height: h.height,
            frames: h.frame_count,
            frame_num: 1,
            last_frame_num: u32::MAX,
            frame_rate: h.fps_num,
            frame_rate_div: h.fps_den,
            read_error: 0,
            open_flags: flags,
            // This is the parsed container field; other native BinkType bits are unclaimed.
            bink_type: h.video_flags,
            size: u32::try_from(h.file_size.checked_sub(8).ok_or("short Bink file")?)
                .map_err(|_| "Bink size exceeds native prefix")?,
            frame_size: 0,
            snd_size: 0,
        })
    }
}

#[derive(Debug, Clone, Copy)]
struct FrameTimeline {
    first_frame: u32,
    wall_start: Instant,
    audio_start: u64,
}

impl FrameTimeline {
    fn due(
        &self,
        frame: u32,
        rate: u32,
        divisor: u32,
        now: Instant,
        audio_frames: Option<(u64, u32)>,
    ) -> Result<bool, String> {
        let delta = frame
            .checked_sub(self.first_frame)
            .ok_or("timeline frame moved backward")? as u128;
        let numerator = delta
            .checked_mul(divisor as u128)
            .ok_or("frame time overflow")?;
        if let Some((played, sample_rate)) = audio_frames {
            let due_samples = numerator
                .checked_mul(sample_rate as u128)
                .ok_or("audio time overflow")?
                / rate as u128;
            Ok(played.saturating_sub(self.audio_start) as u128 >= due_samples)
        } else {
            let due_nanos = numerator
                .checked_mul(1_000_000_000)
                .ok_or("wall time overflow")?
                / rate as u128;
            Ok(now.saturating_duration_since(self.wall_start).as_nanos() >= due_nanos)
        }
    }
}

fn active_audio_clock(
    played: u64,
    timeline: FrameTimeline,
    submitted_source_frames: u64,
    sample_rate: u32,
) -> Option<(u64, u32)> {
    (played.saturating_sub(timeline.audio_start) < submitted_source_frames)
        .then_some((played, sample_rate))
}

pub struct Movie {
    pub prefix: Prefix,
    session: Session,
    audio: Option<Box<dyn AudioSink>>,
    image: Option<VideoFrame>,
    decoded_frame: Option<u32>,
    timeline: Option<FrameTimeline>,
    audio_rate: Option<u32>,
    audio_channels: Option<u8>,
    pending_audio: Vec<BikAudioChunk>,
    submitted_source_frames: u64,
    fault: Option<String>,
}

impl Movie {
    pub fn new(
        session: Session,
        flags: u32,
        audio: Option<Box<dyn AudioSink>>,
    ) -> Result<Self, String> {
        let prefix = Prefix::from_session(&session, flags)?;
        let track = session.header().audio_tracks.first();
        if audio.is_some() && track.is_none() {
            return Err("audio sink supplied for a silent movie".into());
        }
        let audio_rate = track.map(|t| t.sample_rate as u32);
        let audio_channels = track.map(|t| t.flags.channels() as u8);
        Ok(Self {
            prefix,
            session,
            audio,
            image: None,
            decoded_frame: None,
            timeline: None,
            audio_rate,
            audio_channels,
            pending_audio: Vec::new(),
            submitted_source_frames: 0,
            fault: None,
        })
    }

    pub fn image(&self) -> Option<&VideoFrame> {
        self.image.as_ref()
    }

    fn ensure_healthy(&self) -> Result<(), String> {
        match &self.fault {
            Some(error) => Err(format!("movie is faulted: {error}")),
            None => Ok(()),
        }
    }

    fn fail<T>(&mut self, error: String) -> Result<T, String> {
        self.prefix.read_error = 1;
        self.fault = Some(error.clone());
        Err(error)
    }

    fn seek_to(&mut self, target: u32) -> Result<(), String> {
        let pending = if self.audio.is_some() {
            self.session
                .seek_by_decode_with_audio(target - 1)
                .map_err(|error| error.to_string())?
        } else {
            self.session
                .seek_by_decode(target - 1)
                .map_err(|error| error.to_string())?;
            Vec::new()
        };
        if let Some(audio) = self.audio.as_mut() {
            audio.reset()?;
        }
        self.pending_audio = pending;
        self.submitted_source_frames = 0;
        self.timeline = None;
        self.decoded_frame = None;
        Ok(())
    }

    fn submit_chunks(&mut self, chunks: Vec<BikAudioChunk>) -> Result<(), String> {
        let Some(audio) = self.audio.as_mut() else {
            return Ok(());
        };
        let rate = self.audio_rate.expect("audio sink requires track");
        let channels = self.audio_channels.expect("audio sink requires track") as usize;
        for chunk in chunks {
            if chunk.sample_rate != rate
                || chunk.channels as usize != channels
                || chunk.samples.len() % channels != 0
            {
                return Err("decoded audio format differs from header".into());
            }
            let source_frames = (chunk.samples.len() / channels) as u64;
            let next_count = self
                .submitted_source_frames
                .checked_add(source_frames)
                .ok_or("submitted audio frame count overflow")?;
            audio.submit(&chunk.samples)?;
            self.submitted_source_frames = next_count;
        }
        Ok(())
    }

    pub fn do_frame(&mut self, now: Instant) -> Result<(), String> {
        self.ensure_healthy()?;
        if self.decoded_frame == Some(self.prefix.frame_num) {
            return Ok(());
        }
        if self.session.next_index() != self.prefix.frame_num - 1 {
            if let Err(error) = self.seek_to(self.prefix.frame_num) {
                return self.fail(error);
            }
        }
        let audio_start = if self.timeline.is_none() {
            match self.audio.as_mut() {
                Some(audio) => match audio.position_frames() {
                    Ok(frames) => frames,
                    Err(error) => return self.fail(error),
                },
                None => 0,
            }
        } else {
            0
        };
        let frame = match self.session.next_frame() {
            Ok(Some(frame)) => frame,
            Ok(None) => return self.fail("early end of Bink frames".into()),
            Err(error) => return self.fail(error.to_string()),
        };
        let image = match frame.video.pixels {
            BikPixels::Yuv(image) => image,
            BikPixels::Rgba(_) => return self.fail("decoder returned RGBA instead of YUV".into()),
        };
        let pending = std::mem::take(&mut self.pending_audio);
        if let Err(error) = self.submit_chunks(pending) {
            return self.fail(error);
        }
        if let Err(error) = self.submit_chunks(frame.audio) {
            return self.fail(error);
        }
        self.image = Some(image);
        self.decoded_frame = Some(self.prefix.frame_num);
        self.prefix.last_frame_num = self.prefix.frame_num;
        self.timeline.get_or_insert(FrameTimeline {
            first_frame: self.prefix.frame_num,
            wall_start: now,
            audio_start,
        });
        Ok(())
    }

    pub fn next_frame(&mut self, _now: Instant) -> Result<(), String> {
        self.ensure_healthy()?;
        if self.prefix.frame_num < self.prefix.frames {
            self.prefix.frame_num += 1;
        } else {
            if let Err(error) = self.session.rewind() {
                return self.fail(error.to_string());
            }
            if let Some(audio) = self.audio.as_mut() {
                if let Err(error) = audio.reset() {
                    return self.fail(error);
                }
            }
            self.prefix.frame_num = 1;
            self.timeline = None;
            self.pending_audio.clear();
            self.submitted_source_frames = 0;
        }
        self.decoded_frame = None;
        Ok(())
    }

    pub fn goto(&mut self, frame: u32, flags: u32, _now: Instant) -> Result<(), String> {
        self.ensure_healthy()?;
        if flags != 0 {
            return self.fail(format!("unsupported BinkGoto flags: {flags:#x}"));
        }
        let target = frame.clamp(1, self.prefix.frames);
        if let Err(error) = self.seek_to(target) {
            return self.fail(error);
        }
        self.prefix.last_frame_num = self.prefix.frame_num;
        self.prefix.frame_num = target;
        self.decoded_frame = None;
        self.image = None;
        self.timeline = None;
        Ok(())
    }

    /// Returns true while the current frame is not yet due.
    pub fn wait(&mut self, now: Instant) -> Result<bool, String> {
        self.ensure_healthy()?;
        let Some(timeline) = self.timeline else {
            return Ok(false);
        };
        let audio_frames = if let Some(audio) = self.audio.as_mut() {
            if let Err(error) = audio.service() {
                return self.fail(error);
            }
            let played = match audio.position_frames() {
                Ok(frames) => frames,
                Err(error) => return self.fail(error),
            };
            active_audio_clock(
                played,
                timeline,
                self.submitted_source_frames,
                self.audio_rate.expect("audio sink requires track"),
            )
        } else {
            None
        };
        match timeline.due(
            self.prefix.frame_num,
            self.prefix.frame_rate,
            self.prefix.frame_rate_div,
            now,
            audio_frames,
        ) {
            Ok(due) => Ok(!due),
            Err(error) => self.fail(error),
        }
    }

    pub fn set_volume(&mut self, linear: u32) -> Result<(), String> {
        self.ensure_healthy()?;
        if let Some(audio) = self.audio.as_mut() {
            if let Err(error) = audio.set_volume(linear) {
                return self.fail(error);
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::{Arc, Mutex};
    use std::time::Duration;

    fn malformed_one_frame() -> Session {
        // A valid container/index around intentionally undecodable video data.
        let mut bytes = vec![0u8; 60];
        bytes[0..4].copy_from_slice(b"BIKi");
        bytes[4..8].copy_from_slice(&52u32.to_le_bytes());
        bytes[8..12].copy_from_slice(&1u32.to_le_bytes());
        bytes[12..16].copy_from_slice(&8u32.to_le_bytes());
        bytes[16..20].copy_from_slice(&1u32.to_le_bytes());
        bytes[20..24].copy_from_slice(&16u32.to_le_bytes());
        bytes[24..28].copy_from_slice(&16u32.to_le_bytes());
        bytes[28..32].copy_from_slice(&30u32.to_le_bytes());
        bytes[32..36].copy_from_slice(&1u32.to_le_bytes());
        bytes[44..48].copy_from_slice(&52u32.to_le_bytes());
        bytes[48..52].copy_from_slice(&60u32.to_le_bytes());
        bytes[52..60].fill(0xff);
        Session::from_bytes(Arc::from(bytes)).unwrap()
    }

    fn malformed_one_frame_with_audio() -> Session {
        let mut bytes = vec![0u8; 76];
        bytes[0..4].copy_from_slice(b"BIKi");
        bytes[4..8].copy_from_slice(&68u32.to_le_bytes());
        bytes[8..12].copy_from_slice(&1u32.to_le_bytes());
        bytes[12..16].copy_from_slice(&12u32.to_le_bytes());
        bytes[16..20].copy_from_slice(&1u32.to_le_bytes());
        bytes[20..24].copy_from_slice(&16u32.to_le_bytes());
        bytes[24..28].copy_from_slice(&16u32.to_le_bytes());
        bytes[28..32].copy_from_slice(&30u32.to_le_bytes());
        bytes[32..36].copy_from_slice(&1u32.to_le_bytes());
        bytes[40..44].copy_from_slice(&1u32.to_le_bytes());
        bytes[44..48].copy_from_slice(&8u32.to_le_bytes());
        bytes[48..50].copy_from_slice(&22_050u16.to_le_bytes());
        bytes[50..52].copy_from_slice(&0x4000u16.to_le_bytes());
        bytes[56..60].copy_from_slice(&64u32.to_le_bytes());
        bytes[60..64].copy_from_slice(&76u32.to_le_bytes());
        bytes[68..76].fill(0xff);
        Session::from_bytes(Arc::from(bytes)).unwrap()
    }

    struct CountingSink(Arc<Mutex<usize>>);
    impl AudioSink for CountingSink {
        fn submit(&mut self, samples: &[i16]) -> Result<(), String> {
            *self.0.lock().unwrap() += samples.len();
            Ok(())
        }
        fn position_frames(&mut self) -> Result<u64, String> {
            Ok(0)
        }
        fn reset(&mut self) -> Result<(), String> {
            Ok(())
        }
        fn set_volume(&mut self, _: u32) -> Result<(), String> {
            Ok(())
        }
        fn service(&mut self) -> Result<(), String> {
            Ok(())
        }
    }

    #[test]
    fn submitted_source_frames_count_mono_samples_once() {
        let count = Arc::new(Mutex::new(0));
        let mut movie = Movie::new(
            malformed_one_frame_with_audio(),
            0,
            Some(Box::new(CountingSink(count.clone()))),
        )
        .unwrap();
        movie
            .submit_chunks(vec![BikAudioChunk {
                channels: 1,
                sample_rate: 22_050,
                samples: vec![1, 2, 3, 4],
            }])
            .unwrap();
        assert_eq!(movie.submitted_source_frames, 4);
        assert_eq!(*count.lock().unwrap(), 4);
        assert!(
            movie
                .submit_chunks(vec![BikAudioChunk {
                    channels: 2,
                    sample_rate: 22_050,
                    samples: vec![1, 2],
                }])
                .is_err()
        );
        assert_eq!(movie.submitted_source_frames, 4);
        assert_eq!(*count.lock().unwrap(), 4);
    }

    #[test]
    fn prefix_and_decode_failure_are_sticky() {
        let mut movie = Movie::new(malformed_one_frame(), 0x2000, None).unwrap();
        assert_eq!(movie.prefix.width, 16);
        assert_eq!(movie.prefix.frames, 1);
        assert_eq!(movie.prefix.frame_num, 1);
        assert_eq!(movie.prefix.last_frame_num, u32::MAX);
        assert_eq!(movie.prefix.open_flags, 0x2000);
        assert_eq!(movie.prefix.size, 52);
        assert_eq!(movie.prefix.frame_size, 0);
        assert_eq!(movie.prefix.snd_size, 0);
        assert!(movie.do_frame(Instant::now()).is_err());
        assert_eq!(movie.prefix.read_error, 1);
        assert!(movie.image().is_none());
        assert!(
            movie
                .next_frame(Instant::now())
                .unwrap_err()
                .starts_with("movie is faulted:")
        );
    }

    #[test]
    fn unsupported_goto_flags_fault_before_seek() {
        let mut movie = Movie::new(malformed_one_frame(), 0, None).unwrap();
        assert!(
            movie
                .goto(1, 1, Instant::now())
                .unwrap_err()
                .contains("unsupported")
        );
        assert_eq!(movie.prefix.frame_num, 1);
        assert_eq!(movie.prefix.read_error, 1);
    }

    #[test]
    fn prefix_is_thirteen_native_dwords() {
        assert_eq!(std::mem::size_of::<Prefix>(), 13 * 4);
        assert_eq!(std::mem::align_of::<Prefix>(), 4);
    }

    #[test]
    fn wall_timeline_uses_rational_frame_rate_and_first_decode_epoch() {
        let start = Instant::now();
        let timeline = FrameTimeline {
            first_frame: 4,
            wall_start: start,
            audio_start: 0,
        };
        assert!(timeline.due(4, 30, 1, start, None).unwrap());
        assert!(
            !timeline
                .due(5, 30, 1, start + Duration::from_millis(33), None)
                .unwrap()
        );
        assert!(
            timeline
                .due(5, 30, 1, start + Duration::from_millis(34), None)
                .unwrap()
        );
        assert!(
            !timeline
                .due(6, 30_000, 1001, start + Duration::from_millis(66), None)
                .unwrap()
        );
        assert!(
            timeline
                .due(6, 30_000, 1001, start + Duration::from_millis(67), None)
                .unwrap()
        );
    }

    #[test]
    fn audio_timeline_uses_played_sample_frames() {
        let start = Instant::now();
        let timeline = FrameTimeline {
            first_frame: 1,
            wall_start: start,
            audio_start: 100,
        };
        let late_wall = start + Duration::from_secs(10);
        assert!(
            !timeline
                .due(2, 30, 1, late_wall, Some((1699, 48_000)))
                .unwrap()
        );
        assert!(timeline.due(2, 30, 1, start, Some((1700, 48_000))).unwrap());
    }

    #[test]
    fn drained_audio_falls_back_to_wall_clock() {
        let start = Instant::now();
        let timeline = FrameTimeline {
            first_frame: 1,
            wall_start: start,
            audio_start: 100,
        };
        assert_eq!(
            active_audio_clock(109, timeline, 10, 48_000),
            Some((109, 48_000))
        );
        assert_eq!(active_audio_clock(110, timeline, 10, 48_000), None);
        assert!(
            !timeline
                .due(2, 30, 1, start + Duration::from_millis(10), None)
                .unwrap()
        );
        assert!(
            timeline
                .due(2, 30, 1, start + Duration::from_millis(34), None)
                .unwrap()
        );
    }
}

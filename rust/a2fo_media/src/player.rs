//! Playback scheduling. All workers are joined before a host can be released.
use crate::{
    codec::{AudioReader, VideoPicture, VideoReader},
    control::Control,
    host::{Host, Poll},
    pixels::Composer,
};
use std::{
    path::Path,
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
        mpsc::{self, Receiver, SyncSender, TryRecvError},
    },
    thread::{self, JoinHandle},
    time::Duration,
};

enum Video {
    Picture(VideoPicture),
    End,
    Error(String),
}
enum Audio {
    Ready,
    End,
    Error(String),
}
struct Segment {
    stop: Arc<AtomicBool>,
    video: Receiver<Video>,
    audio: Receiver<Audio>,
    go: Option<SyncSender<()>>,
    workers: Vec<JoinHandle<()>>,
}
impl Drop for Segment {
    fn drop(&mut self) {
        self.stop.store(true, Ordering::Release);
        self.go.take();
        // Producers use try_send rather than blocking on a full queue.
        for worker in self.workers.drain(..) {
            let _ = worker.join();
        }
    }
}
fn send<T>(tx: &SyncSender<T>, mut value: T, stop: &AtomicBool) -> bool {
    loop {
        if stop.load(Ordering::Acquire) {
            return false;
        }
        match tx.try_send(value) {
            Ok(()) => return true,
            Err(mpsc::TrySendError::Disconnected(_)) => return false,
            Err(mpsc::TrySendError::Full(v)) => {
                value = v;
                thread::sleep(Duration::from_millis(1));
            }
        }
    }
}
impl Segment {
    fn new(
        mut video: VideoReader,
        audio: Option<AudioReader>,
        host: Arc<dyn Host>,
        control: Arc<Control>,
    ) -> Result<Self, String> {
        let stop = Arc::new(AtomicBool::new(false));
        video.set_cancel(stop.clone());
        let (vtx, vrx) = mpsc::sync_channel(3);
        let (atx, arx) = mpsc::sync_channel(2);
        let (go_tx, go_rx) = mpsc::sync_channel(1);
        let mut result = Self {
            stop: stop.clone(),
            video: vrx,
            audio: arx,
            go: Some(go_tx),
            workers: Vec::new(),
        };
        let vstop = stop.clone();
        result.workers.push(
            thread::Builder::new()
                .name("a2fo-video".into())
                .spawn(move || {
                    let outcome = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                        loop {
                            let item = match video.next() {
                                Ok(Some(p)) => Video::Picture(p),
                                Ok(None) => Video::End,
                                Err(e) => Video::Error(e),
                            };
                            let done = !matches!(item, Video::Picture(_));
                            if !send(&vtx, item, &vstop) || done {
                                break;
                            }
                        }
                    }));
                    if outcome.is_err() {
                        let _ = send(&vtx, Video::Error("video worker panicked".into()), &vstop);
                    }
                })
                .map_err(|e| e.to_string())?,
        );
        if let Some(mut reader) = audio {
            reader.set_cancel(stop.clone());
            let astop = stop.clone();
            result.workers.push(
                thread::Builder::new()
                    .name("a2fo-audio".into())
                    .spawn(move || {
                        let outcome = std::panic::catch_unwind(std::panic::AssertUnwindSafe(
                            || -> Result<(), String> {
                                let first = reader.next_pcm()?;
                                let mut sound = host.sound()?;
                                // Drop always resets before releasing foreign-owned buffers.
                                struct Guard<'a>(&'a mut dyn crate::host::Sound);
                                impl Drop for Guard<'_> {
                                    fn drop(&mut self) {
                                        self.0.reset();
                                    }
                                }
                                let sound = Guard(sound.as_mut());
                                if !send(&atx, Audio::Ready, &astop) {
                                    return Ok(());
                                }
                                loop {
                                    if astop.load(Ordering::Acquire) {
                                        return Ok(());
                                    }
                                    match go_rx.recv_timeout(Duration::from_millis(2)) {
                                        Ok(()) => break,
                                        Err(mpsc::RecvTimeoutError::Timeout) => {}
                                        Err(_) => return Ok(()),
                                    }
                                }
                                control.audio_ready.store(true, Ordering::Release);
                                let mut next = first;
                                loop {
                                    if astop.load(Ordering::Acquire) {
                                        return Ok(());
                                    }
                                    let Some(pcm) = next.take() else { break };
                                    for chunk in pcm.chunks(2048) {
                                        while !sound.0.write(chunk)? {
                                            if astop.load(Ordering::Acquire) {
                                                return Ok(());
                                            }
                                            control
                                                .audio_frames
                                                .store(sound.0.position(), Ordering::Release);
                                            host.sleep(1);
                                        }
                                        control
                                            .audio_frames
                                            .store(sound.0.position(), Ordering::Release);
                                    }
                                    next = reader.next_pcm()?;
                                }
                                while sound.0.pending()? {
                                    if astop.load(Ordering::Acquire) {
                                        return Ok(());
                                    }
                                    control
                                        .audio_frames
                                        .store(sound.0.position(), Ordering::Release);
                                    host.sleep(1);
                                }
                                control
                                    .audio_frames
                                    .store(sound.0.position(), Ordering::Release);
                                control.audio_finished.store(true, Ordering::Release);
                                Ok(())
                            },
                        ));
                        let message = match outcome {
                            Ok(Ok(())) => Audio::End,
                            Ok(Err(e)) => Audio::Error(e),
                            Err(_) => Audio::Error("audio worker panicked".into()),
                        };
                        let _ = send(&atx, message, &astop);
                    })
                    .map_err(|e| e.to_string())?,
            );
        } else {
            control.audio_finished.store(true, Ordering::Release);
            let _ = atx.try_send(Audio::Ready);
        }
        Ok(result)
    }
}
struct Input {
    start: i64,
    armed: bool,
}
impl Input {
    fn stop(&mut self, p: Poll, now: i64) -> bool {
        if p.valid == 0 || p.closing != 0 {
            return true;
        }
        if p.foreground == 0 {
            self.armed = false;
            return false;
        }
        if now - self.start < 250_000 {
            return false;
        }
        if p.keys == 0 {
            self.armed = true;
            return false;
        }
        self.armed
    }
}
struct Clock {
    anchor: i64,
    base: i64,
    tail: Option<(i64, i64)>,
    last: i64,
}
impl Clock {
    fn now(&mut self, host: &dyn Host, c: &Control) -> i64 {
        let wall = host.now();
        let value = if c.audio_ready.load(Ordering::Acquire) {
            let finished = c.audio_finished.load(Ordering::Acquire);
            let audio = self.base.saturating_add(
                (c.audio_frames.load(Ordering::Acquire) as i128 * 1_000_000 / 48_000)
                    .min(i64::MAX as i128) as i64,
            );
            if finished {
                let (at, position) = *self.tail.get_or_insert((wall, audio.max(self.last)));
                position.saturating_add(wall.saturating_sub(at).max(0))
            } else {
                audio
            }
        } else {
            self.base
                .saturating_add(wall.saturating_sub(self.anchor).max(0))
        };
        self.last = self.last.max(value);
        self.last
    }
}
/// Returns 1 on completion, 2 on cancellation; errors permit native fallback.
fn run(
    path: &Path,
    threads: usize,
    mut video: VideoReader,
    mut audio: Option<AudioReader>,
    host: Arc<dyn Host>,
    control: Arc<Control>,
    frames: &mut usize,
    trace: &dyn Fn(&str),
) -> Result<u32, String> {
    let duration = video.info().duration_us;
    let mut target = control
        .seek
        .lock()
        .unwrap_or_else(|p| p.into_inner())
        .take()
        .unwrap_or(0)
        .max(0);
    if duration > 0 && target >= duration {
        return Ok(1);
    }
    if target > 0 {
        video.seek(target)?;
        audio = AudioReader::open(path, target)?;
    }
    let mut input = Input {
        start: host.now(),
        armed: false,
    };
    let mut composer = Composer::new();
    let mut canvas = Vec::new();
    'restart: loop {
        if control.cancelled() {
            return Ok(2);
        }
        control.audio_frames.store(0, Ordering::Release);
        control.audio_ready.store(false, Ordering::Release);
        control.audio_finished.store(false, Ordering::Release);
        let segment = Segment::new(video, audio, host.clone(), control.clone())?;
        let mut ready = false;
        let mut first = None;
        while !ready || first.is_none() {
            if control.cancelled() {
                return Ok(2);
            }
            let poll = host.poll(true)?;
            if input.stop(poll, host.now()) {
                return Ok(2);
            }
            if !ready {
                match segment.audio.try_recv() {
                    Ok(Audio::Ready) => ready = true,
                    Ok(Audio::Error(e)) => return Err(e),
                    Ok(Audio::End) => return Err("audio ended before initialization".into()),
                    Err(TryRecvError::Disconnected) => {
                        return Err("audio worker disconnected".into());
                    }
                    Err(TryRecvError::Empty) => {}
                }
            }
            if first.is_none() {
                match segment.video.try_recv() {
                    Ok(Video::Picture(p)) => first = Some(p),
                    Ok(Video::End) => {
                        return if target > 0 {
                            Ok(1)
                        } else {
                            Err("video ended before first frame".into())
                        };
                    }
                    Ok(Video::Error(e)) => return Err(e),
                    Err(TryRecvError::Disconnected) => {
                        return Err("video worker disconnected".into());
                    }
                    Err(TryRecvError::Empty) => {}
                }
            }
            if !ready || first.is_none() {
                host.sleep(1);
            }
        }
        let first = first.unwrap();
        let p = host.poll(true)?;
        if input.stop(p, host.now()) {
            return Ok(2);
        }
        prepare(&mut composer, &first, p, &mut canvas)?;
        let mut clock = Clock {
            anchor: host.now(),
            base: target,
            tail: None,
            last: target,
        };
        if let Some(go) = &segment.go {
            let _ = go.try_send(());
        }
        let mut next = Some(first);
        let mut prepared = Some((p.width, p.height));
        let mut last_pts = target;
        let mut period = 33_333;
        let mut ended = false;
        loop {
            if control.cancelled() {
                return Ok(2);
            }
            if let Some(seek) = control
                .seek
                .lock()
                .unwrap_or_else(|p| p.into_inner())
                .take()
            {
                target = seek.max(0);
                drop(segment);
                if duration > 0 && target >= duration {
                    return Ok(1);
                }
                video = VideoReader::open(path, threads)?;
                video.seek(target)?;
                audio = AudioReader::open(path, target)?;
                continue 'restart;
            }
            let p = host.poll(!ended)?;
            if input.stop(p, host.now()) {
                return Ok(2);
            }
            match segment.audio.try_recv() {
                Ok(Audio::Error(e)) => return Err(e),
                _ => {}
            }
            let mut now = clock.now(host.as_ref(), &control);
            if let Some(picture) = next.as_ref() {
                // Compose ahead of its PTS. The visible DIB retains the prior
                // frame until present; conversion cost must not delay each PTS.
                if prepared != Some((p.width, p.height)) {
                    prepare(&mut composer, picture, p, &mut canvas)?;
                    prepared = Some((p.width, p.height));
                    now = clock.now(host.as_ref(), &control);
                }
                if control.cancelled() {
                    return Ok(2);
                }
                if now >= picture.pts_us {
                    if picture.pts_us > last_pts {
                        period = (picture.pts_us - last_pts).clamp(1, 1_000_000);
                    }
                    last_pts = picture.pts_us;
                    host.present(&canvas, p.width, p.height)?;
                    *frames += 1;
                    if *frames == 1 {
                        trace(&format!(
                            "Rust movie first frame: startup={} ms; viewport={}x{}; pts={} us",
                            host.now().saturating_sub(input.start) / 1000,
                            p.width,
                            p.height,
                            picture.pts_us
                        ));
                    }
                    next = None;
                    prepared = None;
                }
            }
            if next.is_none() && !ended {
                match segment.video.try_recv() {
                    Ok(Video::Picture(p)) => next = Some(p),
                    Ok(Video::End) => ended = true,
                    Ok(Video::Error(e)) => return Err(e),
                    Err(TryRecvError::Disconnected) => {
                        return Err("video worker disconnected".into());
                    }
                    Err(TryRecvError::Empty) => {}
                }
            }
            if ended
                && control.audio_finished.load(Ordering::Acquire)
                && now >= duration.max(last_pts.saturating_add(period))
            {
                return Ok(1);
            }
            host.sleep(1);
        }
    }
}
/// Runs a synchronous session; tracing stays on the calling UI thread.
pub fn play(
    path: &Path,
    threads: usize,
    video: VideoReader,
    audio: Option<AudioReader>,
    host: Arc<dyn Host>,
    control: Arc<Control>,
    trace: &dyn Fn(&str),
) -> Result<u32, String> {
    let started = std::time::Instant::now();
    let mut frames = 0;
    let result = run(
        path,
        threads,
        video,
        audio,
        host,
        control,
        &mut frames,
        trace,
    );
    let elapsed = started.elapsed();
    trace(&format!(
        "Rust movie performance: frames={frames}; wall={} ms; fps={:.2}; status={}",
        elapsed.as_millis(),
        frames as f64 / elapsed.as_secs_f64().max(0.000001),
        match &result {
            Ok(1) => "completed",
            Ok(_) => "cancelled",
            Err(_) => "failed",
        }
    ));
    result
}
fn prepare(
    composer: &mut Composer,
    picture: &VideoPicture,
    p: Poll,
    canvas: &mut Vec<u8>,
) -> Result<(), String> {
    if p.width == 0 || p.height == 0 || p.width > 16384 || p.height > 16384 {
        return Err("invalid movie viewport".into());
    }
    let len = (p.width as usize)
        .checked_mul(p.height as usize)
        .and_then(|n| n.checked_mul(4))
        .ok_or("viewport overflow")?;
    canvas.resize(len, 0);
    composer.compose(
        &picture.image()?,
        p.width as usize,
        p.height as usize,
        canvas,
    )?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::AtomicI64;
    struct Timer(AtomicI64);
    impl Host for Timer {
        fn now(&self) -> i64 {
            self.0.load(Ordering::Relaxed)
        }
        fn sleep(&self, ms: u32) {
            self.0.fetch_add(ms as i64 * 1000, Ordering::Relaxed);
        }
        fn poll(&self, _: bool) -> Result<Poll, String> {
            unreachable!()
        }
        fn present(&self, _: &[u8], _: u32, _: u32) -> Result<(), String> {
            unreachable!()
        }
        fn sound(&self) -> Result<Box<dyn crate::host::Sound>, String> {
            unreachable!()
        }
    }
    #[test]
    fn audio_position_is_master_and_eof_continues_monotonically() {
        let host = Timer(AtomicI64::new(1_000_000));
        let control = Control::new();
        let mut clock = Clock {
            anchor: 0,
            base: 200_000,
            tail: None,
            last: 200_000,
        };
        control.audio_frames.store(4800, Ordering::Release);
        control.audio_ready.store(true, Ordering::Release);
        assert_eq!(clock.now(&host, &control), 300_000);
        host.sleep(500);
        assert_eq!(clock.now(&host, &control), 300_000);
        control.audio_frames.store(9600, Ordering::Release);
        control.audio_finished.store(true, Ordering::Release);
        assert_eq!(clock.now(&host, &control), 400_000);
        host.sleep(100);
        assert_eq!(clock.now(&host, &control), 500_000);
    }
    #[test]
    fn clock_does_not_reverse_on_audio_start_or_wall_clock_regression() {
        let host = Timer(AtomicI64::new(100));
        let control = Control::new();
        let mut clock = Clock {
            anchor: 0,
            base: 10_000,
            tail: None,
            last: 10_000,
        };
        assert_eq!(clock.now(&host, &control), 10_100);
        control.audio_ready.store(true, Ordering::Release);
        assert_eq!(clock.now(&host, &control), 10_100);
        control.audio_finished.store(true, Ordering::Release);
        assert_eq!(clock.now(&host, &control), 10_100);
        host.0.store(1, Ordering::Relaxed);
        assert_eq!(clock.now(&host, &control), 10_100);
    }
    #[test]
    fn held_keys_and_foreign_focus_require_a_new_release() {
        let mut input = Input {
            start: 0,
            armed: false,
        };
        let mut p = Poll {
            valid: 1,
            foreground: 1,
            keys: 1,
            ..Default::default()
        };
        assert!(!input.stop(p, 200_000));
        assert!(!input.stop(p, 300_000));
        p.keys = 0;
        assert!(!input.stop(p, 301_000));
        p.foreground = 0;
        assert!(!input.stop(p, 302_000));
        p.keys = 1;
        p.foreground = 1;
        assert!(!input.stop(p, 303_000));
        p.keys = 0;
        assert!(!input.stop(p, 304_000));
        p.keys = 1;
        assert!(input.stop(p, 305_000));
        p.valid = 0;
        assert!(input.stop(p, 0));
    }
}

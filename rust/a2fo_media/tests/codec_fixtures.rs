//! End-to-end counts from the small, independently generated video fixtures.
//! Set A2FO_MEDIA_FIXTURES to their directory to run these slower tests.

use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};

use a2fo_media::codec::{AudioReader, VideoReader};

fn fixtures() -> Option<PathBuf> {
    std::env::var_os("A2FO_MEDIA_FIXTURES").map(|value| {
        let path = PathBuf::from(value);
        if path.is_absolute() || path.is_dir() {
            path
        } else {
            Path::new(env!("CARGO_MANIFEST_DIR"))
                .join("../..")
                .join(path)
        }
    })
}

fn count_video(path: &Path) -> usize {
    let mut reader = VideoReader::open(path, 1).expect("open video");
    let mut count = 0;
    while let Some(picture) = reader.next().expect("decode video") {
        let image = picture.image().expect("expose decoded planes");
        assert!(image.width > 0 && image.height > 0);
        count += 1;
    }
    count
}

fn count_audio(path: &Path, start_us: i64) -> Option<usize> {
    let mut reader = AudioReader::open(path, start_us).expect("open audio")?;
    let mut frames = 0;
    while let Some(pcm) = reader.next_pcm().expect("decode audio") {
        assert_eq!(pcm.len() % 2, 0, "stereo PCM must be interleaved");
        frames += pcm.len() / 2;
    }
    Some(frames)
}

#[test]
fn exact_fixture_video_and_audio_counts() {
    let Some(dir) = fixtures() else { return };
    for (file, expected_audio) in [
        ("aac.mp4", Some(16_798)),
        ("opus.webm", Some(16_800)),
        ("vorbis.webm", Some(16_801)),
        ("silent.mp4", None),
    ] {
        let path = dir.join(file);
        assert_eq!(count_video(&path), 9, "{file} video frames");
        assert_eq!(count_audio(&path, 0), expected_audio, "{file} audio frames");
        if let Some(expected) = expected_audio {
            assert_eq!(
                count_audio(&path, 100_000),
                Some(expected - 4_800),
                "{file} audio after 100 ms skip"
            );
        }
    }
}

#[test]
fn seek_to_100ms_rejects_earlier_video_frames() {
    let Some(dir) = fixtures() else { return };
    for file in ["aac.mp4", "opus.webm", "vorbis.webm", "silent.mp4"] {
        let mut reader = VideoReader::open(&dir.join(file), 1).expect("open video");
        reader.seek(100_000).expect("seek video");
        let first = reader
            .next()
            .expect("decode after seek")
            .expect("post-seek frame");
        assert!(first.pts_us >= 100_000, "{file}: {} us", first.pts_us);
    }
}

#[test]
fn pre_read_cancellation_stops_video_and_audio() {
    let Some(dir) = fixtures() else { return };
    let flag = Arc::new(AtomicBool::new(true));
    for file in ["aac.mp4", "opus.webm", "vorbis.webm", "silent.mp4"] {
        let path = dir.join(file);
        let mut video = VideoReader::open(&path, 1).expect("open video");
        video.set_cancel(flag.clone());
        assert!(video.next().is_err(), "{file} video cancellation");
        if let Some(mut audio) = AudioReader::open(&path, 0).expect("open audio") {
            audio.set_cancel(flag.clone());
            assert!(audio.next_pcm().is_err(), "{file} audio cancellation");
        }
    }
    assert!(flag.load(Ordering::Acquire));
}

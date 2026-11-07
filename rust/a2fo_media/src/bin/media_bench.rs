//! Headless media pipeline timing; no window or audio device is opened.

use std::hint::black_box;
use std::path::Path;
use std::time::Instant;

use a2fo_media::codec::{AudioReader, VideoReader};
use a2fo_media::pixels::{ChromaLocation, Composer, Image, Layout, Matrix, Plane};

fn positive(value: Option<&String>, name: &str) -> Result<usize, String> {
    let parsed = value
        .ok_or_else(|| format!("missing {name}"))?
        .parse::<usize>()
        .map_err(|_| format!("invalid {name}"))?;
    if parsed == 0 {
        Err(format!("{name} must be positive"))
    } else {
        Ok(parsed)
    }
}

fn compose(args: &[String]) -> Result<(), String> {
    if args.len() != 3 && args.len() != 5 {
        return Err(
            "usage: media_bench compose WIDTH HEIGHT FRAMES [SOURCE_WIDTH SOURCE_HEIGHT]".into(),
        );
    }
    let width = positive(args.first(), "WIDTH")?;
    let height = positive(args.get(1), "HEIGHT")?;
    let frames = positive(args.get(2), "FRAMES")?;
    if width > 16384 || height > 16384 {
        return Err("dimensions exceed 16384".into());
    }
    let source_width = if args.len() == 5 {
        positive(args.get(3), "SOURCE_WIDTH")?
    } else {
        width
    };
    let source_height = if args.len() == 5 {
        positive(args.get(4), "SOURCE_HEIGHT")?
    } else {
        height
    };
    if source_width > 16384 || source_height > 16384 {
        return Err("source dimensions exceed 16384".into());
    }
    let pixels = source_width
        .checked_mul(source_height)
        .ok_or("image extent overflow")?;
    let output_len = width
        .checked_mul(height)
        .ok_or("output extent overflow")?
        .checked_mul(4)
        .ok_or("output extent overflow")?;
    let cw = source_width.div_ceil(2);
    let ch = source_height.div_ceil(2);
    let mut y = vec![0_u8; pixels];
    let mut u = vec![0_u8; cw * ch];
    let mut v = vec![0_u8; cw * ch];
    for row in 0..source_height {
        for col in 0..source_width {
            y[row * source_width + col] = (16 + ((row + col) % 220)) as u8;
        }
    }
    for row in 0..ch {
        for col in 0..cw {
            u[row * cw + col] = (32 + ((col * 7 + row * 3) % 192)) as u8;
            v[row * cw + col] = (32 + ((col * 5 + row * 11) % 192)) as u8;
        }
    }
    let source = Image {
        width: source_width,
        height: source_height,
        layout: Layout::Yuv420,
        matrix: Matrix::Bt709,
        full_range: false,
        chroma_location: ChromaLocation::Left,
        planes: vec![
            Plane {
                data: &y,
                stride: source_width,
            },
            Plane {
                data: &u,
                stride: cw,
            },
            Plane {
                data: &v,
                stride: cw,
            },
        ],
    };
    let mut output = vec![0_u8; output_len];
    let mut composer = Composer::new();
    let started = Instant::now();
    for _ in 0..frames {
        composer.compose(&source, width, height, &mut output)?;
        black_box(&output);
    }
    let elapsed = started.elapsed();
    let checksum = output
        .iter()
        .fold(0_u64, |sum, &value| sum.wrapping_add(value as u64));
    println!(
        "compose width={width} height={height} frames={frames} elapsed_ms={:.3} checksum={checksum}",
        elapsed.as_secs_f64() * 1000.0
    );
    Ok(())
}

fn video(args: &[String]) -> Result<(), String> {
    if args.len() != 2 {
        return Err("usage: media_bench video PATH THREADS".into());
    }
    let threads = positive(args.get(1), "THREADS")?;
    let started = Instant::now();
    let mut reader = VideoReader::open(Path::new(&args[0]), threads)?;
    let mut frames = 0_u64;
    let mut checksum = 0_u64;
    while let Some(picture) = reader.next()? {
        frames += 1;
        checksum = checksum.wrapping_add(picture.pts_us as u64);
        black_box(&picture.frame);
    }
    println!(
        "video frames={frames} elapsed_ms={:.3} checksum={checksum}",
        started.elapsed().as_secs_f64() * 1000.0
    );
    Ok(())
}

fn audio(args: &[String]) -> Result<(), String> {
    if args.len() != 1 {
        return Err("usage: media_bench audio PATH".into());
    }
    let started = Instant::now();
    let Some(mut reader) = AudioReader::open(Path::new(&args[0]), 0)? else {
        return Err("no audio stream".into());
    };
    let mut frames = 0_u64;
    let mut checksum = 0_u64;
    while let Some(pcm) = reader.next_pcm()? {
        if pcm.len() % 2 != 0 {
            return Err("non-stereo PCM output".into());
        }
        frames += (pcm.len() / 2) as u64;
        for &sample in &pcm {
            checksum = checksum.wrapping_add(sample as i64 as u64);
        }
    }
    println!(
        "audio frames={frames} elapsed_ms={:.3} checksum={checksum}",
        started.elapsed().as_secs_f64() * 1000.0
    );
    Ok(())
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let result = match args.first().map(String::as_str) {
        Some("compose") => compose(&args[1..]),
        Some("video") => video(&args[1..]),
        Some("audio") => audio(&args[1..]),
        _ => Err(
            "usage: media_bench {compose WIDTH HEIGHT FRAMES|video PATH THREADS|audio PATH}".into(),
        ),
    };
    if let Err(error) = result {
        eprintln!("media_bench: {error}");
        std::process::exit(2);
    }
}

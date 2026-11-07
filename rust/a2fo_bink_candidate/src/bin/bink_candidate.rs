use a2fo_bink_candidate::{Result, Session, visible_plane};
use infinitier_bik_decoder::BikPixels;
use serde_json::json;
use std::fs::{self, File};
use std::io::Write;
use std::path::PathBuf;

fn run() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() == 2 && args[1] == "--version" {
        println!(
            "bink_candidate {} (infinitier_bik_decoder 0.0.16, isolated patched source)",
            env!("CARGO_PKG_VERSION")
        );
        return Ok(());
    }
    if args.len() != 7 && args.len() != 9 {
        return Err("usage: bink_candidate decode INPUT --output-dir NEWDIR --max-frames N [--start-frame N]".into());
    }
    if args[1] != "decode"
        || args[3] != "--output-dir"
        || args[5] != "--max-frames"
        || (args.len() == 9 && args[7] != "--start-frame")
    {
        return Err("invalid arguments; see usage".into());
    }
    let input = PathBuf::from(&args[2]);
    let output = PathBuf::from(&args[4]);
    let max_frames: u32 = args[6].parse()?;
    let start_frame: u32 = if args.len() == 9 { args[8].parse()? } else { 0 };
    if max_frames == 0 || max_frames > 300 {
        return Err("max-frames must be 1..300".into());
    }
    let mut session = Session::open(&input)?;
    let header = session.header().clone();
    if start_frame >= header.frame_count {
        return Err("start frame outside stream".into());
    }
    // Existing output is never overwritten; create_dir fails on any alias or existing directory.
    if output
        .canonicalize()
        .is_ok_and(|p| p == input.canonicalize().unwrap_or_default())
    {
        return Err("input and output alias".into());
    }
    session.seek_by_decode(start_frame)?;
    fs::create_dir(&output)?;
    let mut video = File::create(output.join("video.yuv"))?;
    let mut alpha = if header.has_alpha() {
        Some(File::create(output.join("alpha.y"))?)
    } else {
        None
    };
    let mut audio = if header.audio_tracks.is_empty() {
        None
    } else {
        Some(File::create(output.join("audio.s16le"))?)
    };
    let mut decoded = 0u32;
    let mut video_bytes = 0u64;
    let mut alpha_bytes = 0u64;
    let mut audio_bytes = 0u64;
    let mut audio_sample_frames = 0u64;
    while decoded < max_frames {
        let Some(frame) = session.next_frame()? else {
            break;
        };
        let BikPixels::Yuv(planes) = frame.video.pixels else {
            return Err("unexpected pixel format".into());
        };
        for plane in [&planes.y, &planes.u, &planes.v] {
            let bytes = visible_plane(plane)?;
            video.write_all(&bytes)?;
            video_bytes += bytes.len() as u64;
        }
        if let Some(ref mut out) = alpha {
            let bytes = visible_plane(planes.alpha.as_ref().ok_or("missing alpha plane")?)?;
            out.write_all(&bytes)?;
            alpha_bytes += bytes.len() as u64;
        }
        if let Some(ref mut out) = audio {
            for chunk in frame.audio {
                let channels = header.audio_tracks[0].flags.channels() as usize;
                if chunk.channels as usize != channels
                    || chunk.sample_rate != header.audio_tracks[0].sample_rate as u32
                    || chunk.samples.len() % channels != 0
                {
                    return Err("invalid decoded audio chunk".into());
                }
                if chunk.samples.len() > 4_194_304 {
                    return Err("decoded audio chunk exceeds candidate bound".into());
                }
                for sample in &chunk.samples {
                    out.write_all(&sample.to_le_bytes())?;
                }
                audio_bytes += (chunk.samples.len() * 2) as u64;
                audio_sample_frames += (chunk.samples.len() / channels) as u64;
            }
        }
        decoded += 1;
    }
    let track = header.audio_tracks.first();
    let meta = json!({
        "signature": String::from_utf8_lossy(&header.signature),
        "width": header.width, "height": header.height,
        "chroma_width": header.width.div_ceil(2), "chroma_height": header.height.div_ceil(2),
        "fps_num": header.fps_num, "fps_den": header.fps_den,
        "source_frame_count": header.frame_count, "start_frame": start_frame,
        "decoded_frame_count": decoded, "pixel_format": "yuv420p",
        "plane_order": "YUV", "has_alpha": header.has_alpha(),
        "audio_tracks_in_header": header.audio_tracks.len(),
        "audio_sample_rate": track.map(|x| x.sample_rate),
        "audio_channels": track.map(|x| x.flags.channels()),
        "audio_sample_frames": audio_sample_frames,
        "video_bytes": video_bytes, "alpha_bytes": alpha_bytes, "audio_bytes": audio_bytes,
        "seek_mode": "decode_from_start"
    });
    fs::write(
        output.join("metadata.json"),
        serde_json::to_vec_pretty(&meta)?,
    )?;
    println!("{}", meta);
    Ok(())
}

fn main() {
    if let Err(error) = run() {
        eprintln!("bink_candidate: {error}");
        std::process::exit(1);
    }
}

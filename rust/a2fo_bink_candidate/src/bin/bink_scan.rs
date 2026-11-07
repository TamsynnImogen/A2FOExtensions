//! Complete file-only scan without raw stream output.
use a2fo_bink_candidate::{Result, Session, visible_plane};
use infinitier_bik_decoder::{BikPixels, Plane};
use serde_json::json;
use sha2::{Digest, Sha256};
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::Instant;

const MAX_SOURCE_BYTES: u64 = 1 << 30;

fn hash_plane(plane: &Plane, frame: &mut Sha256, whole: &mut Sha256) -> Result<u64> {
    let bytes = visible_plane(plane)?;
    frame.update(&bytes);
    whole.update(&bytes);
    Ok(bytes.len() as u64)
}

fn hash_audio(
    samples: &[i16],
    frame: &mut Sha256,
    whole: &mut Sha256,
    mut pcm: Option<&mut tempfile::NamedTempFile>,
) -> Result<u64> {
    let mut buffer = [0u8; 8192];
    let mut used = 0usize;
    for sample in samples {
        if used == buffer.len() {
            frame.update(buffer);
            whole.update(buffer);
            if let Some(out) = pcm.as_mut() {
                out.write_all(&buffer)?;
            }
            used = 0;
        }
        buffer[used..used + 2].copy_from_slice(&sample.to_le_bytes());
        used += 2;
    }
    if used != 0 {
        frame.update(&buffer[..used]);
        whole.update(&buffer[..used]);
        if let Some(out) = pcm.as_mut() {
            out.write_all(&buffer[..used])?;
        }
    }
    Ok((samples.len() as u64) * 2)
}

fn parent_of(path: &Path) -> &Path {
    path.parent()
        .filter(|p| !p.as_os_str().is_empty())
        .unwrap_or(Path::new("."))
}

fn normalized_target(path: &Path) -> Result<PathBuf> {
    let name = path.file_name().ok_or("output must name a file")?;
    Ok(parent_of(path).canonicalize()?.join(name))
}

fn run() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 3 {
        return Err(
            "usage: bink_scan INPUT.bik NEW_JSON [--memory] [--audio-output NEW_PCM_FILE]".into(),
        );
    }
    let input = Path::new(&args[1]);
    let output = Path::new(&args[2]);
    let mut memory = false;
    let mut audio_output: Option<&Path> = None;
    let mut index = 3;
    while index < args.len() {
        match args[index].as_str() {
            "--memory" if !memory => {
                memory = true;
                index += 1;
            }
            "--audio-output" if audio_output.is_none() && index + 1 < args.len() => {
                audio_output = Some(Path::new(&args[index + 1]));
                index += 2;
            }
            _ => {
                return Err(
                    "usage: bink_scan INPUT.bik NEW_JSON [--memory] [--audio-output NEW_PCM_FILE]"
                        .into(),
                );
            }
        }
    }
    if output.exists() {
        return Err("JSON output already exists".into());
    }
    let output_target = normalized_target(output)?;
    if let Some(pcm) = audio_output {
        if pcm.exists() {
            return Err("PCM output already exists".into());
        }
        if normalized_target(pcm)? == output_target {
            return Err("JSON and PCM output paths alias".into());
        }
    }
    let source_bytes = fs::metadata(input)?.len();
    if !(44..=MAX_SOURCE_BYTES).contains(&source_bytes) {
        return Err("source file size outside candidate bounds".into());
    }
    let started = Instant::now();
    let mut session = if memory {
        let bytes: Arc<[u8]> = Arc::from(fs::read(input)?);
        Session::from_bytes(bytes)?
    } else {
        Session::open(input)?
    };
    let header = session.header().clone();
    let track = header.audio_tracks.first().copied();
    let mut pcm_temporary = audio_output
        .map(|path| tempfile::NamedTempFile::new_in(parent_of(path)))
        .transpose()?;
    let mut video_hash = Sha256::new();
    let mut audio_hash = Sha256::new();
    let mut alpha_hash = Sha256::new();
    let mut per_frame_video_sha256 = Vec::with_capacity(header.frame_count as usize);
    let mut per_frame_audio_sha256 = Vec::with_capacity(header.frame_count as usize);
    let mut per_frame_audio_sample_frames = Vec::with_capacity(header.frame_count as usize);
    let mut video_bytes = 0u64;
    let mut audio_bytes = 0u64;
    let mut alpha_bytes = 0u64;
    let mut audio_sample_frames = 0u64;
    let mut decoded_frame_count = 0u32;

    while let Some(frame) = session.next_frame()? {
        let BikPixels::Yuv(planes) = frame.video.pixels else {
            return Err("unexpected pixel format".into());
        };
        let mut frame_video = Sha256::new();
        for plane in [&planes.y, &planes.u, &planes.v] {
            video_bytes = video_bytes
                .checked_add(hash_plane(plane, &mut frame_video, &mut video_hash)?)
                .ok_or("video byte count overflow")?;
        }
        per_frame_video_sha256.push(format!("{:x}", frame_video.finalize()));
        if header.has_alpha() {
            let mut frame_alpha = Sha256::new();
            let plane = planes.alpha.as_ref().ok_or("missing alpha plane")?;
            alpha_bytes = alpha_bytes
                .checked_add(hash_plane(plane, &mut frame_alpha, &mut alpha_hash)?)
                .ok_or("alpha byte count overflow")?;
        }
        let mut frame_audio = Sha256::new();
        let mut sample_frames = 0u64;
        for chunk in frame.audio {
            let Some(audio_track) = track else {
                return Err("audio chunk without track".into());
            };
            let channels = audio_track.flags.channels() as usize;
            if channels == 0
                || chunk.channels as usize != channels
                || chunk.sample_rate != audio_track.sample_rate as u32
                || chunk.samples.len() % channels != 0
            {
                return Err("invalid decoded audio chunk".into());
            }
            audio_bytes = audio_bytes
                .checked_add(hash_audio(
                    &chunk.samples,
                    &mut frame_audio,
                    &mut audio_hash,
                    pcm_temporary.as_mut(),
                )?)
                .ok_or("audio byte count overflow")?;
            sample_frames = sample_frames
                .checked_add((chunk.samples.len() / channels) as u64)
                .ok_or("audio sample count overflow")?;
        }
        audio_sample_frames = audio_sample_frames
            .checked_add(sample_frames)
            .ok_or("audio sample count overflow")?;
        per_frame_audio_sample_frames.push(sample_frames);
        per_frame_audio_sha256.push(if track.is_some() {
            Some(format!("{:x}", frame_audio.finalize()))
        } else {
            None
        });
        decoded_frame_count = decoded_frame_count
            .checked_add(1)
            .ok_or("frame count overflow")?;
    }
    if decoded_frame_count != header.frame_count {
        return Err("decoder ended before header frame count".into());
    }
    let elapsed_seconds = started.elapsed().as_secs_f64();
    let report = json!({
        "signature": String::from_utf8_lossy(&header.signature),
        "width": header.width, "height": header.height,
        "chroma_width": header.width.div_ceil(2), "chroma_height": header.height.div_ceil(2),
        "fps_num": header.fps_num, "fps_den": header.fps_den,
        "source_frame_count": header.frame_count, "decoded_frame_count": decoded_frame_count,
        "header_file_size": header.file_size, "max_frame_size": header.max_frame_size,
        "video_flags": header.video_flags, "is_gray": header.is_gray(),
        "has_alpha": header.has_alpha(), "audio_tracks_in_header": header.audio_tracks.len(),
        "audio_sample_rate": track.map(|t| t.sample_rate),
        "audio_channels": track.map(|t| t.flags.channels()),
        "audio_flags": track.map(|t| t.flags.bits()),
        "audio_sample_frames": audio_sample_frames,
        "source_bytes": source_bytes, "video_bytes": video_bytes,
        "audio_bytes": audio_bytes, "alpha_bytes": alpha_bytes,
        "video_sha256": format!("{:x}", video_hash.finalize()),
        "audio_sha256": track.map(|_| format!("{:x}", audio_hash.finalize())),
        "alpha_sha256": header.has_alpha().then(|| format!("{:x}", alpha_hash.finalize())),
        "per_frame_audio_sample_frames": per_frame_audio_sample_frames,
        "per_frame_video_sha256": per_frame_video_sha256,
        "per_frame_audio_sha256": per_frame_audio_sha256,
        "elapsed_seconds": elapsed_seconds,
        "scan_mode": if memory { "memory" } else { "file" },
        "audio_output": audio_output.map(|path| path.display().to_string()),
    });
    let mut temporary = tempfile::NamedTempFile::new_in(parent_of(output))?;
    serde_json::to_writer_pretty(&mut temporary, &report)?;
    temporary.write_all(b"\n")?;
    temporary.as_file().sync_all()?;
    if let (Some(pcm), Some(path)) = (pcm_temporary, audio_output) {
        pcm.as_file().sync_all()?;
        pcm.persist_noclobber(path)?;
    }
    if let Err(error) = temporary.persist_noclobber(output) {
        if let Some(path) = audio_output {
            if let Err(cleanup) = fs::remove_file(path) {
                return Err(format!(
                    "JSON commit failed: {error}; PCM cleanup failed: {cleanup}; inspect {}",
                    path.display()
                )
                .into());
            }
        }
        return Err(format!("JSON commit failed: {error}").into());
    }
    println!("{}", output.display());
    Ok(())
}

fn main() {
    if let Err(error) = run() {
        eprintln!("bink_scan: {error}");
        std::process::exit(1);
    }
}

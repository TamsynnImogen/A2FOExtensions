use a2fo_bink_candidate::Session;
use std::path::PathBuf;
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};

fn temp_path(label: &str) -> PathBuf {
    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_nanos();
    std::env::temp_dir().join(format!(
        "bink-session-{label}-{}-{nonce}",
        std::process::id()
    ))
}

fn synthetic_bik(with_audio: bool, audio_len: u32, video: &[u8]) -> Vec<u8> {
    let frame_pos = if with_audio { 64u32 } else { 52u32 };
    let packet_len = if with_audio {
        4 + video.len()
    } else {
        video.len()
    };
    let end = frame_pos as usize + packet_len;
    let mut bytes = vec![0u8; end];
    bytes[0..4].copy_from_slice(b"BIKi");
    bytes[4..8].copy_from_slice(&(end as u32 - 8).to_le_bytes());
    bytes[8..12].copy_from_slice(&1u32.to_le_bytes());
    bytes[12..16].copy_from_slice(&(packet_len as u32).to_le_bytes());
    bytes[16..20].copy_from_slice(&1u32.to_le_bytes());
    bytes[20..24].copy_from_slice(&16u32.to_le_bytes());
    bytes[24..28].copy_from_slice(&16u32.to_le_bytes());
    bytes[28..32].copy_from_slice(&30u32.to_le_bytes());
    bytes[32..36].copy_from_slice(&1u32.to_le_bytes());
    bytes[40..44].copy_from_slice(&(u32::from(with_audio)).to_le_bytes());
    let index = if with_audio {
        bytes[44..48].copy_from_slice(&8u32.to_le_bytes());
        bytes[48..50].copy_from_slice(&22_050u16.to_le_bytes());
        bytes[50..52].copy_from_slice(&0x4000u16.to_le_bytes());
        frame_pos as usize - 8
    } else {
        44
    };
    bytes[index..index + 4].copy_from_slice(&frame_pos.to_le_bytes());
    bytes[index + 4..index + 8].copy_from_slice(&(end as u32).to_le_bytes());
    let payload = frame_pos as usize;
    if with_audio {
        bytes[payload..payload + 4].copy_from_slice(&audio_len.to_le_bytes());
        bytes[payload + 4..payload + 4 + video.len()].copy_from_slice(video);
    } else {
        bytes[payload..payload + video.len()].copy_from_slice(video);
    }
    bytes
}

#[test]
fn audio_guard_reads_open_file_after_path_replacement_and_fault_is_sticky() {
    let path = temp_path("replace");
    let backup = temp_path("original");
    std::fs::write(&path, synthetic_bik(true, 99, &[0; 4])).unwrap();
    let mut session = Session::open(&path).unwrap();
    std::fs::rename(&path, &backup).unwrap();
    // Same shape, but a valid zero-length audio prefix at the replaced path.
    std::fs::write(&path, synthetic_bik(true, 0, &[0; 4])).unwrap();

    let first = session.next_frame().unwrap_err().to_string();
    assert_eq!(first, "audio prefix exceeds frame packet");
    let second = session.next_frame().unwrap_err().to_string();
    assert_eq!(second, format!("session is faulted: {first}"));
    assert_eq!(session.next_index(), 0);

    std::fs::remove_file(path).unwrap();
    std::fs::remove_file(backup).unwrap();
}

#[test]
fn malformed_video_failure_is_sticky_until_successful_rewind() {
    let path = temp_path("malformed");
    std::fs::write(&path, synthetic_bik(false, 0, &[0xff; 8])).unwrap();
    let mut session = Session::open(&path).unwrap();

    let first = session.next_frame().unwrap_err().to_string();
    let second = session.next_frame().unwrap_err().to_string();
    assert!(second.starts_with("session is faulted:"));
    assert!(second.contains(&first));
    assert_eq!(session.next_index(), 0);

    // Rewind succeeds by constructing a fresh decoder; the next actual decode
    // retries the malformed frame instead of returning the prior sticky fault.
    session.rewind().unwrap();
    let after_rewind = session.next_frame().unwrap_err().to_string();
    assert_eq!(after_rewind, first);
    assert!(
        session
            .next_frame()
            .unwrap_err()
            .to_string()
            .starts_with("session is faulted:")
    );
    std::fs::remove_file(path).unwrap();
}

#[test]
fn memory_and_file_sources_have_matching_header_fault_and_rewind_behavior() {
    let path = temp_path("memory-parity");
    let bytes = synthetic_bik(true, 99, &[0; 4]);
    std::fs::write(&path, &bytes).unwrap();
    let mut file_session = Session::open(&path).unwrap();
    let mut memory_session = Session::from_bytes(Arc::<[u8]>::from(bytes)).unwrap();

    assert_eq!(
        file_session.header().signature,
        memory_session.header().signature
    );
    assert_eq!(
        file_session.header().frame_count,
        memory_session.header().frame_count
    );
    assert_eq!(
        file_session.header().frames.len(),
        memory_session.header().frames.len()
    );
    let file_fault = file_session.next_frame().unwrap_err().to_string();
    let memory_fault = memory_session.next_frame().unwrap_err().to_string();
    assert_eq!(file_fault, "audio prefix exceeds frame packet");
    assert_eq!(file_fault, memory_fault);

    file_session.rewind().unwrap();
    memory_session.rewind().unwrap();
    assert_eq!(file_session.next_index(), memory_session.next_index());
    assert_eq!(
        file_session.next_frame().unwrap_err().to_string(),
        memory_session.next_frame().unwrap_err().to_string()
    );
    std::fs::remove_file(path).unwrap();
}

#[test]
fn memory_source_applies_file_preflight_length_bound() {
    let err = Session::from_bytes(Arc::<[u8]>::from(vec![0u8; 43]))
        .err()
        .unwrap();
    assert_eq!(err.to_string(), "source file size outside candidate bounds");
}

#[test]
fn audio_preroll_seek_preserves_file_identity_and_clears_fault_for_file_and_memory() {
    let path = temp_path("audio-preroll");
    let backup = temp_path("audio-preroll-original");
    let bytes = synthetic_bik(true, 99, &[0; 4]);
    std::fs::write(&path, &bytes).unwrap();
    let mut file_session = Session::open(&path).unwrap();
    let mut memory_session = Session::from_bytes(Arc::<[u8]>::from(bytes)).unwrap();
    let file_failure = file_session.next_frame().unwrap_err().to_string();
    let memory_failure = memory_session.next_frame().unwrap_err().to_string();
    assert_eq!(file_failure, "audio prefix exceeds frame packet");
    assert_eq!(file_failure, memory_failure);

    std::fs::rename(&path, &backup).unwrap();
    std::fs::write(&path, synthetic_bik(true, 0, &[0; 4])).unwrap();
    assert!(
        file_session
            .seek_by_decode_with_audio(0)
            .unwrap()
            .is_empty()
    );
    assert!(
        memory_session
            .seek_by_decode_with_audio(0)
            .unwrap()
            .is_empty()
    );
    // Successful reset clears the sticky fault while preserving each source.
    assert_eq!(
        file_session.next_frame().unwrap_err().to_string(),
        file_failure
    );
    assert_eq!(
        memory_session.next_frame().unwrap_err().to_string(),
        memory_failure
    );
    assert!(file_session.seek_by_decode_with_audio(2).is_err());

    std::fs::remove_file(path).unwrap();
    std::fs::remove_file(backup).unwrap();
}

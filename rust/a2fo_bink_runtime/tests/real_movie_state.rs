use a2fo_bink_candidate::Session;
use a2fo_bink_runtime::state::{AudioSink, Movie};
use infinitier_bik_decoder::{BikPixels, VideoFrame};
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use std::time::Instant;

#[derive(Default, Clone)]
struct CaptureState {
    samples: Vec<i16>,
    position: u64,
    reset_count: usize,
    submit_count: usize,
}

struct CaptureSink(Arc<Mutex<CaptureState>>);

impl AudioSink for CaptureSink {
    fn submit(&mut self, samples: &[i16]) -> Result<(), String> {
        let mut state = self.0.lock().map_err(|e| e.to_string())?;
        state.submit_count += 1;
        state.samples.extend_from_slice(samples);
        Ok(())
    }

    fn position_frames(&mut self) -> Result<u64, String> {
        Ok(self.0.lock().map_err(|e| e.to_string())?.position)
    }

    fn reset(&mut self) -> Result<(), String> {
        let mut state = self.0.lock().map_err(|e| e.to_string())?;
        state.samples.clear();
        state.position = 0;
        state.reset_count += 1;
        Ok(())
    }

    fn set_volume(&mut self, _: u32) -> Result<(), String> {
        Ok(())
    }

    fn service(&mut self) -> Result<(), String> {
        Ok(())
    }
}

struct Reference {
    frame_count: u32,
    fps_num: u32,
    fps_den: u32,
    sample_rate: u32,
    channels: usize,
    samples: Vec<i16>,
    first_frame_samples: Vec<i16>,
    first_image: VideoFrame,
    second_image: VideoFrame,
    third_image: VideoFrame,
    last_image: VideoFrame,
}

fn decode_reference(path: &Path) -> Reference {
    let mut session = Session::open(path).expect("open reference movie");
    let header = session.header().clone();
    let track = header.audio_tracks.first().expect("audio track");
    assert_eq!(
        header.audio_tracks.len(),
        1,
        "test movie must have one audio track"
    );
    let mut samples = Vec::new();
    let mut first_image = None;
    let mut second_image = None;
    let mut third_image = None;
    let mut last_image = None;
    let mut first_frame_samples = Vec::new();
    let mut index = 0u32;
    while let Some(frame) = session.next_frame().expect("decode reference frame") {
        index += 1;
        let image = match frame.video.pixels {
            BikPixels::Yuv(image) => image,
            BikPixels::Rgba(_) => panic!("reference session should return YUV"),
        };
        if index == 1 {
            first_image = Some(image.clone());
        } else if index == 2 {
            second_image = Some(image.clone());
        } else if index == 3 {
            third_image = Some(image.clone());
        }
        if index == header.frame_count {
            last_image = Some(image);
        }
        for chunk in frame.audio {
            assert_eq!(chunk.sample_rate, track.sample_rate as u32);
            assert_eq!(chunk.channels as usize, track.flags.channels() as usize);
            if index == 1 {
                first_frame_samples.extend_from_slice(&chunk.samples);
            }
            samples.extend_from_slice(&chunk.samples);
        }
    }
    assert_eq!(index, header.frame_count);
    Reference {
        frame_count: header.frame_count,
        fps_num: header.fps_num,
        fps_den: header.fps_den,
        sample_rate: track.sample_rate as u32,
        channels: track.flags.channels() as usize,
        samples,
        first_frame_samples,
        first_image: first_image.expect("reference frame one"),
        second_image: second_image.expect("reference frame two"),
        third_image: third_image.expect("reference frame three"),
        last_image: last_image.expect("reference last frame"),
    }
}

fn sample_offset(reference: &Reference, target_frame: u32) -> usize {
    let sample_frames =
        ((target_frame - 1) as u128 * reference.fps_den as u128 * reference.sample_rate as u128)
            / reference.fps_num as u128;
    usize::try_from(sample_frames * reference.channels as u128).unwrap()
}

fn captured(state: &Arc<Mutex<CaptureState>>) -> CaptureState {
    state.lock().unwrap().clone()
}

fn assert_image_equal(actual: Option<&VideoFrame>, expected: &VideoFrame) {
    let actual = actual.expect("decoded render image");
    assert_eq!(actual.y.data, expected.y.data);
    assert_eq!(actual.u.data, expected.u.data);
    assert_eq!(actual.v.data, expected.v.data);
    assert_eq!(
        actual.alpha.as_ref().map(|p| &p.data),
        expected.alpha.as_ref().map(|p| &p.data)
    );
}

fn movie_with_capture(path: &Path) -> (Movie, Arc<Mutex<CaptureState>>) {
    let state = Arc::new(Mutex::new(CaptureState::default()));
    let movie = Movie::new(
        Session::open(path).expect("open movie"),
        0,
        Some(Box::new(CaptureSink(state.clone()))),
    )
    .expect("create movie state");
    (movie, state)
}

fn run_to_end(
    movie: &mut Movie,
    sink: &Arc<Mutex<CaptureState>>,
    first_frame: u32,
    reference: &Reference,
    expected_first_image: &VideoFrame,
) {
    let now = Instant::now();
    for frame_num in first_frame..=reference.frame_count {
        if frame_num != first_frame {
            movie.next_frame(now).expect("advance movie");
        }
        movie.do_frame(now).expect("decode movie frame");
        if frame_num == first_frame {
            assert_image_equal(movie.image(), expected_first_image);
            let submitted = captured(sink);
            movie.do_frame(now).expect("idempotent repeated DoFrame");
            let repeated = captured(sink);
            assert_eq!(repeated.samples, submitted.samples);
            assert_eq!(repeated.submit_count, submitted.submit_count);
        }
    }
}

#[test]
fn actual_audio_movie_goto_skip_and_loop_state_matches_reference() {
    let Some(path) = std::env::var_os("A2FO_BINK_TEST_MOVIE").map(PathBuf::from) else {
        eprintln!("SKIP: set A2FO_BINK_TEST_MOVIE to a valid multiframe audio Bink asset");
        return;
    };
    assert!(
        path.is_file(),
        "A2FO_BINK_TEST_MOVIE is not a file: {}",
        path.display()
    );
    let reference = decode_reference(&path);
    assert!(reference.frame_count >= 3);
    assert!(!reference.samples.is_empty());

    let (mut goto_movie, goto_sink) = movie_with_capture(&path);
    goto_movie
        .goto(3, 0, Instant::now())
        .expect("goto frame three");
    run_to_end(
        &mut goto_movie,
        &goto_sink,
        3,
        &reference,
        &reference.third_image,
    );
    let goto_state = captured(&goto_sink);
    let goto_offset = sample_offset(&reference, 3);
    assert_eq!(goto_state.samples, reference.samples[goto_offset..]);
    assert_image_equal(goto_movie.image(), &reference.last_image);

    // Wrapping the last frame resets captured PCM while retaining the last
    // decoded image until the next DoFrame replaces it with frame one.
    goto_movie
        .next_frame(Instant::now())
        .expect("wrap movie loop");
    let wrapped = captured(&goto_sink);
    assert!(wrapped.samples.is_empty(), "loop reset retained old PCM");
    goto_movie
        .do_frame(Instant::now())
        .expect("decode frame one after wrap");
    let after_wrap = captured(&goto_sink);
    assert_eq!(after_wrap.samples, reference.first_frame_samples);
    assert_image_equal(goto_movie.image(), &reference.first_image);

    // Advancing before the first decode must use the same audio preroll as an
    // explicit seek to frame two.
    let (mut skipped_movie, skipped_sink) = movie_with_capture(&path);
    skipped_movie
        .next_frame(Instant::now())
        .expect("skip frame one before decode");
    run_to_end(
        &mut skipped_movie,
        &skipped_sink,
        2,
        &reference,
        &reference.second_image,
    );
    let skipped_state = captured(&skipped_sink);
    let skipped_offset = sample_offset(&reference, 2);
    assert_eq!(skipped_state.samples, reference.samples[skipped_offset..]);
}

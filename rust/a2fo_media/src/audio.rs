//! Streaming PCM conversion to interleaved 48 kHz stereo S16.
//!
//! A 64-tap Hann-windowed sinc uses 32 source frames of lookahead. Its cutoff
//! is 94% of the output Nyquist limit when downsampling, and the source
//! Nyquist limit otherwise. This trades a narrow transition band for
//! attenuation of aliases when downsampling. The initial and final
//! histories are zero padded, so edges may fade over the filter width. The
//! converter retains at most the filter history after each source frame; a
//! single `push` still allocates its returned output. This is not a channel
//! layout mapper: only mono duplication and independent stereo are defined.

use std::collections::{HashMap, VecDeque};
use std::f64::consts::PI;

const OUTPUT_RATE: u32 = 48_000;
const RADIUS: i64 = 32;
const TAPS: usize = (RADIUS as usize) * 2;
const MAX_PHASES: usize = 1024;

#[derive(Clone, Copy, Debug)]
pub enum PcmFormat {
    S16,
    F32Planar,
}

pub struct AudioSpec {
    pub rate: u32,
    pub channels: usize,
    pub format: PcmFormat,
}

pub struct Converter {
    spec: AudioSpec,
    history: VecDeque<[f64; 2]>,
    history_start: u64,
    source_frames: u64,
    output_frames: u64,
    phase_weights: HashMap<u32, [f64; TAPS]>,
    finished: bool,
}

impl Converter {
    pub fn new(spec: AudioSpec) -> Result<Self, String> {
        if !(8_000..=192_000).contains(&spec.rate) {
            return Err("PCM rate must be 8000..192000 Hz".into());
        }
        if spec.channels != 1 && spec.channels != 2 {
            return Err("PCM must have one or two channels".into());
        }
        Ok(Self {
            spec,
            history: VecDeque::new(),
            history_start: 0,
            source_frames: 0,
            output_frames: 0,
            phase_weights: HashMap::new(),
            finished: false,
        })
    }

    /// Accept frames in `[begin, end)` from a decoded packet of `samples`.
    /// S16 has one interleaved byte plane; F32Planar has one plane per channel.
    pub fn push(
        &mut self,
        planes: &[Vec<u8>],
        samples: usize,
        begin: usize,
        end: usize,
    ) -> Result<Vec<i16>, String> {
        if self.finished {
            return Err("audio converter already finished".into());
        }
        if begin > end || end > samples {
            return Err("invalid PCM sample range".into());
        }
        let (plane_count, bytes_per_plane) = match self.spec.format {
            PcmFormat::S16 => (
                1,
                samples
                    .checked_mul(self.spec.channels)
                    .and_then(|n| n.checked_mul(2)),
            ),
            PcmFormat::F32Planar => (self.spec.channels, samples.checked_mul(4)),
        };
        let bytes_per_plane = bytes_per_plane.ok_or("PCM dimensions overflow")?;
        if planes.len() != plane_count || planes.iter().any(|p| p.len() < bytes_per_plane) {
            return Err("truncated or incorrect PCM planes".into());
        }
        let accepted = end - begin;
        let new_total = self
            .source_frames
            .checked_add(accepted as u64)
            .ok_or("PCM frame count overflow")?;
        if (new_total as u128) * (OUTPUT_RATE as u128) / (self.spec.rate as u128) > u64::MAX as u128
        {
            return Err("resampled frame count overflow".into());
        }
        // Validate the entire selected range before changing stream state.
        if let PcmFormat::F32Planar = self.spec.format {
            for plane in planes {
                for index in begin..end {
                    let offset = index * 4;
                    let value = f32::from_le_bytes(plane[offset..offset + 4].try_into().unwrap());
                    if !value.is_finite() {
                        return Err("non-finite PCM float".into());
                    }
                }
            }
        }
        let mut output = Vec::new();
        for index in begin..end {
            let frame = match self.spec.format {
                PcmFormat::S16 => {
                    let offset = index * self.spec.channels * 2;
                    let left = i16::from_le_bytes(planes[0][offset..offset + 2].try_into().unwrap())
                        as f64
                        / 32768.0;
                    let right = if self.spec.channels == 2 {
                        i16::from_le_bytes(planes[0][offset + 2..offset + 4].try_into().unwrap())
                            as f64
                            / 32768.0
                    } else {
                        left
                    };
                    [left, right]
                }
                PcmFormat::F32Planar => {
                    let offset = index * 4;
                    let left = f32::from_le_bytes(planes[0][offset..offset + 4].try_into().unwrap())
                        .clamp(-1.0, 1.0) as f64;
                    let right = if self.spec.channels == 2 {
                        f32::from_le_bytes(planes[1][offset..offset + 4].try_into().unwrap())
                            .clamp(-1.0, 1.0) as f64
                    } else {
                        left
                    };
                    [left, right]
                }
            };
            self.history.push_back(frame);
            self.source_frames += 1;
            self.emit_available(false, &mut output);
        }
        Ok(output)
    }

    /// Emit the zero-padded tail. A second call is an error.
    pub fn finish(&mut self) -> Result<Vec<i16>, String> {
        if self.finished {
            return Err("audio converter already finished".into());
        }
        self.finished = true;
        let mut output = Vec::new();
        self.emit_available(true, &mut output);
        Ok(output)
    }

    pub fn reset(&mut self) {
        self.history.clear();
        self.history_start = 0;
        self.source_frames = 0;
        self.output_frames = 0;
        self.finished = false;
    }

    fn emit_available(&mut self, eof: bool, output: &mut Vec<i16>) {
        let cutoff = if self.spec.rate > OUTPUT_RATE {
            0.94 * OUTPUT_RATE as f64 / self.spec.rate as f64
        } else {
            1.0
        };
        let final_count =
            (self.source_frames as u128 * OUTPUT_RATE as u128 / self.spec.rate as u128) as u64;
        loop {
            let scaled = self.output_frames as u128 * self.spec.rate as u128;
            let center = (scaled / OUTPUT_RATE as u128) as i64;
            let phase = (scaled % OUTPUT_RATE as u128) as u32;
            if eof {
                if self.output_frames >= final_count {
                    break;
                }
            } else if center as u64 + RADIUS as u64 >= self.source_frames {
                break;
            }
            let first = center - RADIUS + 1;
            let weights = if let Some(cached) = self.phase_weights.get(&phase) {
                *cached
            } else {
                let computed = phase_weights(phase, cutoff);
                if self.phase_weights.len() < MAX_PHASES {
                    self.phase_weights.insert(phase, computed);
                }
                computed
            };
            let mut mixed = [0.0; 2];
            for (tap, weight) in weights.into_iter().enumerate() {
                let index = first + tap as i64;
                if index < 0 || index as u64 >= self.source_frames {
                    continue;
                }
                let Some(frame) = self
                    .history
                    .get((index as u64 - self.history_start) as usize)
                else {
                    continue;
                };
                mixed[0] += frame[0] * weight;
                mixed[1] += frame[1] * weight;
            }
            output.push(to_i16(mixed[0]));
            output.push(to_i16(mixed[1]));
            self.output_frames += 1;
            let next_center =
                (self.output_frames as u128 * self.spec.rate as u128 / OUTPUT_RATE as u128) as i64;
            let keep_from = (next_center - RADIUS + 1).max(0) as u64;
            while self.history_start < keep_from && !self.history.is_empty() {
                self.history.pop_front();
                self.history_start += 1;
            }
        }
    }
}

fn phase_weights(phase: u32, cutoff: f64) -> [f64; TAPS] {
    let fraction = phase as f64 / OUTPUT_RATE as f64;
    std::array::from_fn(|tap| {
        let distance = fraction + (RADIUS - 1 - tap as i64) as f64;
        let window = 0.5 + 0.5 * (PI * distance / RADIUS as f64).cos();
        let argument = PI * distance * cutoff;
        let sinc = if argument.abs() < 1e-12 {
            1.0
        } else {
            argument.sin() / argument
        };
        cutoff * sinc * window
    })
}

fn to_i16(value: f64) -> i16 {
    (value * 32768.0).round().clamp(-32768.0, 32767.0) as i16
}

#[cfg(test)]
mod tests {
    use super::*;

    fn s16(frames: &[[i16; 2]]) -> Vec<u8> {
        frames
            .iter()
            .flat_map(|f| f.iter().flat_map(|v| v.to_le_bytes()))
            .collect()
    }

    fn f32_plane(values: &[f32]) -> Vec<u8> {
        values.iter().flat_map(|v| v.to_le_bytes()).collect()
    }

    #[test]
    fn packet_partitions_and_rates() {
        for rate in [22_050, 24_000, 44_101, 48_000, 96_000] {
            let frames: Vec<_> = (0..901)
                .map(|i| {
                    let x = ((i as f64 * 123.0).sin() * 12000.0) as i16;
                    [x, -x]
                })
                .collect();
            let input = s16(&frames);
            let spec = || AudioSpec {
                rate,
                channels: 2,
                format: PcmFormat::S16,
            };
            let mut whole = Converter::new(spec()).unwrap();
            let mut expected = whole
                .push(&[input.clone()], frames.len(), 0, frames.len())
                .unwrap();
            expected.extend(whole.finish().unwrap());
            let mut split = Converter::new(spec()).unwrap();
            let mut actual = Vec::new();
            for chunk in input.chunks(2 * 2 * 13) {
                actual.extend(
                    split
                        .push(&[chunk.to_vec()], chunk.len() / 4, 0, chunk.len() / 4)
                        .unwrap(),
                );
            }
            actual.extend(split.finish().unwrap());
            assert_eq!(actual, expected, "rate {rate}");
            assert_eq!(
                actual.len(),
                2 * (frames.len() as u64 * 48_000 / rate as u64) as usize
            );
            assert!(split.history.len() <= 66);
        }
    }

    #[test]
    fn phase_cache_is_bounded_and_48k_identity_is_exact() {
        let frames: Vec<_> = (0..2200)
            .map(|i| {
                [
                    ((i * 193) as i16).wrapping_sub(20000),
                    ((i * 311) as i16).wrapping_sub(10000),
                ]
            })
            .collect();
        let mut identity = Converter::new(AudioSpec {
            rate: 48_000,
            channels: 2,
            format: PcmFormat::S16,
        })
        .unwrap();
        let mut output = identity
            .push(&[s16(&frames)], frames.len(), 0, frames.len())
            .unwrap();
        output.extend(identity.finish().unwrap());
        assert_eq!(
            output,
            frames
                .iter()
                .flat_map(|f| f.iter().copied())
                .collect::<Vec<_>>()
        );
        assert_eq!(identity.phase_weights.len(), 1);

        let mut many_phases = Converter::new(AudioSpec {
            rate: 44_101,
            channels: 2,
            format: PcmFormat::S16,
        })
        .unwrap();
        many_phases
            .push(&[s16(&frames)], frames.len(), 0, frames.len())
            .unwrap();
        many_phases.finish().unwrap();
        assert_eq!(many_phases.phase_weights.len(), MAX_PHASES);
    }

    #[test]
    fn trim_before_filter_and_reset() {
        let input = s16(&[
            [30000, 30000],
            [30000, 30000],
            [0, 0],
            [2000, -2000],
            [0, 0],
        ]);
        let spec = || AudioSpec {
            rate: 48_000,
            channels: 2,
            format: PcmFormat::S16,
        };
        let mut trimmed = Converter::new(spec()).unwrap();
        trimmed.push(&[input], 5, 2, 5).unwrap();
        let first = trimmed.finish().unwrap();
        let mut direct = Converter::new(spec()).unwrap();
        direct
            .push(&[s16(&[[0, 0], [2000, -2000], [0, 0]])], 3, 0, 3)
            .unwrap();
        assert_eq!(first, direct.finish().unwrap());
        assert!(trimmed.finish().is_err());
        trimmed.reset();
        trimmed
            .push(&[s16(&[[0, 0], [2000, -2000], [0, 0]])], 3, 0, 3)
            .unwrap();
        assert_eq!(first, trimmed.finish().unwrap());
    }

    #[test]
    fn mono_float_clamps_and_dc_gain() {
        let mut converter = Converter::new(AudioSpec {
            rate: 24_000,
            channels: 1,
            format: PcmFormat::F32Planar,
        })
        .unwrap();
        let mut input = vec![0.25_f32; 256];
        input[0] = 2.0;
        input[255] = -2.0;
        let mut output = converter
            .push(&[f32_plane(&input)], input.len(), 0, input.len())
            .unwrap();
        output.extend(converter.finish().unwrap());
        assert_eq!(output.len(), 1024);
        assert!(output.chunks_exact(2).all(|p| p[0] == p[1]));
        for pair in output.chunks_exact(2).skip(80).take(300) {
            assert!((pair[0] as i32 - 8192).abs() < 20);
        }
    }

    #[test]
    fn stereo_separation_impulse_and_tail() {
        let mut converter = Converter::new(AudioSpec {
            rate: 48_000,
            channels: 2,
            format: PcmFormat::F32Planar,
        })
        .unwrap();
        let mut left = vec![0.0_f32; 80];
        let right = vec![0.0_f32; 80];
        left[40] = 0.5;
        let mut output = converter
            .push(&[f32_plane(&left), f32_plane(&right)], 80, 0, 80)
            .unwrap();
        assert!(!output.is_empty());
        output.extend(converter.finish().unwrap());
        assert_eq!(output.len(), 160);
        assert!(output.chunks_exact(2).all(|p| p[1] == 0));
        assert!((output[80] as i32 - 16384).abs() < 2);
    }

    #[test]
    fn malformed_input_is_rejected_without_state_change() {
        assert!(
            Converter::new(AudioSpec {
                rate: 7_999,
                channels: 1,
                format: PcmFormat::S16
            })
            .is_err()
        );
        assert!(
            Converter::new(AudioSpec {
                rate: 48_000,
                channels: 3,
                format: PcmFormat::S16
            })
            .is_err()
        );
        let mut converter = Converter::new(AudioSpec {
            rate: 48_000,
            channels: 1,
            format: PcmFormat::F32Planar,
        })
        .unwrap();
        assert!(converter.push(&[], 1, 0, 1).is_err());
        assert!(converter.push(&[vec![0; 2]], 1, 0, 1).is_err());
        assert!(converter.push(&[f32_plane(&[f32::NAN])], 1, 0, 1).is_err());
        assert!(converter.push(&[f32_plane(&[0.0])], 1, 1, 0).is_err());
        assert_eq!(converter.source_frames, 0);
        assert!(converter.finish().unwrap().is_empty());
        assert!(converter.push(&[f32_plane(&[0.0])], 1, 0, 1).is_err());
    }

    #[test]
    fn downsampling_attenuates_out_of_band_tone() {
        let rate = 96_000;
        let count = 4096;
        let low: Vec<_> = (0..count)
            .map(|i| (0.8 * (2.0 * PI * 1000.0 * i as f64 / rate as f64).sin()) as f32)
            .collect();
        let high: Vec<_> = (0..count)
            .map(|i| (0.8 * (2.0 * PI * 35_000.0 * i as f64 / rate as f64).sin()) as f32)
            .collect();
        let run = |samples: &[f32]| {
            let mut c = Converter::new(AudioSpec {
                rate,
                channels: 1,
                format: PcmFormat::F32Planar,
            })
            .unwrap();
            let mut out = c
                .push(&[f32_plane(samples)], samples.len(), 0, samples.len())
                .unwrap();
            out.extend(c.finish().unwrap());
            let interior = &out[200..out.len() - 200];
            (interior
                .iter()
                .step_by(2)
                .map(|x| (*x as f64).powi(2))
                .sum::<f64>()
                / (interior.len() / 2) as f64)
                .sqrt()
        };
        assert!(run(&high) < run(&low) * 0.02);
    }
}

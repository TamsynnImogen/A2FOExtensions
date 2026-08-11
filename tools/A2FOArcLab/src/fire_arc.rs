use std::fmt::Write;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum ArcMode {
    Box = 0,
    Cone = 1,
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct ArcConfig {
    mode_raw: i32,
    pub yaw_degrees: f32,
    pub pitch_degrees: f32,
    pub yaw_angle_degrees: f32,
    pub pitch_angle_degrees: f32,
    pub cone_angle_degrees: f32,
}

impl Default for ArcConfig {
    fn default() -> Self {
        Self {
            mode_raw: ArcMode::Box as i32,
            yaw_degrees: 0.0,
            pitch_degrees: 0.0,
            yaw_angle_degrees: 360.0,
            pitch_angle_degrees: 180.0,
            cone_angle_degrees: 90.0,
        }
    }
}

impl ArcConfig {
    pub fn mode(self) -> ArcMode {
        if self.mode_raw == ArcMode::Cone as i32 {
            ArcMode::Cone
        } else {
            ArcMode::Box
        }
    }

    pub fn set_mode(&mut self, mode: ArcMode) {
        self.mode_raw = mode as i32;
    }

    pub fn normalized(mut self) -> Self {
        unsafe { a2fo_arclab_normalize(&mut self) };
        self
    }

    pub fn allows_identity(self, target: [f32; 3]) -> bool {
        const IDENTITY: [f32; 12] = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0];
        unsafe { a2fo_arclab_allows(&self, IDENTITY.as_ptr(), target.as_ptr()) != 0 }
    }

    pub fn snippet(self) -> String {
        let value = self.normalized();
        let mut output = String::new();
        match value.mode() {
            ArcMode::Box => {
                let _ = writeln!(output, "fireArcMode = \"box\"");
                let _ = writeln!(output, "fireArcYaw = {}", format_number(value.yaw_degrees));
                let _ = writeln!(
                    output,
                    "fireArcPitch = {}",
                    format_number(value.pitch_degrees)
                );
                let _ = writeln!(
                    output,
                    "fireArcYawAngle = {}",
                    format_number(value.yaw_angle_degrees)
                );
                let _ = writeln!(
                    output,
                    "fireArcPitchAngle = {}",
                    format_number(value.pitch_angle_degrees)
                );
            }
            ArcMode::Cone => {
                let _ = writeln!(output, "fireArcMode = \"cone\"");
                let _ = writeln!(output, "fireArcYaw = {}", format_number(value.yaw_degrees));
                let _ = writeln!(
                    output,
                    "fireArcPitch = {}",
                    format_number(value.pitch_degrees)
                );
                let _ = writeln!(
                    output,
                    "fireArcAngle = {}",
                    format_number(value.cone_angle_degrees)
                );
            }
        }
        output
    }

    pub fn pitch_limits(self) -> (f32, f32) {
        let value = self.normalized();
        let half = value.pitch_angle_degrees * 0.5;
        (
            (value.pitch_degrees - half).max(-90.0),
            (value.pitch_degrees + half).min(90.0),
        )
    }

    pub fn yaw_limits(self) -> (f32, f32) {
        let value = self.normalized();
        let half = value.yaw_angle_degrees * 0.5;
        (value.yaw_degrees - half, value.yaw_degrees + half)
    }
}

fn format_number(value: f32) -> String {
    if (value - value.round()).abs() < 0.0001 {
        format!("{:.0}", value)
    } else {
        let mut text = format!("{:.3}", value);
        while text.ends_with('0') {
            text.pop();
        }
        if text.ends_with('.') {
            text.pop();
        }
        text
    }
}

extern "C" {
    fn a2fo_arclab_normalize(config: *mut ArcConfig);
    fn a2fo_arclab_allows(
        config: *const ArcConfig,
        owner_matrix: *const f32,
        target_position: *const f32,
    ) -> i32;
}

#[cfg(test)]
mod tests {
    use super::*;

    fn direction(yaw: f32, pitch: f32) -> [f32; 3] {
        let yaw = yaw.to_radians();
        let pitch = pitch.to_radians();
        let cos_pitch = pitch.cos();
        [yaw.sin() * cos_pitch, pitch.sin(), yaw.cos() * cos_pitch]
    }

    #[test]
    fn dorsal_preset_rejects_below() {
        let config = ArcConfig {
            pitch_degrees: 90.0,
            yaw_angle_degrees: 270.0,
            pitch_angle_degrees: 180.0,
            ..Default::default()
        };
        assert!(config.allows_identity(direction(0.0, 0.0)));
        assert!(config.allows_identity(direction(0.0, 80.0)));
        assert!(!config.allows_identity(direction(0.0, -1.0)));
    }

    #[test]
    fn snippets_use_total_width_commands() {
        let config = ArcConfig {
            pitch_degrees: 45.0,
            pitch_angle_degrees: 90.0,
            ..Default::default()
        };
        let snippet = config.snippet();
        assert!(snippet.contains("fireArcPitch = 45"));
        assert!(snippet.contains("fireArcPitchAngle = 90"));
    }
}

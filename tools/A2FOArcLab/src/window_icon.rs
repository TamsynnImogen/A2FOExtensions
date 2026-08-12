use bevy::prelude::*;
use bevy::window::PrimaryWindow;
use bevy::winit::WinitWindows;
use winit::window::Icon;

const ICON_SIZE: u32 = 64;
const SUPERSAMPLE: u32 = 4;

pub fn install_window_icon(
    primary_window: Query<Entity, With<PrimaryWindow>>,
    winit_windows: NonSend<WinitWindows>,
) {
    let Ok(entity) = primary_window.get_single() else {
        return;
    };
    let Some(window) = winit_windows.get_window(entity) else {
        return;
    };
    let Ok(icon) = Icon::from_rgba(arc_lab_icon_rgba(), ICON_SIZE, ICON_SIZE) else {
        return;
    };
    // Winit applies this to title bars and task switchers on Windows and X11.
    // Wayland compositors intentionally own their application-icon policy, so
    // the same call is a harmless no-op there.
    window.set_window_icon(Some(icon));
}

fn arc_lab_icon_rgba() -> Vec<u8> {
    let mut output = Vec::with_capacity((ICON_SIZE * ICON_SIZE * 4) as usize);
    for pixel_y in 0..ICON_SIZE {
        for pixel_x in 0..ICON_SIZE {
            let mut accumulated = [0.0f32; 4];
            for sample_y in 0..SUPERSAMPLE {
                for sample_x in 0..SUPERSAMPLE {
                    let x = ((pixel_x as f32 + (sample_x as f32 + 0.5) / SUPERSAMPLE as f32)
                        / ICON_SIZE as f32)
                        * 2.0
                        - 1.0;
                    let y = ((pixel_y as f32 + (sample_y as f32 + 0.5) / SUPERSAMPLE as f32)
                        / ICON_SIZE as f32)
                        * 2.0
                        - 1.0;
                    let sample = icon_sample(x, y);
                    for channel in 0..4 {
                        accumulated[channel] += sample[channel];
                    }
                }
            }
            let divisor = (SUPERSAMPLE * SUPERSAMPLE) as f32;
            for channel in accumulated {
                output.push(((channel / divisor).clamp(0.0, 1.0) * 255.0).round() as u8);
            }
        }
    }
    output
}

fn icon_sample(x: f32, y: f32) -> [f32; 4] {
    let qx = x.abs() - 0.70;
    let qy = y.abs() - 0.70;
    let outside_x = qx.max(0.0);
    let outside_y = qy.max(0.0);
    let rounded_box_distance =
        (outside_x * outside_x + outside_y * outside_y).sqrt() + qx.max(qy).min(0.0) - 0.18;
    if rounded_box_distance > 0.0 {
        return [0.0, 0.0, 0.0, 0.0];
    }

    let mut colour = [0.025, 0.055 + (y + 1.0) * 0.018, 0.11, 1.0];
    if rounded_box_distance > -0.045 {
        colour = [0.08, 0.72, 0.92, 1.0];
    }

    let origin_x = -0.37;
    let dx = x - origin_x;
    let radius = (dx * dx + y * y).sqrt();
    let angle = y.atan2(dx).abs();
    let boundary_ray = dx >= 0.0 && radius <= 1.10 && (angle - 0.62).abs() < 0.042;
    let boundary_curve = dx >= 0.0 && angle <= 0.62 && (radius - 1.08).abs() < 0.042;
    if boundary_ray || boundary_curve {
        colour = [0.08, 0.88, 1.0, 1.0];
    }
    if dx >= 0.02 && dx <= 0.86 && y.abs() < 0.026 {
        colour = [1.0, 0.80, 0.10, 1.0];
    }

    let target_dx = x - 0.48;
    let target_radius = (target_dx * target_dx + y * y).sqrt();
    if target_radius < 0.105 {
        colour = [0.12, 1.0, 0.28, 1.0];
    } else if target_radius < 0.15 {
        colour = [0.04, 0.42, 0.20, 1.0];
    }

    let ship_dx = x + 0.50;
    let ship_body = (-0.08..=0.34).contains(&ship_dx) && y.abs() <= (ship_dx + 0.08) * 0.43;
    let ship_tail = (-0.12..=0.08).contains(&ship_dx) && y.abs() < 0.10;
    if ship_body || ship_tail {
        colour = [0.90, 0.96, 1.0, 1.0];
    }
    colour
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn generated_icon_has_valid_rgba_dimensions_and_transparency() {
        let icon = arc_lab_icon_rgba();
        assert_eq!(icon.len(), (ICON_SIZE * ICON_SIZE * 4) as usize);
        assert!(icon.chunks_exact(4).any(|pixel| pixel[3] == 0));
        assert!(icon.chunks_exact(4).any(|pixel| pixel[3] == 255));
    }
}

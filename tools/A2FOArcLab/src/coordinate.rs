use bevy::prelude::*;

/// Armada SODs use +X right, +Y up, and +Z forward. Bevy is also Y-up, but
/// its right-handed view space has forward on -Z. Reflecting the forward axis
/// preserves Armada's port/starboard meaning while correcting model chirality.
pub const RIGHT: Vec3 = Vec3::X;
pub const UP: Vec3 = Vec3::Y;
pub const FORWARD: Vec3 = Vec3::NEG_Z;

pub fn armada_to_viewer_vector(value: Vec3) -> Vec3 {
    Vec3::new(value.x, value.y, -value.z)
}

/// Convert an Armada local transform without introducing a reflected scale.
/// Both the parent space and child-local space change basis, hence C * M * C.
pub fn armada_to_viewer_matrix(value: Mat4) -> Mat4 {
    let conversion = Mat4::from_scale(Vec3::new(1.0, 1.0, -1.0));
    conversion * value * conversion
}

#[cfg(test)]
mod tests {
    use super::*;

    fn assert_vec3_close(actual: Vec3, expected: Vec3) {
        assert!(
            actual.abs_diff_eq(expected, 0.0001),
            "expected {expected:?}, got {actual:?}"
        );
    }

    #[test]
    fn armada_axes_keep_right_and_up_but_map_forward_to_bevy_forward() {
        assert_eq!(armada_to_viewer_vector(Vec3::X), RIGHT);
        assert_eq!(armada_to_viewer_vector(Vec3::Y), UP);
        assert_eq!(armada_to_viewer_vector(Vec3::Z), FORWARD);
    }

    #[test]
    fn matrix_conversion_matches_point_conversion_through_a_hierarchy() {
        let parent = Mat4::from_scale_rotation_translation(
            Vec3::splat(1.25),
            Quat::from_euler(EulerRot::XYZ, 0.2, -0.4, 0.1),
            Vec3::new(4.0, -2.0, 7.0),
        );
        let child = Mat4::from_rotation_translation(
            Quat::from_euler(EulerRot::XYZ, -0.3, 0.15, 0.5),
            Vec3::new(-1.0, 3.0, 2.0),
        );
        let point = Vec3::new(2.0, 5.0, -4.0);

        let armada_world_point = (parent * child).transform_point3(point);
        let viewer_world = armada_to_viewer_matrix(parent) * armada_to_viewer_matrix(child);
        let viewer_point = viewer_world.transform_point3(armada_to_viewer_vector(point));

        assert_vec3_close(viewer_point, armada_to_viewer_vector(armada_world_point));
    }
}

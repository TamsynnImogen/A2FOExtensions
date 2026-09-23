use crate::document::editor_to_bytes;
use crate::glyphs::{rewrite_glyph_keyframes, FontCatalog};
use anyhow::{bail, Context, Result};
use image::codecs::png::PngEncoder;
use image::codecs::tga::TgaEncoder;
use image::{ColorType, ImageEncoder, ImageFormat, Rgba, RgbaImage};
use std::ffi::OsString;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub struct GlyphBuildRequest {
    pub byte: u8,
    pub label: String,
    pub phrase: String,
}

#[derive(Debug, Clone)]
pub struct GlyphBuildResult {
    pub byte: u8,
    pub x: u32,
    pub y: u32,
    pub width: u32,
    pub spr_backup: PathBuf,
    pub texture_backup: PathBuf,
}

pub fn build_phrase_glyph(
    catalog: &FontCatalog,
    request: &GlyphBuildRequest,
) -> Result<GlyphBuildResult> {
    if request.byte < 0x7f {
        bail!("Custom glyph codes must be in the 0x7F through 0xFF range");
    }
    if request.label.trim().is_empty() {
        bail!("Give the new glyph a readable label");
    }
    if request.phrase.trim().is_empty() {
        bail!("Type the phrase that the new glyph should display");
    }
    let phrase = editor_to_bytes(&request.phrase, b"\n")
        .context("The glyph phrase contains unsupported text")?;
    if phrase
        .iter()
        .any(|byte| matches!(byte, b'\r' | b'\n' | b'\t'))
    {
        bail!("A code glyph must be a single line and cannot contain tabs");
    }

    let format = texture_format(&catalog.texture_path)?;
    let texture_bytes = fs::read(&catalog.texture_path)
        .with_context(|| format!("Read atlas {}", catalog.texture_path.display()))?;
    let mut atlas = image::load_from_memory_with_format(&texture_bytes, format)
        .with_context(|| format!("Decode atlas {}", catalog.texture_path.display()))?
        .into_rgba8();
    if atlas.width() != catalog.reference_width || atlas.height() != catalog.reference_height {
        bail!(
            "Atlas is {}x{}, but FontSmall.spr declares {}x{}; editing a scaled atlas is not safe",
            atlas.width(),
            atlas.height(),
            catalog.reference_width,
            catalog.reference_height
        );
    }

    let mut source_glyphs = Vec::with_capacity(phrase.len());
    let mut cursor = 0.0f32;
    let mut render_width = 0u32;
    for byte in phrase {
        let glyph = catalog
            .glyph(byte)
            .with_context(|| format!("The active font does not define byte 0x{byte:02X}"))?;
        if glyph.x + glyph.width > atlas.width() || glyph.y + glyph.height > atlas.height() {
            bail!("Source glyph 0x{byte:02X} lies outside the font atlas");
        }
        let x = cursor.round() as u32;
        render_width = render_width.max(x + glyph.width);
        source_glyphs.push((glyph.clone(), x));
        cursor += glyph.advance;
    }
    let advance = cursor.ceil().max(1.0);
    let width = render_width.max(advance as u32).max(1);
    let height = catalog.line_height.max(1);
    if width > atlas.width() || height > atlas.height() {
        bail!("The composed glyph is {width}x{height}, larger than the font atlas");
    }
    let (x, y) = find_free_rectangle(&atlas, catalog, width, height)
        .context("The font atlas has no transparent space large enough for this glyph")?;

    let original = atlas.clone();
    for (glyph, destination_x) in source_glyphs {
        if glyph.byte == b' ' {
            continue;
        }
        for source_y in 0..glyph.height.min(height) {
            for source_x in 0..glyph.width {
                let pixel = *original.get_pixel(glyph.x + source_x, glyph.y + source_y);
                if pixel.0[3] == 0 {
                    continue;
                }
                let target_x = x + destination_x + source_x;
                if target_x < x + width {
                    blend_pixel(&mut atlas, target_x, y + source_y, pixel);
                }
            }
        }
    }

    let encoded_atlas = encode_texture(&atlas, format)?;
    let updated_spr = rewrite_glyph_keyframes(
        &catalog.spr_path,
        request.byte,
        x,
        y,
        width as f32,
        request.label.trim(),
    )?;
    let spr_backup = backup_path(&catalog.spr_path);
    let texture_backup = backup_path(&catalog.texture_path);
    create_backup(&catalog.spr_path, &spr_backup)?;
    create_backup(&catalog.texture_path, &texture_backup)?;
    fs::write(&catalog.texture_path, encoded_atlas)
        .with_context(|| format!("Write atlas {}", catalog.texture_path.display()))?;
    fs::write(&catalog.spr_path, updated_spr)
        .with_context(|| format!("Write sprite {}", catalog.spr_path.display()))?;

    Ok(GlyphBuildResult {
        byte: request.byte,
        x,
        y,
        width,
        spr_backup,
        texture_backup,
    })
}

fn texture_format(path: &Path) -> Result<ImageFormat> {
    match path
        .extension()
        .and_then(|extension| extension.to_str())
        .map(str::to_ascii_lowercase)
        .as_deref()
    {
        Some("tga") => Ok(ImageFormat::Tga),
        Some("png") => Ok(ImageFormat::Png),
        Some("dds") => bail!(
            "DDS font atlases can be previewed but not edited; convert or select a TGA/PNG font atlas"
        ),
        _ => bail!("Glyph Builder supports TGA and PNG font atlases"),
    }
}

fn encode_texture(image: &RgbaImage, format: ImageFormat) -> Result<Vec<u8>> {
    let mut output = Vec::new();
    match format {
        ImageFormat::Tga => TgaEncoder::new(&mut output).write_image(
            image.as_raw(),
            image.width(),
            image.height(),
            ColorType::Rgba8,
        )?,
        ImageFormat::Png => PngEncoder::new(&mut output).write_image(
            image.as_raw(),
            image.width(),
            image.height(),
            ColorType::Rgba8,
        )?,
        _ => unreachable!("validated format"),
    }
    Ok(output)
}

fn find_free_rectangle(
    atlas: &RgbaImage,
    catalog: &FontCatalog,
    width: u32,
    height: u32,
) -> Option<(u32, u32)> {
    let max_x = atlas.width().checked_sub(width)?;
    let max_y = atlas.height().checked_sub(height)?;
    for y in (0..=max_y).rev() {
        for x in 0..=max_x {
            if catalog.glyphs.values().any(|glyph| {
                rectangles_intersect(
                    x,
                    y,
                    width,
                    height,
                    glyph.x,
                    glyph.y,
                    glyph.width,
                    glyph.height,
                )
            }) {
                continue;
            }
            if (y..y + height)
                .all(|row| (x..x + width).all(|column| atlas.get_pixel(column, row).0[3] == 0))
            {
                return Some((x, y));
            }
        }
    }
    None
}

#[allow(clippy::too_many_arguments)]
fn rectangles_intersect(
    left_x: u32,
    left_y: u32,
    left_width: u32,
    left_height: u32,
    right_x: u32,
    right_y: u32,
    right_width: u32,
    right_height: u32,
) -> bool {
    left_x < right_x.saturating_add(right_width)
        && right_x < left_x.saturating_add(left_width)
        && left_y < right_y.saturating_add(right_height)
        && right_y < left_y.saturating_add(left_height)
}

fn blend_pixel(atlas: &mut RgbaImage, x: u32, y: u32, source: Rgba<u8>) {
    let destination = *atlas.get_pixel(x, y);
    let source_alpha = source.0[3] as f32 / 255.0;
    let destination_alpha = destination.0[3] as f32 / 255.0;
    let output_alpha = source_alpha + destination_alpha * (1.0 - source_alpha);
    if output_alpha <= f32::EPSILON {
        atlas.put_pixel(x, y, Rgba([0, 0, 0, 0]));
        return;
    }
    let mut output = [0u8; 4];
    for (channel, output_channel) in output.iter_mut().enumerate().take(3) {
        *output_channel = (((source.0[channel] as f32 * source_alpha)
            + (destination.0[channel] as f32 * destination_alpha * (1.0 - source_alpha)))
            / output_alpha)
            .round() as u8;
    }
    output[3] = (output_alpha * 255.0).round() as u8;
    atlas.put_pixel(x, y, Rgba(output));
}

fn create_backup(source: &Path, backup: &Path) -> Result<()> {
    if !backup.exists() {
        fs::copy(source, backup).with_context(|| format!("Create backup {}", backup.display()))?;
    }
    Ok(())
}

fn backup_path(path: &Path) -> PathBuf {
    let mut name = path
        .file_name()
        .map(OsString::from)
        .unwrap_or_else(|| OsString::from("font-asset"));
    name.push(".a2fo-tooltip-lab.bak");
    path.with_file_name(name)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::glyphs::FontGlyph;
    use std::collections::BTreeMap;

    #[test]
    fn allocator_avoids_existing_glyph_rectangles_and_pixels() {
        let mut atlas = RgbaImage::new(20, 20);
        atlas.put_pixel(0, 16, Rgba([255, 255, 255, 255]));
        let mut glyphs = BTreeMap::new();
        glyphs.insert(
            b'A',
            FontGlyph {
                byte: b'A',
                x: 0,
                y: 10,
                width: 8,
                advance: 8.0,
                height: 4,
                label: None,
            },
        );
        let catalog = FontCatalog {
            spr_path: PathBuf::new(),
            texture_path: PathBuf::new(),
            reference_width: 20,
            reference_height: 20,
            line_height: 4,
            glyphs,
            roots: Vec::new(),
            warnings: Vec::new(),
        };
        assert_eq!(find_free_rectangle(&atlas, &catalog, 8, 4), Some((1, 16)));
    }

    #[test]
    fn builds_a_phrase_and_preserves_original_asset_backups() {
        let root = std::env::temp_dir().join(format!(
            "a2fo-tooltip-glyph-build-{}-{}",
            std::process::id(),
            std::thread::current().name().unwrap_or("test")
        ));
        fs::create_dir_all(&root).unwrap();
        let spr_path = root.join("FontSmall.spr");
        let texture_path = root.join("atlas.tga");
        let mut spr = String::from(
            "@referenceWidth=64\n@referenceHeight=32\n@animation font_uv\n@keyframes\n",
        );
        for _ in 0..224 {
            spr.push_str("0 0\n");
        }
        spr.push_str("@animation font_w\n@keyframes\n");
        for _ in 0..224 {
            spr.push_str("4\n");
        }
        spr.push_str("page atlas 0 0 0 8 @anim=font_uv @anim=font_w\n");
        fs::write(&spr_path, &spr).unwrap();

        let mut atlas = RgbaImage::new(64, 32);
        atlas.put_pixel(0, 0, Rgba([255, 10, 10, 255]));
        atlas.put_pixel(4, 0, Rgba([10, 255, 10, 255]));
        fs::write(
            &texture_path,
            encode_texture(&atlas, ImageFormat::Tga).unwrap(),
        )
        .unwrap();
        let glyphs = [
            FontGlyph {
                byte: b'A',
                x: 0,
                y: 0,
                width: 3,
                advance: 4.0,
                height: 8,
                label: None,
            },
            FontGlyph {
                byte: b'B',
                x: 4,
                y: 0,
                width: 3,
                advance: 4.0,
                height: 8,
                label: None,
            },
        ]
        .into_iter()
        .map(|glyph| (glyph.byte, glyph))
        .collect();
        let catalog = FontCatalog {
            spr_path: spr_path.clone(),
            texture_path: texture_path.clone(),
            reference_width: 64,
            reference_height: 32,
            line_height: 8,
            glyphs,
            roots: vec![root.clone()],
            warnings: Vec::new(),
        };
        let result = build_phrase_glyph(
            &catalog,
            &GlyphBuildRequest {
                byte: 0x80,
                label: "AB phrase".to_string(),
                phrase: "AB".to_string(),
            },
        )
        .unwrap();
        assert_eq!(result.width, 8);
        assert!(result.spr_backup.is_file());
        assert!(result.texture_backup.is_file());
        assert_eq!(fs::read_to_string(&result.spr_backup).unwrap(), spr);
        let updated = fs::read_to_string(&spr_path).unwrap();
        assert!(updated.contains(&format!("{}\t{}\t# AB phrase", result.x, result.y)));
        assert!(updated.contains("8\t# AB phrase"));
        assert!(updated.contains("\r\n"));
        assert!(!updated.replace("\r\n", "").contains('\n'));
        let rendered = image::load_from_memory_with_format(
            &fs::read(&texture_path).unwrap(),
            ImageFormat::Tga,
        )
        .unwrap()
        .into_rgba8();
        assert_eq!(rendered.get_pixel(result.x, result.y).0[0], 255);
        assert_eq!(rendered.get_pixel(result.x + 4, result.y).0[1], 255);
        fs::remove_dir_all(root).ok();
    }
}

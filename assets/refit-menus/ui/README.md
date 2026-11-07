# Refit menu component PNGs

These original, generated RGBA assets supply the Instant Action panel, header,
button and dropdown artwork. They follow the existing Refit-inspired navy/amber
style. No game images, user names or dynamic selections are baked into them.

The cut corners are fully transparent. Panel interiors are translucent, leaving
the animated scene visible beneath them. Dropdown rows have opaque interiors so
the underlying row text does not show through. Text, colours and map previews
remain live. Empty status panes have no frame or fill.

Each source tile is 64×64 with eight-pixel nine-slice margins. `atlas.json`
records the tile order and geometry. Edit the eight named source PNGs to change
the skin, retaining RGBA format and dimensions. Packing preserves those edits:

```sh
python3 tools/generate_refit_menu_skin.py --pack
python3 tools/generate_refit_menu_skin.py --pack --check
make refit-menus
```

Python and Pillow are build-time requirements. The default generator command
without `--pack` recreates the reference source tiles; use it only to reset the
generated design. `--check` never writes files.

The build embeds the same pixels as a premultiplied BGRA resource. The runtime
caches resized components per UI thread and alpha-blends each in one call. It
does not decode PNG files during menu animation. The cache is bounded to 96
entries and 8 million pixels. A skin/resource failure retains the earlier GDI
control artwork. Fonts and native click handling are separate.

These are shared components, currently connected to the Instant Action layout.
They do not replace every other screen's native widget layout. Rebuild and fully
restart the game after changing the PNGs. They are source assets, not loose-file
overrides for the game's `bitmaps/` directory.

Keep the project's existing MIT attribution to Cedric Haase's armada2-refit and
the separate Noto font OFL notices. No third-party images were added here.

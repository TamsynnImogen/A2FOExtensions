#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source_dir="$root/third_party/armada2-refit/menu-renderer"
output_dir="$root/build/modules/refit"
resources="$root/build/refit-scene-resources"
toolchain=${REFIT_RUST_TOOLCHAIN:-1.96.0}

sysroot=$(rustup run "$toolchain" rustc --print sysroot)
host_triple=$(rustup run "$toolchain" rustc -vV | sed -n 's/^host: //p')
linker="$sysroot/lib/rustlib/$host_triple/bin/rust-lld"
if [[ ! -x "$linker" ]]; then
    printf 'Missing rust-lld: %s\n' "$linker" >&2
    exit 1
fi

(
    cd "$source_dir"
    CARGO_TARGET_I686_PC_WINDOWS_MSVC_LINKER="$linker" \
    RUSTFLAGS='-C target-feature=+sse2 -C linker-flavor=lld-link -C link-arg=/nodefaultlib -C link-arg=/entry:DllMain' \
        rustup run "$toolchain" cargo build --offline --locked --no-default-features --features a2fo-menu-host --release --target i686-pc-windows-msvc
)

mkdir -p "$output_dir" "$resources" "$output_dir/licenses"
cp "$source_dir/target/i686-pc-windows-msvc/release/render3d.dll" "$output_dir/A2FORefitScene.dll"
# Small menu-only bake targets; the original mission-renderer defaults cause
# multi-second volume bakes and have no place on the shell's UI thread.
cp "$source_dir/a2fo-menu.ini" "$resources/Render3D.ini"
cp "$source_dir/ordnance.ini" "$resources/"
cp "$source_dir/LICENSE" "$output_dir/licenses/armada2-refit.txt"
cp "$root/third_party/armada2-refit/libm-0.2.16-LICENSE.txt" "$output_dir/licenses/libm-0.2.16.txt"
for folder in menus skies hulls surfaces nebulae; do
    if [[ -d "$source_dir/$folder" ]]; then
        destination=$folder
        if [[ "$folder" == menus ]]; then destination=Menu; fi
        mkdir -p "$resources/$destination"
        find "$source_dir/$folder" -maxdepth 1 -type f -name '*.ini' -exec cp -t "$resources/$destination" -- {} +
    fi
done
printf 'Built %s\nStaged config %s\n' "$output_dir/A2FORefitScene.dll" "$resources"

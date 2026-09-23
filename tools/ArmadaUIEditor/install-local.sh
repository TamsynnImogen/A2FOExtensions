#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/armada-ui-editor"
BIN_DIR="${HOME}/.local/bin"
APPLICATION_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"

cargo build --release --locked --manifest-path "$SCRIPT_DIR/Cargo.toml"
mkdir -p "$INSTALL_DIR" "$BIN_DIR" "$APPLICATION_DIR"
cp "$SCRIPT_DIR/target/release/armada_ui_editor" "$INSTALL_DIR/armada-ui-editor"
ln -sfn "$INSTALL_DIR/armada-ui-editor" "$BIN_DIR/armada-ui-editor"
cp "$SCRIPT_DIR/armada-ui-editor.desktop" \
    "$APPLICATION_DIR/armada-ui-editor.desktop"

echo "Installed: $INSTALL_DIR/armada-ui-editor"
echo "Command:   $BIN_DIR/armada-ui-editor"
echo "Desktop:   $APPLICATION_DIR/armada-ui-editor.desktop"

#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tool_dir="$(cd "${script_dir}/.." && pwd)"
repo_dir="$(cd "${tool_dir}/../.." && pwd)"
package_dir="${repo_dir}/dist/A2FOArcLab-linux-x86_64"

cargo build --release --locked --manifest-path "${tool_dir}/Cargo.toml"
mkdir -p "${package_dir}"
cp "${tool_dir}/target/release/a2fo_arclab" "${package_dir}/A2FOArcLab"
cp "${tool_dir}/README.md" "${package_dir}/README.md"
tar -C "${repo_dir}/dist" -czf \
    "${repo_dir}/dist/A2FOArcLab-linux-x86_64.tar.gz" \
    "A2FOArcLab-linux-x86_64"

echo "Created ${repo_dir}/dist/A2FOArcLab-linux-x86_64.tar.gz"

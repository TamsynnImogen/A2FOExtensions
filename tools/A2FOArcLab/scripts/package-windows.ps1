$ErrorActionPreference = "Stop"

$ToolDir = Split-Path -Parent $PSScriptRoot
$RepoDir = (Resolve-Path (Join-Path $ToolDir "..\..")).Path
$PackageDir = Join-Path $RepoDir "dist\A2FOArcLab-windows-x86_64"
$Archive = Join-Path $RepoDir "dist\A2FOArcLab-windows-x86_64.zip"

cargo build --release --locked --manifest-path (Join-Path $ToolDir "Cargo.toml")
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null
Copy-Item (Join-Path $ToolDir "target\release\a2fo_arclab.exe") (Join-Path $PackageDir "A2FOArcLab.exe") -Force
Copy-Item (Join-Path $ToolDir "README.md") $PackageDir -Force
if (Test-Path $Archive) {
    Remove-Item $Archive -Force
}
Compress-Archive -Path $PackageDir -DestinationPath $Archive
Write-Host "Created $Archive"

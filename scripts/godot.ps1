#!/usr/bin/env pwsh
# Launch the pinned Godot 4.6 editor against godot/ (not the repo root).
param(
    [switch]$Run,
    [switch]$HeadlessImport
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $Root "godot"
$PathFile = Join-Path $Root "tools/godot/editor.path"
$Setup = Join-Path $PSScriptRoot "setup_godot.ps1"

function Resolve-Godot {
    if (Test-Path $PathFile) {
        $p = (Get-Content $PathFile -Raw).Trim()
        if ($p -and (Test-Path $p)) { return $p }
    }
    $candidates = @(
        (Join-Path $Root "tools/godot/bin/Godot_v4.6-stable_win64_console.exe"),
        (Join-Path $Root "tools/godot/bin/Godot_v4.6-stable_win64.exe"),
        (Join-Path $Root "tools/godot/bin/Godot_v4.6-stable_windows_arm64_console.exe"),
        (Join-Path $Root "tools/godot/bin/godot")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }
    return $null
}

$Godot = Resolve-Godot
if (-not $Godot) {
    Write-Host "Pinned Godot missing; running setup..."
    & $Setup
    $Godot = Resolve-Godot
}
if (-not $Godot) {
    throw "Godot editor not found. Run powershell -File scripts/setup_godot.ps1"
}

$passthrough = @()
$seenSep = $false
foreach ($a in $args) {
    if ($a -eq "--") { $seenSep = $true; continue }
    $passthrough += $a
}

if ($HeadlessImport) {
    # --import already quits. Adding --quit crashes Godot 4.6-stable on Windows.
    & $Godot --headless --path $Project --import @passthrough
    exit $LASTEXITCODE
}

$mode = if ($Run) { @("--path", $Project) } else { @("--editor", "--path", $Project) }
Write-Host "Using $Godot"
& $Godot @mode @passthrough
exit $LASTEXITCODE

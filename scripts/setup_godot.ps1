#!/usr/bin/env pwsh
# Download the pinned Godot 4.6-stable editor into tools/godot/bin.
# Override platform with -Platform windows-x86_64 | windows-arm64 | linux-x86_64 | linux-arm64 | macos-universal
param(
    [string]$Platform = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$PinPath = Join-Path $Root "tools/godot/pin.json"
$BinDir = Join-Path $Root "tools/godot/bin"
$CacheDir = Join-Path $Root "tools/godot/cache"
$PathFile = Join-Path $Root "tools/godot/editor.path"

if (-not (Test-Path $PinPath)) {
    throw "Missing $PinPath"
}
$pin = Get-Content -Raw -Path $PinPath | ConvertFrom-Json

function Get-PlatformKey {
    if ($Platform) { return $Platform }
    $isWindows = [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
        [System.Runtime.InteropServices.OSPlatform]::Windows)
    $isOsx = [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
        [System.Runtime.InteropServices.OSPlatform]::OSX)
    $arch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString().ToLowerInvariant()
    if ($isOsx) { return "macos-universal" }
    if ($isWindows) {
        if ($arch -eq "arm64") { return "windows-arm64" }
        return "windows-x86_64"
    }
    if ($arch -eq "arm64") { return "linux-arm64" }
    return "linux-x86_64"
}

function Test-ExistingEditor([string]$exe) {
    if (-not (Test-Path $exe)) { return $false }
    try {
        $ver = & $exe --version 2>&1 | Out-String
        return $ver -match "4\.6"
    } catch {
        return $false
    }
}

function Write-EditorPath([string]$exe) {
    Set-Content -Path $PathFile -Value $exe -Encoding ascii -NoNewline
    $cmd = Join-Path $BinDir "godot.cmd"
    @"
@echo off
"$exe" %*
"@ | Set-Content -Path $cmd -Encoding ascii
}

$key = Get-PlatformKey
$spec = $pin.downloads.$key
if (-not $spec) {
    throw "No download in pin.json for platform '$key'"
}

New-Item -ItemType Directory -Force -Path $BinDir | Out-Null
New-Item -ItemType Directory -Force -Path $CacheDir | Out-Null

$preferred = $spec.binary
if ($spec.PSObject.Properties.Name -contains "console" -and $spec.console) {
    $preferred = $spec.console
}
if ($key.StartsWith("macos")) {
    $editor = Join-Path $BinDir $spec.binary
} else {
    $editor = Join-Path $BinDir $preferred
}

if (-not $Force -and (Test-ExistingEditor $editor)) {
    $stale = Join-Path $BinDir "godot.exe"
    if (Test-Path $stale) { Remove-Item -Force $stale }
    Write-EditorPath $editor
    Write-Host "Godot $($pin.engine) already installed: $editor"
    & $editor --version
    exit 0
}

$zip = Join-Path $CacheDir $spec.archive
$needDownload = $true
if ((Test-Path $zip) -and -not $Force) {
    $cached = (Get-FileHash -Path $zip -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($cached -eq $spec.sha256) { $needDownload = $false }
}
if ($needDownload) {
    Write-Host "Downloading $($spec.url)"
    Invoke-WebRequest -Uri $spec.url -OutFile $zip -UseBasicParsing
}
$actual = (Get-FileHash -Path $zip -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $spec.sha256) {
    throw "SHA256 mismatch for $($spec.archive): expected $($spec.sha256) got $actual"
}

$extract = Join-Path $CacheDir "extract"
if (Test-Path $extract) { Remove-Item -Recurse -Force $extract }
Expand-Archive -Path $zip -DestinationPath $extract -Force

Get-ChildItem -Path $extract -Recurse -File | ForEach-Object {
    Copy-Item -Force $_.FullName (Join-Path $BinDir $_.Name)
}
# Official Windows builds refuse a renamed wrapper (godot.exe -> "Invalid wrapper executable name").
$stale = Join-Path $BinDir "godot.exe"
if (Test-Path $stale) { Remove-Item -Force $stale }

if ($key.StartsWith("macos")) {
    $app = Get-ChildItem -Path $extract -Filter "Godot.app" -Directory -Recurse | Select-Object -First 1
    if ($app) {
        $destApp = Join-Path $BinDir "Godot.app"
        if (Test-Path $destApp) { Remove-Item -Recurse -Force $destApp }
        Copy-Item -Recurse -Force $app.FullName $destApp
        $editor = Join-Path $destApp "Contents/MacOS/Godot"
    }
}

if (-not (Test-Path $editor)) {
    throw "Extracted archive did not contain $preferred"
}

if (-not $key.StartsWith("windows")) {
    & chmod +x $editor 2>$null
}

Write-EditorPath $editor
Write-Host "Installed Godot $($pin.engine) -> $editor"
& $editor --version
Write-Host "Launch the project with: powershell -File scripts/godot.ps1"

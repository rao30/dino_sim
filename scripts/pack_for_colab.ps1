# Pack a Drive/Colab-sized source zip (no Godot binaries, no local runs).
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root "train\runs"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$zip = Join-Path $outDir "dino_sim_colab.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }

$stage = Join-Path $env:TEMP "dino_sim_colab_pack"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }

robocopy $root $stage /E /NFL /NDL /NJH /NJS /nc /ns /np `
    /XD build build-godot .git .venv __pycache__ .godot bin cache snapshots runs | Out-Null

Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -Force
Remove-Item $stage -Recurse -Force
Write-Host "Upload this zip to Google Drive, then open train/colab_train.ipynb:"
Write-Host $zip

---
name: godot-harness
description: Installs and runs the pinned Godot 4.6 editor for dino_sim, builds the GDExtension, and keeps island sim logic in C++. Use when setting up Godot, opening the editor, editing godot/ scenes or GDScript, building dino_godot, or changing src/godot/.
---

# Godot 4.6 harness

## Pin

This repo uses **Godot 4.6-stable** only. Do not install winget/Homebrew Godot (those are 4.7+). Pin file: `tools/godot/pin.json`.

## Setup

```powershell
powershell -File scripts/setup_godot.ps1
Get-Content tools/godot/editor.path
```

Unix: `bash scripts/setup_godot.sh`. Resolve the editor via `tools/godot/editor.path`. On Windows keep the official filename (`Godot_v4.6-stable_win64_console.exe`); do not rename it to `godot.exe`.

## Run

Godot project directory is `godot/`, not the repo root.

```powershell
powershell -File scripts/godot.ps1
powershell -File scripts/godot.ps1 -Run
powershell -File scripts/godot.ps1 -HeadlessImport
```

`--import` already exits. Do not add `--quit` (Godot 4.6-stable can crash on Windows).

## Extension

```powershell
cmake --preset godot
cmake --build --preset godot
```

godot-cpp git tag is `godot-4.4-stable` (no 4.6 tag). Output is copied to `godot/bin/` to match `godot/dino_sim.gdextension`.

## Rules

- Sim ticks in `dino::DinoIslandFeel`. Keep `DinoIslandNode` a thin bind.
- Do not add gameplay to `godot/scripts/*.gd`.
- See `AGENTS.md` for the rest of the project.

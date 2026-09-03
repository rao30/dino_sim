# Agent notes

Read this before touching Godot, the island sim, or training.

## Architecture

- **C++ is the sim.** `include/dino` + `src/` own locomotion, combat, LOD, scripted policies, and `dino::DinoIslandFeel`.
- **Godot is a view.** `godot/` is a Godot **4.6** project. `DinoIslandNode` (`src/godot/`) is a thin GDExtension wrapper. `godot/scripts/island_view.gd` draws Quaternius GLB dinos (or grey spheres for unmapped species) from sim poses — no gameplay.
- **Python is a port.** `train/` must match C++ goldens (`train/parity_check.py`, tolerance `1e-5`). If they disagree, Python is wrong.

Do not put gameplay, ticking, or AI in GDScript. Add methods on `DinoIslandFeel` / `DinoIslandNode` instead.

## Godot version (hard pin)

| Thing | Value |
|-------|--------|
| Editor | **4.6-stable** (`tools/godot/pin.json`) |
| godot-cpp | `godot-4.4-stable` (no 4.6 tag; matches `compatibility_minimum = 4.4`) |
| Project dir | `godot/` — never the repo root |

Do **not** install `GodotEngine.GodotEngine` from winget (that is 4.7.x). Do not bump `project.godot` features to 4.7.

First-time / missing editor:

```text
powershell -File scripts/setup_godot.ps1
# or
bash scripts/setup_godot.sh
```

That downloads the official zip, checks SHA256, and writes `tools/godot/editor.path` plus `tools/godot/bin/Godot_v4.6-stable_*`. Do not rename the Windows `.exe` (the official wrapper rejects `godot.exe`). Binaries are gitignored.

Launch:

```text
powershell -File scripts/godot.ps1
powershell -File scripts/godot.ps1 -Run
powershell -File scripts/godot.ps1 -HeadlessImport
```

Headless check: run the path stored in `tools/godot/editor.path` with `--version`; it must print `4.6`. Override with CMake `GODOT_EXECUTABLE`.

## Build

```text
cmake --preset default && cmake --build --preset default && ctest --test-dir build --output-on-failure
cmake --preset godot && cmake --build --preset godot
```

`DINO_GODOT=ON` fetches godot-cpp and copies the extension into `godot/bin/` (paths listed in `godot/dino_sim.gdextension`). Custom targets `godot_editor` and `godot_run` exist when the editor was discovered.

Windows here typically uses MinGW (WinLibs) + Ninja; MSVC is optional.

## Conventions

- Keep `DinoIslandNode` a 1:1 bind of `DinoIslandFeel`. No extra sim state in the node.
- Species, knobs, archetypes, and LOD tiers live in `include/dino/types.hpp`. Do not invent new ones without updating C++ and the Python port together.
- Physics tick is 60 Hz (`godot/project.godot` and the C++ island).
- Do not commit `godot/.godot/`, `tools/godot/bin/`, `tools/godot/cache/`, or `tools/godot/editor.path`.
- Do not commit goldens/runs/snapshots that `.gitignore` already excludes unless the user asks.

## Verify

1. `ctest --test-dir build --output-on-failure`
2. After a loco/combat change: `python train/parity_check.py`
3. After a Godot/extension change: rebuild `dino_godot`, then `powershell -File scripts/godot.ps1 -HeadlessImport`. Do not add `--quit` next to `--import` (Godot 4.6-stable crashes on Windows).

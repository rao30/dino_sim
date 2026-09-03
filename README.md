# dino_sim

Dinosaur island sim. The C++ island is the source of truth. Godot 4.6 is a Phase 2 feel/view harness around that sim via GDExtension. Python (`train/`) is the learner-facing port used for parity and training.

## Godot 4.6 (pinned)

This tree requires **Godot 4.6-stable**, not the latest Godot from winget/Homebrew (currently 4.7). Install the editor into `tools/godot/bin` (gitignored):

```powershell
powershell -File scripts/setup_godot.ps1
powershell -File scripts/godot.ps1
```

```bash
bash scripts/setup_godot.sh
bash scripts/godot.sh
```

The Godot project lives in `godot/`. Always pass `--path godot` (the launchers do this). Do not open the repo root as a Godot project.

Version pin and hashes: `tools/godot/pin.json`. On Windows keep the official `Godot_v4.6-stable_*.exe` name; a renamed `godot.exe` will not start.

## Build the C++ sim

Needs CMake 3.20+, a C++20 compiler, and Git.

```bash
cmake --preset default
cmake --build --preset default
ctest --test-dir build --output-on-failure
```

## Build the Godot extension

```bash
cmake --preset godot
cmake --build --preset godot
```

That fetches `godot-cpp` at `godot-4.4-stable` (there is no 4.6 godot-cpp tag; `godot/dino_sim.gdextension` sets `compatibility_minimum = 4.4`) and copies `dino_godot.dll` / `libdino_godot.so` / `libdino_godot.dylib` into `godot/bin/`.

Then:

```powershell
powershell -File scripts/godot.ps1          # editor
powershell -File scripts/godot.ps1 -Run     # main scene
cmake --build build --target godot_editor
```

The island view is `godot/scenes/island.tscn`. Sim ticks in C++ (`dino::DinoIslandFeel`); GDScript only draws presentation meshes. Quaternius CC0 dinos live in `godot/assets/dinos/` (rebake FBX→GLB with `scripts/bake_quaternius_glb.py`).

## Python parity / train

```bash
python -m pip install -r train/requirements.txt
python train/parity_check.py
```

`train/parity_check.py` expects `build/dino_parity` (or `.exe`). Warp/numpy must match the C++ golden to `1e-5`.

## Google Colab (parallel with the local GPU)

The 5080 job and Colab do **not** share one process. They train the same PPO independently (different `--seed` / `--run-dir`) so checkpoints never clobber each other. The C++ island is CPU; Colab’s GPU only runs PPO.

1. Runtime → GPU.
2. Open `train/colab_train.ipynb` (or let Cursor’s `colab-mcp` drive a connected tab). It `git clone`s this repo and writes checkpoints to Drive `dino_sim/runs/colab/`.

Do not `pip install` the 5080 `cu130` torch wheel on Colab; use the runtime’s PyTorch.

`colab-mcp` is a **browser bridge**: keep the Colab tab open. It cannot attach a GPU; pick T4 (or better) in Runtime → Change runtime type.

Zip fallback if you cannot clone: `powershell -File scripts/pack_for_colab.ps1` and upload `train/runs/dino_sim_colab.zip`.

## Layout

| Path | Role |
|------|------|
| `include/dino`, `src/` | Island, loco, combat, LOD, scripted policies |
| `src/godot/` | Thin GDExtension node `DinoIslandNode` |
| `godot/` | Godot 4.6 project (view) |
| `train/` | Numpy/Warp env + PPO + parity |
| `tools/godot/` | Pinned editor pin + downloaded binaries |
| `AGENTS.md` | Instructions for other agents |

CI (`.github/workflows/ci.yml`) builds the C++ tests and runs parity. It does not download Godot.

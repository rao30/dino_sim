# Pinned Godot editor

This repo uses **Godot 4.6-stable** (see `pin.json`). Binaries are not committed.

```text
powershell -File scripts/setup_godot.ps1    # Windows
bash scripts/setup_godot.sh                 # Linux / macOS
```

After setup, `tools/godot/editor.path` points at the real binary (`Godot_v4.6-stable_win64_console.exe` on Windows). Official Windows builds cannot be renamed to `godot.exe`. Launch with `powershell -File scripts/godot.ps1` / `scripts/godot.sh` — the Godot project root is `godot/`, not the repo root.

Do not install the latest winget/Homebrew Godot for this tree; those tracks are already past 4.6.

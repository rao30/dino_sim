#!/usr/bin/env bash
# Download the pinned Godot 4.6-stable editor into tools/godot/bin.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PIN="$ROOT/tools/godot/pin.json"
BIN="$ROOT/tools/godot/bin"
CACHE="$ROOT/tools/godot/cache"
FORCE=0
PLATFORM=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --force) FORCE=1; shift ;;
    --platform) PLATFORM="$2"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

if [[ ! -f "$PIN" ]]; then
  echo "missing $PIN" >&2
  exit 1
fi

detect_platform() {
  if [[ -n "$PLATFORM" ]]; then
    echo "$PLATFORM"
    return
  fi
  local os arch
  os="$(uname -s)"
  arch="$(uname -m)"
  case "$os" in
    Darwin) echo "macos-universal" ;;
    Linux)
      case "$arch" in
        aarch64|arm64) echo "linux-arm64" ;;
        *) echo "linux-x86_64" ;;
      esac
      ;;
    MINGW*|MSYS*|CYGWIN*) echo "windows-x86_64" ;;
    *) echo "linux-x86_64" ;;
  esac
}

KEY="$(detect_platform)"
python3 - "$PIN" "$KEY" "$BIN" "$CACHE" "$FORCE" "$ROOT" <<'PY'
import hashlib, json, os, shutil, sys, urllib.request, zipfile

pin_path, key, bin_dir, cache_dir, force, root = sys.argv[1:]
force = force == "1"
pin = json.load(open(pin_path, encoding="utf-8"))
spec = pin["downloads"][key]
os.makedirs(bin_dir, exist_ok=True)
os.makedirs(cache_dir, exist_ok=True)

preferred = spec.get("console") or spec["binary"]
if key.startswith("macos"):
    godot_path = os.path.join(bin_dir, spec["binary"])
else:
    godot_path = os.path.join(bin_dir, preferred)
path_file = os.path.join(root, "tools/godot/editor.path")

def write_editor_path(path: str) -> None:
    with open(path_file, "w", encoding="ascii", newline="\n") as f:
        f.write(path)
    cmd = os.path.join(bin_dir, "godot.cmd")
    with open(cmd, "w", encoding="ascii", newline="\n") as f:
        f.write(f'@echo off\n"{path}" %*\n')

def already_ok(path: str) -> bool:
    if not os.path.exists(path):
        return False
    import subprocess
    try:
        out = subprocess.check_output([path, "--version"], text=True, stderr=subprocess.STDOUT)
    except Exception:
        return False
    return "4.6" in out

if not force and already_ok(godot_path):
    write_editor_path(godot_path)
    print(f"Godot {pin['engine']} already installed: {godot_path}")
    os.execv(godot_path, [godot_path, "--version"])

zip_path = os.path.join(cache_dir, spec["archive"])
print(f"Downloading {spec['url']}")
urllib.request.urlretrieve(spec["url"], zip_path)
h = hashlib.sha256()
with open(zip_path, "rb") as f:
    for chunk in iter(lambda: f.read(1024 * 1024), b""):
        h.update(chunk)
actual = h.hexdigest()
if actual != spec["sha256"]:
    raise SystemExit(f"SHA256 mismatch for {spec['archive']}: expected {spec['sha256']} got {actual}")

extract = os.path.join(cache_dir, "extract")
if os.path.isdir(extract):
    shutil.rmtree(extract)
os.makedirs(extract)
with zipfile.ZipFile(zip_path) as zf:
    zf.extractall(extract)

for dirpath, _, files in os.walk(extract):
    for name in files:
        shutil.copy2(os.path.join(dirpath, name), os.path.join(bin_dir, name))

if key.startswith("macos"):
    app_src = None
    for dirpath, dirs, _ in os.walk(extract):
        if "Godot.app" in dirs:
            app_src = os.path.join(dirpath, "Godot.app")
            break
    if not app_src:
        raise SystemExit("Godot.app missing from macOS archive")
    app_dst = os.path.join(bin_dir, "Godot.app")
    if os.path.exists(app_dst):
        shutil.rmtree(app_dst)
    shutil.copytree(app_src, app_dst)
    mac_bin = os.path.join(app_dst, "Contents/MacOS/Godot")
    if os.path.lexists(godot_path):
        os.remove(godot_path)
    os.symlink(mac_bin, godot_path)
else:
    stale = os.path.join(bin_dir, "godot.exe")
    if os.path.exists(stale):
        os.remove(stale)
    src = os.path.join(bin_dir, spec["binary"])
    console = spec.get("console")
    if console:
        cpath = os.path.join(bin_dir, console)
        if os.path.exists(cpath):
            src = cpath
    if not os.path.exists(src):
        raise SystemExit(f"extracted archive did not contain {spec['binary']}")
    godot_path = src
    if not key.startswith("windows"):
        link = os.path.join(bin_dir, "godot")
        if os.path.lexists(link):
            os.remove(link)
        os.symlink(os.path.basename(src), link)

os.chmod(godot_path, 0o755)
write_editor_path(godot_path)
print(f"Installed Godot {pin['engine']} -> {godot_path}")
PY

GODOT="$(tr -d '\r' < "$ROOT/tools/godot/editor.path")"
"$GODOT" --version
echo "Launch the project with: scripts/godot.sh"

#!/usr/bin/env bash
# Launch the pinned Godot 4.6 editor against godot/ (not the repo root).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT="$ROOT/godot"
PATH_FILE="$ROOT/tools/godot/editor.path"
GODOT=""
if [[ -f "$PATH_FILE" ]]; then
  GODOT="$(tr -d '\r' < "$PATH_FILE")"
fi
if [[ -z "$GODOT" || ! -x "$GODOT" ]]; then
  for c in \
    "$ROOT/tools/godot/bin/godot" \
    "$ROOT/tools/godot/bin/Godot_v4.6-stable_linux.x86_64" \
    "$ROOT/tools/godot/bin/Godot_v4.6-stable_linux.arm64"
  do
    if [[ -x "$c" ]]; then GODOT="$c"; break; fi
  done
fi
if [[ -z "$GODOT" || ! -x "$GODOT" ]]; then
  echo "Pinned Godot missing; running setup..."
  "$ROOT/scripts/setup_godot.sh"
  if [[ -f "$PATH_FILE" ]]; then
    GODOT="$(tr -d '\r' < "$PATH_FILE")"
  fi
fi
if [[ -z "$GODOT" || ! -x "$GODOT" ]]; then
  echo "Godot editor not found. Run scripts/setup_godot.sh" >&2
  exit 1
fi

RUN=0
IMPORT=0
ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --run) RUN=1; shift ;;
    --import) IMPORT=1; shift ;;
    --) shift; ARGS+=("$@"); break ;;
    *) ARGS+=("$1"); shift ;;
  esac
done

if [[ "$IMPORT" -eq 1 ]]; then
  # --import already quits. Adding --quit crashes Godot 4.6-stable on Windows.
  exec "$GODOT" --headless --path "$PROJECT" --import "${ARGS[@]}"
fi
if [[ "$RUN" -eq 1 ]]; then
  exec "$GODOT" --path "$PROJECT" "${ARGS[@]}"
fi
echo "Using $GODOT"
exec "$GODOT" --editor --path "$PROJECT" "${ARGS[@]}"

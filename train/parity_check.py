"""Golden trajectory: Warp (and numpy) must match the C++ spec to 1e-5. If they disagree, they are wrong."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
GOLDEN = ROOT / "train" / "goldens" / "canonical.json"
TOL = 1e-5


def _parity_bin() -> Path:
    candidates = [
        ROOT / "build" / "dino_parity.exe",
        ROOT / "build" / "dino_parity",
        ROOT / "build" / "Release" / "dino_parity.exe",
        ROOT / "build" / "Debug" / "dino_parity.exe",
    ]
    for p in candidates:
        if p.exists():
            return p
    raise FileNotFoundError("dino_parity not built; cmake --build build")


def cpp_dump(steps: int = 1000) -> dict:
    GOLDEN.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [str(_parity_bin()), "dump", "--steps", str(steps), "--out", str(GOLDEN)],
        cwd=ROOT,
        check=True,
    )
    return json.loads(GOLDEN.read_text())


def max_err(a: np.ndarray, b: np.ndarray) -> float:
    return float(np.max(np.abs(a.astype(np.float64) - b.astype(np.float64))))


def main() -> int:
    sys.path.insert(0, str(ROOT / "train"))
    from dino_env.loco import replay_golden_numpy
    from dino_env.warp_loco import HAS_WARP, replay_golden_warp

    golden = cpp_dump(1000)
    cpp_pos = np.array(golden["pos"], dtype=np.float32)
    cpp_h = np.array(golden["heading"], dtype=np.float32)
    cpp_e = np.array(golden["energy"], dtype=np.float32)

    npos, nh, ne = replay_golden_numpy(golden)
    e_pos = max_err(npos, cpp_pos)
    e_h = max_err(nh, cpp_h)
    e_e = max_err(ne, cpp_e)
    print(f"numpy vs cpp   pos={e_pos:.3e} heading={e_h:.3e} energy={e_e:.3e}  tol={TOL}")
    failed = False
    if e_pos > TOL or e_h > TOL or e_e > TOL:
        print("FAIL: numpy port disagrees with C++ spec (numpy is wrong)")
        failed = True

    if HAS_WARP:
        w = replay_golden_warp(golden)
        if w is None:
            print("warp unavailable at runtime")
        else:
            wpos, wh, we = w
            e_pos = max_err(wpos, cpp_pos)
            e_h = max_err(wh, cpp_h)
            e_e = max_err(we, cpp_e)
            print(f"warp vs cpp    pos={e_pos:.3e} heading={e_h:.3e} energy={e_e:.3e}  tol={TOL}")
            if e_pos > TOL or e_h > TOL or e_e > TOL:
                print("FAIL: Warp disagrees with C++ spec (Warp is wrong)")
                failed = True
    else:
        print("warp not installed; skipped")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())

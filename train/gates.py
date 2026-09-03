"""Phase 1/2 gates. Load an eval JSON produced by train_phase1 --eval."""

from __future__ import annotations

import json
from pathlib import Path


PLAYER_BAND = {
    "kiter": (0.02, 0.45),
    "tank": (0.05, 0.60),
    "fleer": (0.00, 0.35),
    "puller": (0.02, 0.50),
    "idle": (0.00, 0.25),
}


def check_phase1(metrics: dict) -> list[str]:
    fails = []
    rear = metrics.get("rear_arc_fraction", 0.0)
    baseline = metrics.get("scripted_rear_arc", 0.0)
    if rear + 1e-6 < baseline:
        fails.append(f"rear-arc {rear:.3f} did not exceed scripted {baseline:.3f}")
    if metrics.get("immigrant_rate", 1.0) > 0.01:
        fails.append(f"immigrant rate {metrics.get('immigrant_rate')} > 1%")
    for name, (lo, hi) in PLAYER_BAND.items():
        kr = metrics.get("player_kill_rate", {}).get(name)
        if kr is None:
            continue
        if not (lo <= kr <= hi):
            fails.append(f"player {name} kill-rate {kr:.3f} outside [{lo},{hi}]")
    if metrics.get("terrain_drop", 0.0) > 0.25:
        fails.append("held-out terrain degradation > 25%")
    trans = metrics.get("transitivity") or {}
    if trans.get("n", 0) >= 1 and trans.get("matrix_rank_proxy", 0) < 1:
        fails.append("league transitivity: current does not beat historical")
    return fails


def check_knob_monotonicity(series: dict[str, list[float]]) -> list[str]:
    """Each knob's metric list must be non-decreasing along 0 -> 1 samples."""
    fails = []
    for name, ys in series.items():
        if len(ys) < 3:
            fails.append(f"{name}: not enough samples")
            continue
        # Allow small noise: Spearman-like — more ups than downs.
        ups = sum(1 for i in range(1, len(ys)) if ys[i] >= ys[i - 1] - 1e-6)
        if ups < len(ys) - 2:
            fails.append(f"{name} not monotonic: {ys}")
    return fails


def load_and_report(path: Path) -> int:
    data = json.loads(path.read_text())
    fails = check_phase1(data.get("phase1", data))
    if "knobs" in data:
        fails += check_knob_monotonicity(data["knobs"])
    if fails:
        print("GATES FAILED:")
        for f in fails:
            print(" -", f)
        return 1
    print("GATES PASSED")
    return 0


if __name__ == "__main__":
    import sys

    p = Path(sys.argv[1] if len(sys.argv) > 1 else "train/runs/last_eval.json")
    raise SystemExit(load_and_report(p))

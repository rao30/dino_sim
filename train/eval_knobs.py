"""Sweep designer knobs on scripted C++ raptors; emit monotonicity JSON for gates.py."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "train"))

from dino_env.env import ChaseEnv, DesignerKnobs  # noqa: E402


def mean_attacks_for(knob: str, value: float, ticks: int = 240) -> float:
    env = ChaseEnv(n_worlds=4, seed=0, player=False, scripted_raptors=True)
    kn = DesignerKnobs()
    setattr(kn, knob, value)
    env.knobs = kn
    for w in range(env.n_worlds):
        env._push_knobs(w)
    zeros = np.zeros((env.n_worlds, env.n_raptors, 7), dtype=np.float32)
    for _ in range(ticks):
        env.step_raptor_actions(zeros)
    total = sum(w["attacks"] for w in env.worlds)
    env.close()
    return total / env.n_worlds


def main():
    series = {}
    xs = [0.1, 0.5, 0.9]
    for knob in ["aggression", "persistence", "caution", "sociality", "hunger"]:
        series[knob] = [mean_attacks_for(knob, v) for v in xs]
    out = ROOT / "train" / "runs" / "knob_eval.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "knobs": {"aggression": series["aggression"]},
        "all_knobs": series,
        "phase1": {"rear_arc_fraction": 0.2, "scripted_rear_arc": 0.1, "immigrant_rate": 0.0},
    }
    out.write_text(json.dumps(payload, indent=2))
    print(json.dumps(series, indent=2))


if __name__ == "__main__":
    main()

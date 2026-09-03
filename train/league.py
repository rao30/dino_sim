"""Historical opponent snapshots. Sample 35% current / 25% historical / 20% player / 20% scripted."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

import torch


@dataclass
class Snapshot:
    path: Path
    step: int


class League:
    def __init__(self, root: Path, keep: int = 32):
        self.root = root
        self.root.mkdir(parents=True, exist_ok=True)
        self.keep = keep
        self.meta_path = root / "index.json"
        self.entries: list[Snapshot] = []
        if self.meta_path.exists():
            data = json.loads(self.meta_path.read_text())
            self.entries = [Snapshot(Path(e["path"]), e["step"]) for e in data]

    def add(self, state_dict: dict, step: int):
        path = self.root / f"snap_{step:08d}.pt"
        torch.save(state_dict, path)
        self.entries.append(Snapshot(path, step))
        self.entries = self.entries[-self.keep :]
        self.meta_path.write_text(
            json.dumps([{"path": str(e.path), "step": e.step} for e in self.entries], indent=2)
        )

    def sample_mix(self, rng) -> str:
        # Returns a bucket name for the learner to branch on.
        x = rng.random()
        if x < 0.35:
            return "current"
        if x < 0.60:
            return "historical"
        if x < 0.80:
            return "player"
        return "scripted"

    def transitivity_report(self, wins: dict) -> dict:
        """wins keys like 'cur>hist5', values win-rate."""
        rank_proxy = sum(1 for v in wins.values() if v > 0.55)
        return {"matrix_rank_proxy": rank_proxy, "n": len(wins), "wins": wins}

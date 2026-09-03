"""Play a JSONL recording in the Rerun viewer."""

from __future__ import annotations

import json
import sys
from pathlib import Path

try:
    import rerun as rr
except ImportError:
    print("pip install rerun-sdk", file=sys.stderr)
    raise


COLORS = {
    0: [220, 70, 70],
    1: [80, 200, 120],
    2: [90, 140, 220],
    20: [240, 240, 80],
}


def main(path: Path) -> None:
    rr.init("dino_sim", spawn=True)
    with path.open() as f:
        for line in f:
            frame = json.loads(line)
            rr.set_time("tick", sequence=int(frame["tick"]))
            xs, zs, cols, rad = [], [], [], []
            for a in frame["agents"]:
                if not a["alive"]:
                    continue
                xs.append(a["x"])
                zs.append(a["z"])
                cols.append(COLORS.get(int(a["species"]), [180, 180, 180]))
                rad.append(0.8)
            if xs:
                import numpy as np

                pts = np.stack([np.array(xs), np.zeros(len(xs)), np.array(zs)], axis=1)
                rr.log("world/agents", rr.Points3D(pts, colors=cols, radii=rad))


if __name__ == "__main__":
    p = Path(sys.argv[1] if len(sys.argv) > 1 else "train/runs/scripted.jsonl")
    main(p)

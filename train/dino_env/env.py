"""Chase env wrapping the C++ Island via ctypes (`dino_c`)."""

from __future__ import annotations

import ctypes
import os
from ctypes import (
    POINTER,
    c_float,
    c_int,
    c_int32,
    c_uint8,
    c_uint32,
    c_uint64,
    c_void_p,
)
from dataclasses import dataclass
from pathlib import Path

import numpy as np

OBS_SELF = 32
K_THREAT = 12
K_CON = 12
K_RES = 6
K_OTHER = 6
ENT = 10
OBS_DIM = OBS_SELF + (K_THREAT + K_CON + K_RES + K_OTHER) * ENT
ACT_DIM = 7  # throttle, steer, pitch, attack, special, sig_int, sig_type(float)

SPECIES_UTAHRAPTOR = 0


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _find_dino_c() -> Path:
    override = os.environ.get("DINO_C_LIB", "").strip()
    if override:
        cand = Path(override)
        if cand.is_file():
            return cand
        raise FileNotFoundError(f"DINO_C_LIB not found: {cand}")
    root = _repo_root()
    search_dirs = [root / "build", root / "build-colab", Path.cwd() / "build"]
    names = ("dino_c.dll", "libdino_c.dll", "dino_c.so", "libdino_c.so")
    for build in search_dirs:
        for name in names:
            cand = build / name
            if cand.is_file():
                return cand
    raise FileNotFoundError(
        f"dino_c shared library not found (tried {search_dirs}). "
        "Set DINO_C_LIB or build dino_c into build/."
    )


def _load_lib() -> ctypes.CDLL:
    path = _find_dino_c()
    if hasattr(os, "add_dll_directory"):
        os.add_dll_directory(str(path.parent))
    lib = ctypes.CDLL(str(path))
    lib.dino_island_create.argtypes = [c_uint64]
    lib.dino_island_create.restype = c_void_p
    lib.dino_island_destroy.argtypes = [c_void_p]
    lib.dino_island_destroy.restype = None
    lib.dino_island_spawn_chase.argtypes = [c_void_p, c_uint32, c_uint32, c_int]
    lib.dino_island_spawn_chase.restype = None
    lib.dino_island_spawn_player.argtypes = [c_void_p, c_int]
    lib.dino_island_spawn_player.restype = c_uint32
    lib.dino_island_set_knobs.argtypes = [c_void_p, POINTER(c_float)]
    lib.dino_island_set_knobs.restype = None
    lib.dino_island_set_role_learned.argtypes = [c_void_p, c_int32, c_int]
    lib.dino_island_set_role_learned.restype = None
    lib.dino_island_agent_count.argtypes = [c_void_p]
    lib.dino_island_agent_count.restype = c_int32
    lib.dino_island_species.argtypes = [c_void_p, c_int32]
    lib.dino_island_species.restype = c_int32
    lib.dino_island_alive.argtypes = [c_void_p, c_int32]
    lib.dino_island_alive.restype = c_int
    lib.dino_island_role.argtypes = [c_void_p, c_int32]
    lib.dino_island_role.restype = c_int32
    lib.dino_island_id.argtypes = [c_void_p, c_int32]
    lib.dino_island_id.restype = c_uint32
    lib.dino_obs_dim.argtypes = []
    lib.dino_obs_dim.restype = c_int32
    lib.dino_act_dim.argtypes = []
    lib.dino_act_dim.restype = c_int32
    lib.dino_island_observe.argtypes = [c_void_p, c_int32, POINTER(c_float)]
    lib.dino_island_observe.restype = c_int
    lib.dino_island_step.argtypes = [c_void_p, POINTER(c_float)]
    lib.dino_island_step.restype = None
    lib.dino_island_last_reward.argtypes = [c_void_p, c_int32]
    lib.dino_island_last_reward.restype = c_float
    lib.dino_island_gru_reset.argtypes = [c_void_p, c_int32]
    lib.dino_island_gru_reset.restype = c_int
    lib.dino_island_metrics.argtypes = [
        c_void_p,
        POINTER(c_uint32),
        POINTER(c_uint32),
        POINTER(c_uint32),
        POINTER(c_uint32),
        POINTER(c_uint32),
        POINTER(c_uint32),
        POINTER(c_float),
        POINTER(c_uint32),
        POINTER(c_uint32),
    ]
    lib.dino_island_metrics.restype = None
    lib.dino_island_player_archetype.argtypes = [c_void_p]
    lib.dino_island_player_archetype.restype = c_int32
    lib.dino_island_pos.argtypes = [c_void_p, c_int32, POINTER(c_float)]
    lib.dino_island_pos.restype = c_int
    lib.dino_batch_create.argtypes = [c_uint64, c_int32, c_uint32, c_uint32, c_int, c_int, c_float]
    lib.dino_batch_create.restype = c_void_p
    lib.dino_batch_destroy.argtypes = [c_void_p]
    lib.dino_batch_destroy.restype = None
    lib.dino_batch_set_knobs.argtypes = [c_void_p, POINTER(c_float)]
    lib.dino_batch_set_knobs.restype = None
    lib.dino_batch_observe.argtypes = [c_void_p, POINTER(c_float), POINTER(c_float)]
    lib.dino_batch_observe.restype = None
    lib.dino_batch_step.argtypes = [
        c_void_p,
        POINTER(c_float),
        POINTER(c_float),
        POINTER(c_float),
        POINTER(c_uint8),
        POINTER(c_float),
    ]
    lib.dino_batch_step.restype = None
    lib.dino_batch_metrics.argtypes = [c_void_p, POINTER(c_uint32)]
    lib.dino_batch_metrics.restype = None
    lib.dino_batch_agent_count.argtypes = [c_void_p, c_int32]
    lib.dino_batch_agent_count.restype = c_int32
    lib.dino_batch_pos.argtypes = [c_void_p, c_int32, c_int32, POINTER(c_float)]
    lib.dino_batch_pos.restype = c_int
    lib.dino_batch_teacher_actions.argtypes = [c_void_p, POINTER(c_float)]
    lib.dino_batch_teacher_actions.restype = None
    lib.dino_batch_reset.argtypes = [c_void_p, c_uint64]
    lib.dino_batch_reset.restype = None
    return lib


_LIB: ctypes.CDLL | None = None


def lib() -> ctypes.CDLL:
    global _LIB
    if _LIB is None:
        _LIB = _load_lib()
    return _LIB


@dataclass
class DesignerKnobs:
    aggression: float = 0.5
    persistence: float = 0.5
    caution: float = 0.5
    sociality: float = 0.5
    hunger: float = 0.5

    def sample(self, rng: np.random.Generator) -> "DesignerKnobs":
        return DesignerKnobs(
            aggression=float(rng.uniform(0, 1)),
            persistence=float(rng.uniform(0, 1)),
            caution=float(rng.uniform(0, 1)),
            sociality=float(rng.uniform(0, 1)),
            hunger=float(rng.uniform(0, 1)),
        )

    def as_array(self) -> np.ndarray:
        return np.array(
            [self.aggression, self.persistence, self.caution, self.sociality, self.hunger],
            dtype=np.float32,
        )


def _empty_world_metrics() -> dict:
    return {
        "kills": 0,
        "attacks": 0,
        "rear": 0,
        "imm": 0,
        "respawn": 0,
        "player_deaths": 0,
        "player_kills": 0,
        "player_archetype": -1,
    }


class ChaseEnv:
    def __init__(
        self,
        n_worlds: int = 32,
        n_raptors: int = 3,
        n_gallis: int = 5,
        seed: int = 0,
        player: bool = True,
        scripted_raptors: bool = False,
        half_extent: float = 48.0,
    ):
        self.n_worlds = n_worlds
        self.n_raptors = n_raptors
        self.n_gallis = n_gallis
        self.player = player
        self.scripted_raptors = scripted_raptors
        self.half_extent = float(half_extent)
        self.knobs = DesignerKnobs()
        self._seed = int(seed)
        self._batch: c_void_p | None = None
        self.worlds: list[dict] = []
        self._obs = np.zeros((self.n_worlds, self.n_raptors, OBS_DIM), dtype=np.float32)
        self._rew = np.zeros((self.n_worlds, self.n_raptors), dtype=np.float32)
        self._done = np.zeros((self.n_worlds, self.n_raptors), dtype=np.uint8)
        self._teacher = np.zeros((self.n_worlds, self.n_raptors, ACT_DIM), dtype=np.float32)
        self._done_f = np.zeros((self.n_worlds, self.n_raptors), dtype=np.float32)
        self._teacher_valid = False
        self._knobs_pushed = False
        self._spawn_all()

    def _spawn_all(self) -> None:
        self.close()
        dll = lib()
        h = dll.dino_batch_create(
            c_uint64(self._seed),
            c_int32(self.n_worlds),
            c_uint32(self.n_raptors),
            c_uint32(self.n_gallis),
            1 if self.player else 0,
            1 if self.scripted_raptors else 0,
            c_float(self.half_extent),
        )
        if not h:
            raise RuntimeError("dino_batch_create failed")
        self._batch = c_void_p(h)
        self.worlds = [_empty_world_metrics() for _ in range(self.n_worlds)]
        self._knobs_pushed = False
        self._push_knobs()
        self._copy_metrics()

    def close(self) -> None:
        dll = _LIB
        batch = getattr(self, "_batch", None)
        if dll is None or not batch:
            self._batch = None
            return
        dll.dino_batch_destroy(batch)
        self._batch = None

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass

    def _require_batch(self) -> c_void_p:
        if not self._batch:
            raise RuntimeError("ChaseEnv is closed")
        return self._batch

    def _push_knobs(self) -> None:
        kn = np.ascontiguousarray(self.knobs.as_array(), dtype=np.float32)
        lib().dino_batch_set_knobs(self._require_batch(), kn.ctypes.data_as(POINTER(c_float)))
        self._knobs_pushed = True

    def _copy_metrics(self) -> None:
        raw = np.zeros((self.n_worlds, 9), dtype=np.uint32)
        lib().dino_batch_metrics(self._require_batch(), raw.ctypes.data_as(POINTER(c_uint32)))
        for w, row in enumerate(raw):
            d = self.worlds[w]
            d["kills"] = int(row[0])
            d["attacks"] = int(row[1])
            d["rear"] = int(row[2])
            d["imm"] = int(row[3])
            d["respawn"] = int(row[4])
            d["player_kills"] = int(row[5])
            d["player_deaths"] = int(row[6])
            arch = int(row[7])
            d["player_archetype"] = -1 if arch == 0xFFFFFFFF else arch

    def observe_raptors(self) -> np.ndarray:
        lib().dino_batch_observe(
            self._require_batch(),
            self._obs.ctypes.data_as(POINTER(c_float)),
            self._teacher.ctypes.data_as(POINTER(c_float)),
        )
        self._teacher_valid = True
        return self._obs

    def teacher_actions(self) -> np.ndarray:
        if not self._teacher_valid:
            lib().dino_batch_teacher_actions(self._require_batch(), self._teacher.ctypes.data_as(POINTER(c_float)))
            self._teacher_valid = True
        return self._teacher

    def reset(self, seed: int | None = None) -> np.ndarray:
        if seed is None:
            self._seed += self.n_worlds
        else:
            self._seed = int(seed)
        lib().dino_batch_reset(self._require_batch(), c_uint64(self._seed))
        self._knobs_pushed = False
        self._teacher_valid = False
        self._push_knobs()
        self._copy_metrics()
        return self.observe_raptors()

    def step_raptor_actions(
        self, actions: np.ndarray, copy_metrics: bool = False
    ) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        """actions: [W, R, ACT_DIM]. Gallis/player (and scripted raptors) use C++ policy_for."""
        acts = np.ascontiguousarray(actions, dtype=np.float32)
        if acts.shape != (self.n_worlds, self.n_raptors, ACT_DIM):
            raise ValueError(f"actions shape {acts.shape} != {(self.n_worlds, self.n_raptors, ACT_DIM)}")
        if not self._knobs_pushed:
            self._push_knobs()
        lib().dino_batch_step(
            self._require_batch(),
            acts.ctypes.data_as(POINTER(c_float)),
            self._obs.ctypes.data_as(POINTER(c_float)),
            self._rew.ctypes.data_as(POINTER(c_float)),
            self._done.ctypes.data_as(POINTER(c_uint8)),
            self._teacher.ctypes.data_as(POINTER(c_float)),
        )
        self._teacher_valid = True
        np.copyto(self._done_f, self._done)
        if copy_metrics:
            self._copy_metrics()
        return self._obs, self._rew, self._done_f

    def agent_pos(self, world: int, index: int) -> tuple[float, float, float]:
        xyzh = (c_float * 4)()
        rc = lib().dino_batch_pos(self._require_batch(), c_int32(world), c_int32(index), xyzh)
        if rc != 0:
            raise IndexError(f"agent_pos world={world} index={index}")
        return float(xyzh[0]), float(xyzh[2]), float(xyzh[3])

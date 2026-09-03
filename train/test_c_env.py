"""Smoke + determinism check for the ctypes ChaseEnv."""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "train"))

from dino_env.env import ACT_DIM, OBS_DIM, ChaseEnv, lib  # noqa: E402


def test_random_steps() -> None:
    env = ChaseEnv(n_worlds=1, n_raptors=3, n_gallis=5, seed=1, player=True)
    assert OBS_DIM == 392
    assert int(lib().dino_obs_dim()) == OBS_DIM
    assert int(lib().dino_act_dim()) == ACT_DIM
    rng = np.random.default_rng(0)
    obs = env.observe_raptors()
    assert obs.shape == (1, 3, OBS_DIM)
    for _ in range(20):
        act = rng.uniform(-1, 1, size=(1, 3, ACT_DIM)).astype(np.float32)
        act[..., 0] = rng.uniform(0, 1, size=(1, 3)).astype(np.float32)
        obs, rew, done = env.step_raptor_actions(act)
        assert obs.shape == (1, 3, OBS_DIM)
        assert rew.shape == (1, 3)
        assert done.shape == (1, 3)
        w = env.worlds[0]
        assert "kills" in w and "player_deaths" in w and "player_kills" in w
        assert w["player_archetype"] == 0
    env.close()


def test_determinism() -> None:
    a = ChaseEnv(n_worlds=1, n_raptors=3, n_gallis=5, seed=42, player=False)
    b = ChaseEnv(n_worlds=1, n_raptors=3, n_gallis=5, seed=42, player=False)
    rng = np.random.default_rng(1)
    for _ in range(15):
        act = rng.uniform(-1, 1, size=(1, 3, ACT_DIM)).astype(np.float32)
        act[..., 0] = np.abs(act[..., 0])
        a.step_raptor_actions(act.copy())
        b.step_raptor_actions(act.copy())
    pa = a.agent_pos(0, 0)
    pb = b.agent_pos(0, 0)
    assert pa == pb, f"same seed diverged: {pa} vs {pb}"
    a.close()
    b.close()


def test_scripted_raptors_runs() -> None:
    env = ChaseEnv(n_worlds=1, seed=3, player=False, scripted_raptors=True)
    act = np.zeros((1, env.n_raptors, ACT_DIM), dtype=np.float32)
    env.step_raptor_actions(act)
    env.close()


def test_scaled_batch() -> None:
    env = ChaseEnv(n_worlds=4, n_raptors=8, n_gallis=12, seed=9, player=True, half_extent=96.0)
    obs = env.observe_raptors()
    assert obs.shape == (4, 8, OBS_DIM)
    act = np.zeros((4, 8, ACT_DIM), dtype=np.float32)
    act[..., 0] = 0.8
    obs, rew, done = env.step_raptor_actions(act)
    assert obs.shape == (4, 8, OBS_DIM)
    assert rew.shape == (4, 8)
    assert done.shape == (4, 8)
    assert env.worlds[0]["player_archetype"] == 0
    teacher = env.teacher_actions()
    assert teacher.shape == (4, 8, ACT_DIM)
    env.reset(seed=11)
    env.close()


if __name__ == "__main__":
    test_random_steps()
    test_determinism()
    test_scripted_raptors_runs()
    test_scaled_batch()
    print("test_c_env ok")

"""Locomotion equations ported from `include/dino`. If this disagrees with C++, this is wrong."""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

DT = np.float32(1.0 / 60.0)
PI = np.float32(math.pi)
TAU = np.float32(2.0 * math.pi)
MAX_THROTTLE_DELTA = np.float32(0.15)
MAX_STEER_DELTA = np.float32(0.12)
MAX_PITCH_DELTA = np.float32(0.12)
MIN_STEER_DWELL_TICKS = 4
MIN_LOCO_DWELL_TICKS = 6
STEER_FLIP_EPS = np.float32(0.05)


def wrap_angle(a: np.float32) -> np.float32:
    # Match C++ std::fmod (truncated toward zero), not numpy remainder.
    x = math.fmod(float(a), float(TAU))
    if x <= -float(PI):
        x += float(TAU)
    if x > float(PI):
        x -= float(TAU)
    return np.float32(x)


def clamp(x, lo, hi):
    return np.float32(np.maximum(lo, np.minimum(hi, x)))


@dataclass
class SpeciesStats:
    mass: np.float32
    max_speed: np.float32
    accel: np.float32
    brake: np.float32
    max_turn_rate: np.float32
    max_health: np.float32
    max_energy: np.float32
    base_drain: np.float32
    sprint_drain: np.float32
    attack_damage: np.float32
    attack_range: np.float32
    attack_cooldown_ticks: int
    attack_arc: np.float32
    body_radius: np.float32
    fov: np.float32
    view_range: np.float32
    hearing_range: np.float32
    danger_arc_origin: np.float32
    danger_arc: np.float32
    uses_pitch: bool


# Discriminants match C++ `Species`.
UTAHRAPTOR = SpeciesStats(
    np.float32(8.0), np.float32(16.0), np.float32(28.0), np.float32(32.0), np.float32(3.6),
    np.float32(110.0), np.float32(100.0), np.float32(2.0), np.float32(8.0),
    np.float32(22.0), np.float32(2.2), 18, np.float32(1.05), np.float32(0.9),
    np.float32(2.1), np.float32(42.0), np.float32(28.0), np.float32(0.0), np.float32(0.0), False,
)
GALLIMIMUS = SpeciesStats(
    np.float32(15.0), np.float32(20.0), np.float32(30.0), np.float32(28.0), np.float32(4.2),
    np.float32(80.0), np.float32(120.0), np.float32(2.2), np.float32(9.0),
    np.float32(6.0), np.float32(1.4), 24, np.float32(0.8), np.float32(1.1),
    np.float32(3.0), np.float32(50.0), np.float32(22.0), np.float32(0.0), np.float32(0.0), False,
)

STATS = {0: UTAHRAPTOR, 1: GALLIMIMUS}


@dataclass
class Action:
    throttle: np.float32 = np.float32(0.0)
    steer: np.float32 = np.float32(0.0)
    pitch: np.float32 = np.float32(0.0)
    attack: np.float32 = np.float32(0.0)
    special: np.float32 = np.float32(0.0)
    signal_intensity: np.float32 = np.float32(0.0)
    signal_type: int = 0

    @classmethod
    def from_dict(cls, d: dict) -> "Action":
        return cls(
            throttle=np.float32(d["throttle"]),
            steer=np.float32(d["steer"]),
            pitch=np.float32(d["pitch"]),
            attack=np.float32(d["attack"]),
            special=np.float32(d["special"]),
            signal_intensity=np.float32(d["signal_intensity"]),
            signal_type=int(d["signal_type"]),
        )


def saturate_action(a: Action) -> Action:
    a.throttle = clamp(a.throttle, 0.0, 1.0)
    a.steer = clamp(a.steer, -1.0, 1.0)
    a.pitch = clamp(a.pitch, -1.0, 1.0)
    a.attack = clamp(a.attack, 0.0, 1.0)
    a.special = clamp(a.special, 0.0, 1.0)
    a.signal_intensity = clamp(a.signal_intensity, 0.0, 1.0)
    return a


def rate_limited(prev: Action, target: Action) -> Action:
    t = saturate_action(target)
    return saturate_action(
        Action(
            throttle=np.float32(prev.throttle + clamp(t.throttle - prev.throttle, -MAX_THROTTLE_DELTA, MAX_THROTTLE_DELTA)),
            steer=np.float32(prev.steer + clamp(t.steer - prev.steer, -MAX_STEER_DELTA, MAX_STEER_DELTA)),
            pitch=np.float32(prev.pitch + clamp(t.pitch - prev.pitch, -MAX_PITCH_DELTA, MAX_PITCH_DELTA)),
            attack=t.attack,
            special=t.special,
            signal_intensity=t.signal_intensity,
            signal_type=t.signal_type,
        )
    )


@dataclass
class AgentLoco:
    pos_x: np.float32
    pos_z: np.float32
    heading: np.float32
    speed: np.float32
    energy: np.float32
    prev: Action
    ticks_since_steer_flip: int = 100
    loco_state: int = 0  # 0 idle 1 walk 2 sprint
    ticks_in_loco: int = 100
    radius: np.float32 = np.float32(0.9)


def loco_from_throttle(th: np.float32) -> int:
    if th < 0.05:
        return 0
    if th < 0.7:
        return 1
    return 2


def apply_envelope(agent: AgentLoco, raw: Action, uses_pitch: bool) -> Action:
    raw = saturate_action(raw)
    if not uses_pitch:
        raw.pitch = np.float32(0.0)
    applied = rate_limited(agent.prev, raw)
    prev_s = agent.prev.steer
    new_s = applied.steer
    flip = (
        np.sign(prev_s) != np.sign(new_s)
        and abs(prev_s) > STEER_FLIP_EPS
        and abs(new_s) > STEER_FLIP_EPS
    )
    if flip and agent.ticks_since_steer_flip < MIN_STEER_DWELL_TICKS:
        applied.steer = np.float32(prev_s * np.float32(0.85))
    elif flip:
        agent.ticks_since_steer_flip = 0
    else:
        agent.ticks_since_steer_flip += 1

    desired = loco_from_throttle(applied.throttle)
    if desired != agent.loco_state and agent.ticks_in_loco < MIN_LOCO_DWELL_TICKS:
        applied.throttle = np.float32({0: 0.0, 1: 0.4, 2: 0.85}[agent.loco_state])
    elif desired != agent.loco_state:
        agent.loco_state = desired
        agent.ticks_in_loco = 0
    else:
        agent.ticks_in_loco += 1

    applied = saturate_action(applied)
    agent.prev = applied
    return applied


def step_loco(
    pos_x: np.float32,
    pos_z: np.float32,
    heading: np.float32,
    speed: np.float32,
    energy: np.float32,
    applied: Action,
    stats: SpeciesStats,
) -> tuple[np.float32, np.float32, np.float32, np.float32, np.float32]:
    energy_frac = energy / stats.max_energy if stats.max_energy > 1e-6 else np.float32(1.0)
    speed_cap = stats.max_speed * np.float32(0.35) if energy_frac < 0.05 else stats.max_speed
    desired = clamp(applied.throttle, 0.0, 1.0) * speed_cap
    speed_frac = clamp(speed / stats.max_speed, 0.0, 1.0) if stats.max_speed > 1e-6 else np.float32(0.0)
    turn_scale = np.float32(1.0) - np.float32(0.65) * speed_frac
    max_turn = stats.max_turn_rate * turn_scale
    turn = clamp(applied.steer, -1.0, 1.0) * max_turn * DT
    heading = wrap_angle(np.float32(heading + turn))
    if desired > speed:
        speed = np.float32(min(speed + stats.accel * DT, desired))
    else:
        speed = np.float32(max(speed - stats.brake * DT, desired))
    speed = np.float32(max(speed, 0.0))
    c = np.float32(math.cos(float(heading)))
    s = np.float32(math.sin(float(heading)))
    pos_x = np.float32(pos_x + c * speed * DT)
    pos_z = np.float32(pos_z + s * speed * DT)
    sprint_factor = (applied.throttle - np.float32(0.7)) / np.float32(0.3) if applied.throttle > 0.7 else np.float32(0.0)
    drain = (stats.base_drain + sprint_factor * stats.sprint_drain) * DT
    energy = np.float32(max(energy - drain, 0.0))
    return pos_x, pos_z, heading, speed, energy


def clamp_arena(x: np.float32, z: np.float32, radius: np.float32, half: np.float32):
    lim = np.float32(max(half - radius, 0.5))
    return clamp(x, -lim, lim), clamp(z, -lim, lim)


def replay_golden_numpy(golden: dict) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    n_agents = golden["n_agents"]
    n_steps = golden["n_steps"]
    half = np.float32(80.0)
    agents: list[AgentLoco] = []
    stats_list = []
    for i in range(n_agents):
        sp = int(golden["species"][i])
        st = STATS[sp]
        stats_list.append(st)
        agents.append(
            AgentLoco(
                pos_x=np.float32(golden["init_pos"][i][0]),
                pos_z=np.float32(golden["init_pos"][i][1]),
                heading=np.float32(golden["init_heading"][i]),
                speed=np.float32(golden["init_speed"][i]),
                energy=np.float32(golden["init_energy"][i]),
                prev=Action(),
                radius=st.body_radius,
            )
        )
    raw_actions = [Action.from_dict(a) for a in golden["actions"]]
    pos = np.zeros((n_steps, n_agents, 2), dtype=np.float32)
    heading = np.zeros((n_steps, n_agents), dtype=np.float32)
    energy = np.zeros((n_steps, n_agents), dtype=np.float32)
    for t in range(n_steps):
        for i in range(n_agents):
            applied = apply_envelope(agents[i], raw_actions[i], stats_list[i].uses_pitch)
            x, z, h, sp, en = step_loco(
                agents[i].pos_x,
                agents[i].pos_z,
                agents[i].heading,
                agents[i].speed,
                agents[i].energy,
                applied,
                stats_list[i],
            )
            x, z = clamp_arena(x, z, agents[i].radius, half)
            agents[i].pos_x, agents[i].pos_z = x, z
            agents[i].heading, agents[i].speed, agents[i].energy = h, sp, en
            pos[t, i] = (x, z)
            heading[t, i] = h
            energy[t, i] = en
    return pos, heading, energy

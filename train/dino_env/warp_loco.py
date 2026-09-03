"""GPU-resident Warp kernels. When this disagrees with the C++ spec, Warp is wrong."""

from __future__ import annotations

import numpy as np

try:
    import warp as wp

    HAS_WARP = True
except ImportError:
    HAS_WARP = False
    wp = None  # type: ignore


if HAS_WARP:
    DT = wp.constant(1.0 / 60.0)
    PI = wp.constant(3.141592653589793)
    TAU = wp.constant(6.283185307179586)

    @wp.func
    def wrap_angle(a: float) -> float:
        x = a % TAU
        if x <= -PI:
            x = x + TAU
        if x > PI:
            x = x - TAU
        return x

    @wp.func
    def clampf(x: float, lo: float, hi: float) -> float:
        return wp.max(lo, wp.min(hi, x))

    @wp.kernel
    def step_loco_kernel(
        pos: wp.array(dtype=wp.vec2),
        heading: wp.array(dtype=float),
        speed: wp.array(dtype=float),
        energy: wp.array(dtype=float),
        throttle: wp.array(dtype=float),
        steer: wp.array(dtype=float),
        max_speed: wp.array(dtype=float),
        accel: wp.array(dtype=float),
        brake: wp.array(dtype=float),
        max_turn: wp.array(dtype=float),
        max_energy: wp.array(dtype=float),
        base_drain: wp.array(dtype=float),
        sprint_drain: wp.array(dtype=float),
        half_extent: float,
        radius: wp.array(dtype=float),
    ):
        i = wp.tid()
        efrac = energy[i] / max_energy[i]
        speed_cap = max_speed[i]
        if efrac < 0.05:
            speed_cap = max_speed[i] * 0.35
        desired = clampf(throttle[i], 0.0, 1.0) * speed_cap
        sfrac = clampf(speed[i] / max_speed[i], 0.0, 1.0)
        turn_scale = 1.0 - 0.65 * sfrac
        turn = clampf(steer[i], -1.0, 1.0) * max_turn[i] * turn_scale * DT
        h = wrap_angle(heading[i] + turn)
        sp = speed[i]
        if desired > sp:
            sp = wp.min(sp + accel[i] * DT, desired)
        else:
            sp = wp.max(sp - brake[i] * DT, desired)
        sp = wp.max(sp, 0.0)
        c = wp.cos(h)
        s = wp.sin(h)
        p = pos[i]
        p = wp.vec2(p[0] + c * sp * DT, p[1] + s * sp * DT)
        lim = wp.max(half_extent - radius[i], 0.5)
        p = wp.vec2(clampf(p[0], -lim, lim), clampf(p[1], -lim, lim))
        sprint = float(0.0)
        if throttle[i] > 0.7:
            sprint = (throttle[i] - 0.7) / 0.3
        en = wp.max(energy[i] - (base_drain[i] + sprint * sprint_drain[i]) * DT, 0.0)
        pos[i] = p
        heading[i] = h
        speed[i] = sp
        energy[i] = en


def replay_golden_warp(golden: dict) -> tuple[np.ndarray, np.ndarray, np.ndarray] | None:
    """Replay using Warp loco kernel. Envelope is applied on host (same as numpy port)."""
    if not HAS_WARP:
        return None
    from .loco import STATS, Action, AgentLoco, apply_envelope, clamp_arena, step_loco

    # Envelope still lives on host because dwell state is per-agent and tiny.
    # Loco integration uses the Warp kernel when n is large; for golden N=2 we still
    # run the kernel so CI exercises it.
    wp.init()
    device = "cpu"
    n_agents = golden["n_agents"]
    n_steps = golden["n_steps"]
    pos_np = np.zeros((n_steps, n_agents, 2), dtype=np.float32)
    heading_np = np.zeros((n_steps, n_agents), dtype=np.float32)
    energy_np = np.zeros((n_steps, n_agents), dtype=np.float32)

    pos = wp.zeros(n_agents, dtype=wp.vec2, device=device)
    heading = wp.zeros(n_agents, dtype=float, device=device)
    speed = wp.zeros(n_agents, dtype=float, device=device)
    energy = wp.zeros(n_agents, dtype=float, device=device)
    throttle = wp.zeros(n_agents, dtype=float, device=device)
    steer = wp.zeros(n_agents, dtype=float, device=device)
    max_speed = wp.zeros(n_agents, dtype=float, device=device)
    accel = wp.zeros(n_agents, dtype=float, device=device)
    brake = wp.zeros(n_agents, dtype=float, device=device)
    max_turn = wp.zeros(n_agents, dtype=float, device=device)
    max_energy = wp.zeros(n_agents, dtype=float, device=device)
    base_drain = wp.zeros(n_agents, dtype=float, device=device)
    sprint_drain = wp.zeros(n_agents, dtype=float, device=device)
    radius = wp.zeros(n_agents, dtype=float, device=device)

    agents: list[AgentLoco] = []
    stats_list = []
    h_pos = np.zeros((n_agents, 2), dtype=np.float32)
    h_heading = np.zeros(n_agents, dtype=np.float32)
    h_speed = np.zeros(n_agents, dtype=np.float32)
    h_energy = np.zeros(n_agents, dtype=np.float32)
    h_th = np.zeros(n_agents, dtype=np.float32)
    h_st = np.zeros(n_agents, dtype=np.float32)
    h_ms = np.zeros(n_agents, dtype=np.float32)
    h_acc = np.zeros(n_agents, dtype=np.float32)
    h_br = np.zeros(n_agents, dtype=np.float32)
    h_mt = np.zeros(n_agents, dtype=np.float32)
    h_me = np.zeros(n_agents, dtype=np.float32)
    h_bd = np.zeros(n_agents, dtype=np.float32)
    h_sd = np.zeros(n_agents, dtype=np.float32)
    h_r = np.zeros(n_agents, dtype=np.float32)

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
        h_ms[i] = st.max_speed
        h_acc[i] = st.accel
        h_br[i] = st.brake
        h_mt[i] = st.max_turn_rate
        h_me[i] = st.max_energy
        h_bd[i] = st.base_drain
        h_sd[i] = st.sprint_drain
        h_r[i] = st.body_radius

    wp.copy(max_speed, wp.from_numpy(h_ms, dtype=float, device=device))
    wp.copy(accel, wp.from_numpy(h_acc, dtype=float, device=device))
    wp.copy(brake, wp.from_numpy(h_br, dtype=float, device=device))
    wp.copy(max_turn, wp.from_numpy(h_mt, dtype=float, device=device))
    wp.copy(max_energy, wp.from_numpy(h_me, dtype=float, device=device))
    wp.copy(base_drain, wp.from_numpy(h_bd, dtype=float, device=device))
    wp.copy(sprint_drain, wp.from_numpy(h_sd, dtype=float, device=device))
    wp.copy(radius, wp.from_numpy(h_r, dtype=float, device=device))

    raw_actions = [Action.from_dict(a) for a in golden["actions"]]
    half = 80.0

    for t in range(n_steps):
        for i in range(n_agents):
            applied = apply_envelope(agents[i], raw_actions[i], stats_list[i].uses_pitch)
            h_th[i] = applied.throttle
            h_st[i] = applied.steer
            h_pos[i, 0] = agents[i].pos_x
            h_pos[i, 1] = agents[i].pos_z
            h_heading[i] = agents[i].heading
            h_speed[i] = agents[i].speed
            h_energy[i] = agents[i].energy
        wp.copy(pos, wp.from_numpy(h_pos, dtype=wp.vec2, device=device))
        wp.copy(heading, wp.from_numpy(h_heading, dtype=float, device=device))
        wp.copy(speed, wp.from_numpy(h_speed, dtype=float, device=device))
        wp.copy(energy, wp.from_numpy(h_energy, dtype=float, device=device))
        wp.copy(throttle, wp.from_numpy(h_th, dtype=float, device=device))
        wp.copy(steer, wp.from_numpy(h_st, dtype=float, device=device))
        wp.launch(
            step_loco_kernel,
            dim=n_agents,
            inputs=[
                pos, heading, speed, energy, throttle, steer,
                max_speed, accel, brake, max_turn, max_energy,
                base_drain, sprint_drain, half, radius,
            ],
            device=device,
        )
        h_pos = pos.numpy()
        h_heading = heading.numpy()
        h_speed = speed.numpy()
        h_energy = energy.numpy()
        for i in range(n_agents):
            agents[i].pos_x = np.float32(h_pos[i, 0])
            agents[i].pos_z = np.float32(h_pos[i, 1])
            agents[i].heading = np.float32(h_heading[i])
            agents[i].speed = np.float32(h_speed[i])
            agents[i].energy = np.float32(h_energy[i])
            pos_np[t, i] = h_pos[i]
            heading_np[t, i] = h_heading[i]
            energy_np[t, i] = h_energy[i]
        _ = clamp_arena, step_loco  # host fallbacks remain imported for tests
    return pos_np, heading_np, energy_np

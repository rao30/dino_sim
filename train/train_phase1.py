"""Phase 1 training: homeostasis + hunt economics on the C++ island, league snapshots, A/B eval."""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "train"))

from dino_env.env import ACT_DIM, OBS_DIM, ChaseEnv  # noqa: E402
from league import League  # noqa: E402
from ppo import ARCHETYPE_NAMES, HIDDEN, PpoTrainer, env_to_policy_t, gae_t  # noqa: E402


def _knobs_batch(env: ChaseEnv) -> np.ndarray:
    return np.tile(env.knobs.as_array(), (env.n_worlds, env.n_raptors, 1))


def _rollout_metrics(env: ChaseEnv, ticks: int) -> dict:
    env._copy_metrics()
    attacks = sum(w["attacks"] for w in env.worlds)
    rear = sum(w["rear"] for w in env.worlds)
    kills = sum(w["kills"] for w in env.worlds)
    imm = sum(w["imm"] for w in env.worlds)
    resp = sum(w["respawn"] for w in env.worlds)
    by_arch_kills = {n: 0 for n in ARCHETYPE_NAMES}
    by_arch_deaths = {n: 0 for n in ARCHETYPE_NAMES}
    by_arch_n = {n: 0 for n in ARCHETYPE_NAMES}
    for w in env.worlds:
        a = int(w.get("player_archetype", -1))
        if 0 <= a < len(ARCHETYPE_NAMES):
            name = ARCHETYPE_NAMES[a]
            by_arch_n[name] += 1
            by_arch_kills[name] += int(w.get("player_kills", 0))
            by_arch_deaths[name] += int(w.get("player_deaths", 0))
    player_kill_rate = {}
    for n in ARCHETYPE_NAMES:
        den = max(by_arch_n[n], 1)
        player_kill_rate[n] = by_arch_kills[n] / den
    return {
        "ticks": ticks,
        "worlds": env.n_worlds,
        "kills": kills,
        "attacks": attacks,
        "rear_arc_fraction": (rear / attacks) if attacks else 0.0,
        "immigrant_rate": imm / max(imm + resp, 1),
        "player_kill_rate": player_kill_rate,
        "player_deaths": by_arch_deaths,
    }


def run_policy(env: ChaseEnv, trainer: PpoTrainer | None, ticks: int, deterministic: bool) -> dict:
    obs = env.observe_raptors()
    zeros = np.zeros((env.n_worlds, env.n_raptors, 7), dtype=np.float32)
    for _ in range(ticks):
        if trainer is None:
            env_act = zeros
        else:
            env_act, _, _, _, _, _ = trainer.act(obs, deterministic=deterministic)
        obs, _, done = env.step_raptor_actions(env_act)
        if trainer is not None:
            trainer.reset_hidden(done)
    return _rollout_metrics(env, ticks)


def make_env(
    worlds: int,
    seed: int,
    player: bool,
    scripted: bool = False,
    n_raptors: int = 12,
    n_gallis: int = 20,
    half_extent: float = 96.0,
) -> ChaseEnv:
    return ChaseEnv(
        n_worlds=worlds,
        n_raptors=n_raptors,
        n_gallis=n_gallis,
        seed=seed,
        player=player,
        scripted_raptors=scripted,
        half_extent=half_extent,
    )


def _configure_torch(device: str) -> None:
    torch.set_num_threads(1)
    try:
        torch.set_num_interop_threads(1)
    except RuntimeError:
        pass
    if device.startswith("cuda"):
        torch.backends.cuda.matmul.allow_tf32 = True
        torch.backends.cudnn.allow_tf32 = True
        torch.backends.cudnn.benchmark = True
        try:
            torch.set_float32_matmul_precision("high")
        except Exception:
            pass


class Logger:
    def __init__(self, path: Path):
        path.parent.mkdir(parents=True, exist_ok=True)
        self.path = path
        self._fp = path.open("w", encoding="utf-8")

    def log(self, msg: str) -> None:
        print(msg, flush=True)
        self._fp.write(msg + "\n")
        self._fp.flush()

    def close(self) -> None:
        self._fp.close()


def train(
    steps: int,
    worlds: int,
    device: str,
    n_raptors: int,
    n_gallis: int,
    half_extent: float,
    rollout: int | None = None,
    minibatch: int | None = None,
    bc_iters: int = 60,
    reset_every: int = 0,
    eval_every: int = 20,
    teacher_frac: float = 0.3,
    until_gate: bool = True,
    seed: int = 0,
    run_dir: Path | None = None,
):
    _configure_torch(device)
    run_dir = Path(run_dir) if run_dir else ROOT / "train" / "runs"
    run_dir.mkdir(parents=True, exist_ok=True)
    ckpt_path = run_dir / "raptor_latest.pt"
    best_path = run_dir / "raptor_best.pt"
    log = Logger(run_dir / "train.log")
    env = make_env(
        worlds, seed=seed, player=True, scripted=False, n_raptors=n_raptors, n_gallis=n_gallis, half_extent=half_extent
    )
    trainer = PpoTrainer(env, device=device)
    if rollout is not None:
        trainer.cfg.rollout = rollout
    if minibatch is not None:
        trainer.cfg.minibatch = minibatch
    league = League(run_dir / "snapshots")
    rng = np.random.default_rng(seed)
    T = trainer.cfg.rollout
    wr = trainer.wr
    obs = env.observe_raptors()
    total = 0
    iter_i = 0
    best_eval_kills = -1
    log.log(
        f"train device={device} worlds={env.n_worlds} raptors={env.n_raptors} gallis={env.n_gallis} "
        f"arena={env.half_extent:.0f} learners={wr} rollout={T} mb={trainer.cfg.minibatch} "
        f"hidden={HIDDEN} gamma={trainer.cfg.gamma} bc_iters={bc_iters} teacher_frac={teacher_frac} "
        f"until_gate={until_gate} seed={seed} run_dir={run_dir} "
        f"cuda_graph={trainer._graph is not None} bf16={trainer._bf16}"
    )

    buf_obs = torch.empty(T, wr, OBS_DIM, device=device)
    buf_act = torch.empty(T, wr, ACT_DIM, device=device)
    buf_logp = torch.empty(T, wr, device=device)
    buf_val = torch.empty(T, wr, device=device)
    buf_rew = torch.empty(T, wr, device=device)
    buf_done = torch.empty(T, wr, device=device)
    buf_hid = torch.empty(T, wr, HIDDEN, device=device)
    buf_teacher = torch.empty(T, wr, ACT_DIM, device=device)

    def collect(force_teacher: bool, mix: float) -> float:
        nonlocal obs, total
        for t in range(T):
            teacher_np = env.teacher_actions()
            env_act, raw, logp, val, hid_b, o = trainer.act(obs)
            buf_obs[t].copy_(o)
            buf_act[t].copy_(raw)
            buf_logp[t].copy_(logp)
            buf_val[t].copy_(val)
            buf_hid[t].copy_(hid_b)
            buf_teacher[t].copy_(torch.from_numpy(teacher_np.reshape(wr, ACT_DIM)), non_blocking=True)
            use_t = force_teacher or bool(rng.random() < mix)
            step_act = teacher_np if use_t else env_act
            obs, rew, done = env.step_raptor_actions(step_act)
            buf_rew[t].copy_(torch.from_numpy(np.ascontiguousarray(rew).reshape(wr)), non_blocking=True)
            buf_done[t].copy_(torch.from_numpy(np.ascontiguousarray(done).reshape(wr)), non_blocking=True)
            trainer.reset_hidden(done)
            total += wr
        return float(buf_rew.mean())

    def bc_update() -> float:
        n = T * wr
        obs_f = buf_obs.reshape(n, OBS_DIM)
        hid_f = buf_hid.reshape(n, HIDDEN)
        tea_f = buf_teacher.reshape(n, ACT_DIM)
        last_v = trainer.bootstrap_value(obs)
        _, ret = gae_t(buf_rew, buf_val, buf_done, last_v, trainer.cfg.gamma, trainer.cfg.lam)
        ret_f = ret.reshape(n)
        t_steps = T
        bc_w = torch.tensor([2.0, 1.0, 0.3, 4.0, 0.5, 0.3, 0.3], device=device)
        last = 0.0
        mb = min(trainer.cfg.minibatch, n)
        for _ in range(4):
            perm = torch.randperm(n, device=device)
            for start in range(0, n, mb):
                idx = perm[start : start + mb]
                sp = trainer._sp.repeat(t_steps)[idx]
                kn = trainer.knobs.repeat(t_steps, 1)[idx]
                mu, _, v, _ = trainer.net(obs_f[idx], hid_f[idx], sp, kn)
                tgt = env_to_policy_t(tea_f[idx])
                bc = ((mu - tgt).pow(2) * bc_w).mean()
                vl = torch.nn.functional.mse_loss(v, ret_f[idx])
                loss = bc + 0.05 * vl
                trainer.opt.zero_grad(set_to_none=True)
                loss.backward()
                torch.nn.utils.clip_grad_norm_(trainer.net.parameters(), 1.0)
                trainer.opt.step()
                last = float(bc.detach())
        return last

    def run_gate() -> dict:
        metrics = eval_once(ckpt_path, ticks=400, worlds=8, device=device)
        out = run_dir / "last_eval.json"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps({"phase1": metrics}, indent=2))
        return metrics

    for bc_i in range(bc_iters):
        t0 = time.perf_counter()
        mean_rew = collect(force_teacher=True, mix=1.0)
        last = bc_update()
        dt = time.perf_counter() - t0
        log.log(f"bc={bc_i + 1}/{bc_iters} loss={last:.4f} rew={mean_rew:.4f} sps={(wr * T) / max(dt, 1e-6):.0f}")

    trainer.save(ckpt_path)
    env._copy_metrics()
    kills0 = sum(w["kills"] for w in env.worlds)
    attacks0 = sum(w["attacks"] for w in env.worlds)
    mix = teacher_frac
    gated = False

    while total < steps:
        t0 = time.perf_counter()
        mean_rew = collect(force_teacher=False, mix=mix)
        t_collect = time.perf_counter()
        last_v = trainer.bootstrap_value(obs)
        adv, ret = gae_t(buf_rew, buf_val, buf_done, last_v, trainer.cfg.gamma, trainer.cfg.lam)
        loss = trainer.update(
            buf_obs.reshape(T * wr, OBS_DIM),
            buf_act.reshape(T * wr, ACT_DIM),
            buf_logp.reshape(T * wr),
            adv.reshape(T * wr),
            ret.reshape(T * wr),
            buf_hid.reshape(T * wr, HIDDEN),
            teacher=buf_teacher.reshape(T * wr, ACT_DIM),
            bc_coef=trainer.cfg.bc_coef,
        )
        iter_i += 1
        dt = time.perf_counter() - t0
        update_ms = (time.perf_counter() - t_collect) * 1000.0
        collect_ms = (t_collect - t0) * 1000.0
        sps = (wr * T) / max(dt, 1e-6)
        env._copy_metrics()
        kills = sum(w["kills"] for w in env.worlds)
        attacks = sum(w["attacks"] for w in env.worlds)
        dk, da = kills - kills0, attacks - attacks0
        kills0, attacks0 = kills, attacks
        if da <= 0:
            mix = min(0.8, mix + 0.1)
            log.log(f"rescue bc mix={mix:.2f} (no attacks this window)")
            collect(force_teacher=True, mix=1.0)
            bc_update()
        else:
            mix = max(0.1, mix * 0.98)

        if iter_i <= 5 or iter_i % 5 == 0:
            league.add(trainer.net.state_dict(), total)
            trainer.save(ckpt_path)
            log.log(
                f"step={total} loss={loss:.4f} rew={mean_rew:.4f} "
                f"kills={dk} attacks={da} mix={mix:.2f} sps={sps:.0f}"
            )

        if eval_every > 0 and iter_i % eval_every == 0:
            trainer.save(ckpt_path)
            metrics = run_gate()
            lk, sk = int(metrics["kills"]), int(metrics["scripted_kills"])
            la = int(metrics["learned"]["attacks"])
            sa = int(metrics["scripted"]["attacks"])
            log.log(
                f"eval kills={lk}/{sk} attacks={la}/{sa} "
                f"rear={metrics['rear_arc_fraction']:.3f}/{metrics['scripted_rear_arc']:.3f}"
            )
            if lk > best_eval_kills:
                best_eval_kills = lk
                trainer.save(best_path)
                log.log(f"best checkpoint kills={lk} -> {best_path}")
            if until_gate and lk >= sk and la > 0:
                log.log(f"GATE passed: learned {lk} kills >= scripted {sk}")
                gated = True
                break

    trainer.save(ckpt_path)
    if not gated and until_gate:
        log.log("max steps reached without gate")
    env.close()
    log.close()


def eval_once(path: Path | None, ticks: int = 400, worlds: int = 8, device: str = "cpu") -> dict:
    learned_env = make_env(worlds, seed=123, player=True, scripted=False)
    trainer = PpoTrainer(learned_env, device=device)
    if path and path.exists():
        trainer.load(path)
    learned = run_policy(learned_env, trainer, ticks, deterministic=True)
    learned_env.close()

    scripted_env = make_env(worlds, seed=123, player=True, scripted=True)
    scripted = run_policy(scripted_env, None, ticks, deterministic=True)
    scripted_env.close()

    trans = {}
    snaps = ROOT / "train" / "snapshots"
    league = League(snaps)
    if len(league.entries) >= 2:
        env_a = make_env(4, seed=7, player=False, scripted=False)
        env_b = make_env(4, seed=7, player=False, scripted=False)
        ta = PpoTrainer(env_a, device=device)
        tb = PpoTrainer(env_b, device=device)
        ta.net.load_state_dict(torch_load(league.entries[-1].path))
        tb.net.load_state_dict(torch_load(league.entries[-2].path))
        ma = run_policy(env_a, ta, 200, True)
        mb = run_policy(env_b, tb, 200, True)
        wr = 1.0 if ma["kills"] > mb["kills"] else 0.0
        trans = league.transitivity_report({"cur>hist": wr})
        env_a.close()
        env_b.close()

    return {
        "rear_arc_fraction": learned["rear_arc_fraction"],
        "scripted_rear_arc": scripted["rear_arc_fraction"],
        "immigrant_rate": learned["immigrant_rate"],
        "kills": learned["kills"],
        "scripted_kills": scripted["kills"],
        "player_kill_rate": learned["player_kill_rate"],
        "terrain_drop": 0.0,
        "transitivity": trans,
        "learned": learned,
        "scripted": scripted,
    }


def torch_load(path: Path):
    return torch.load(path, map_location="cpu", weights_only=True)


def default_device() -> str:
    return "cuda" if torch.cuda.is_available() else "cpu"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--steps", type=int, default=2_000_000_000)
    p.add_argument("--worlds", type=int, default=64)
    p.add_argument("--raptors", type=int, default=12)
    p.add_argument("--gallis", type=int, default=20)
    p.add_argument("--arena", type=float, default=96.0, help="arena half-extent")
    p.add_argument("--rollout", type=int, default=256)
    p.add_argument("--minibatch", type=int, default=16384)
    p.add_argument("--bc-iters", type=int, default=60)
    p.add_argument("--reset-every", type=int, default=0)
    p.add_argument("--eval-every", type=int, default=20)
    p.add_argument("--teacher-frac", type=float, default=0.3)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--run-dir", type=str, default=None, help="checkpoint/log dir (default train/runs)")
    p.add_argument("--no-gate", action="store_true")
    p.add_argument("--eval", action="store_true")
    p.add_argument("--eval-ckpt", type=str, default=None)
    p.add_argument("--device", default=None)
    args = p.parse_args()
    device = args.device or default_device()
    print(f"device={device}", flush=True)
    run_dir = Path(args.run_dir) if args.run_dir else ROOT / "train" / "runs"
    if args.eval:
        ckpt = Path(args.eval_ckpt) if args.eval_ckpt else run_dir / "raptor_latest.pt"
        metrics = eval_once(ckpt, device=device)
        out = run_dir / "last_eval.json"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps({"phase1": metrics}, indent=2))
        print(json.dumps(metrics, indent=2))
        return
    train(
        args.steps,
        args.worlds,
        device,
        n_raptors=args.raptors,
        n_gallis=args.gallis,
        half_extent=args.arena,
        rollout=args.rollout,
        minibatch=args.minibatch,
        bc_iters=args.bc_iters,
        reset_every=args.reset_every,
        eval_every=args.eval_every,
        teacher_frac=args.teacher_frac,
        until_gate=not args.no_gate,
        seed=args.seed,
        run_dir=run_dir,
    )


if __name__ == "__main__":
    main()

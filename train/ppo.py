"""PPO with GRU + FiLM knobs. Policy actions stay in [-1, 1]; env throttle is mapped separately."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import math

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

from dino_env.env import ACT_DIM, OBS_DIM, ChaseEnv


HIDDEN = 512
ARCHETYPE_NAMES = ("kiter", "tank", "fleer", "puller", "idle")
_LOG2PI = math.log(2.0 * math.pi)


def _normal_logp(mu: torch.Tensor, std: torch.Tensor, act: torch.Tensor) -> torch.Tensor:
    z = (act - mu) / std
    return (-0.5 * (z.square() + 2.0 * std.log() + _LOG2PI)).sum(-1)


def _normal_entropy(std: torch.Tensor) -> torch.Tensor:
    return (std.log() + 0.5 * (1.0 + _LOG2PI)).sum(-1)


class FilmGruPolicy(nn.Module):
    def __init__(self, obs_dim: int = OBS_DIM, act_dim: int = ACT_DIM, hidden: int = HIDDEN):
        super().__init__()
        self.species_emb = nn.Embedding(21, 8)
        self.enc = nn.Linear(obs_dim + 8, hidden)
        self.knob_film = nn.Linear(5, hidden * 2)
        self.gru = nn.GRUCell(hidden, hidden)
        self.mu = nn.Linear(hidden, act_dim)
        nn.init.zeros_(self.mu.bias)
        with torch.no_grad():
            self.mu.bias[0] = 0.85  # throttle: sprint
            self.mu.bias[3] = 0.7  # attack: well above threshold
        self.log_std = nn.Parameter(torch.zeros(act_dim) - 0.5)
        self.v = nn.Linear(hidden, 1)

    def forward(self, obs, hid, species, knobs):
        e = self.species_emb(species)
        x = torch.tanh(self.enc(torch.cat([obs, e], dim=-1)))
        gb = self.knob_film(knobs)
        gamma, beta = gb.chunk(2, dim=-1)
        x = x * (1 + torch.tanh(gamma)) + beta
        hid = self.gru(x, hid)
        mu = torch.tanh(self.mu(hid))
        value = self.v(hid).squeeze(-1)
        return mu, self.log_std.exp().clamp(1e-3, 2.0).expand_as(mu), value, hid


def policy_to_env_t(act: torch.Tensor) -> torch.Tensor:
    out = act.clone()
    out[..., 0] = (act[..., 0] + 1.0) * 0.5
    out[..., 3] = (act[..., 3] + 1.0) * 0.5
    out[..., 4] = (act[..., 4] + 1.0) * 0.5
    out[..., 5] = (act[..., 5] + 1.0) * 0.5
    out[..., 6] = (act[..., 6] + 1.0) * 2.0  # signal_type 0..4
    return out


def env_to_policy_t(act: torch.Tensor) -> torch.Tensor:
    out = act.clone()
    out[..., 0] = act[..., 0] * 2.0 - 1.0
    out[..., 3] = act[..., 3] * 2.0 - 1.0
    out[..., 4] = act[..., 4] * 2.0 - 1.0
    out[..., 5] = act[..., 5] * 2.0 - 1.0
    out[..., 6] = act[..., 6] * 0.5 - 1.0
    return out.clamp(-1.0, 1.0)


def policy_to_env(act: np.ndarray) -> np.ndarray:
    """Map policy [-1,1] to engine Action (throttle/attack/special/sig in [0,1])."""
    return policy_to_env_t(torch.as_tensor(act)).numpy().astype(np.float32)


def gae_t(rew, val, done, last_val, gamma=0.997, lam=0.95):
    t = rew.shape[0]
    adv = torch.zeros_like(rew)
    last = torch.zeros(rew.shape[1], device=rew.device, dtype=rew.dtype)
    for i in range(t - 1, -1, -1):
        nxt = last_val if i == t - 1 else val[i + 1]
        nonterm = 1.0 - done[i]
        delta = rew[i] + gamma * nxt * nonterm - val[i]
        last = delta + gamma * lam * nonterm * last
        adv[i] = last
    return adv, adv + val


@dataclass
class PpoCfg:
    lr: float = 1e-4
    gamma: float = 0.997
    lam: float = 0.95
    clip: float = 0.1
    epochs: int = 3
    rollout: int = 256
    minibatch: int = 16384
    entropy: float = 0.015
    value_coef: float = 0.1
    bc_coef: float = 0.8


class PpoTrainer:
    def __init__(self, env: ChaseEnv, device: str = "cpu"):
        self.env = env
        self.device = device
        self._cuda = device.startswith("cuda")
        self._bf16 = bool(self._cuda and torch.cuda.is_bf16_supported())
        self.net = FilmGruPolicy().to(device)
        try:
            self.opt = torch.optim.Adam(self.net.parameters(), lr=PpoCfg.lr, fused=self._cuda)
        except (TypeError, RuntimeError):
            self.opt = torch.optim.Adam(self.net.parameters(), lr=PpoCfg.lr)
        self.cfg = PpoCfg()
        w, r = env.n_worlds, env.n_raptors
        self.wr = w * r
        self.n_worlds = w
        self.n_raptors = r
        self.hid = torch.zeros(self.wr, HIDDEN, device=device)
        self._sp = torch.zeros(self.wr, dtype=torch.long, device=device)
        kn = torch.as_tensor(env.knobs.as_array(), device=device)
        self.knobs = kn.unsqueeze(0).expand(self.wr, -1).contiguous()
        self._obs_t = torch.empty(self.wr, OBS_DIM, device=device)
        if self._cuda:
            self._obs_pin = torch.empty(self.wr, OBS_DIM, pin_memory=True)
            self._act_pin = torch.empty(self.wr, ACT_DIM, pin_memory=True)
        else:
            self._obs_pin = None
            self._act_pin = torch.empty(self.wr, ACT_DIM)
        self._act_np = self._act_pin.numpy().reshape(w, r, ACT_DIM)
        self._graph = None
        self._g_obs = None
        self._capture_cuda_graph()

    def _autocast(self):
        return torch.autocast(device_type="cuda", dtype=torch.bfloat16, enabled=self._bf16)

    def _capture_cuda_graph(self) -> None:
        if not self._cuda:
            return
        try:
            obs = torch.zeros(self.wr, OBS_DIM, device=self.device)
            hid = torch.zeros(self.wr, HIDDEN, device=self.device)
            s = torch.cuda.Stream()
            s.wait_stream(torch.cuda.current_stream())
            with torch.cuda.stream(s):
                for _ in range(3):
                    self.net(obs, hid, self._sp, self.knobs)
            torch.cuda.current_stream().wait_stream(s)
            self._g_obs = obs
            self._g_hid = hid
            g = torch.cuda.CUDAGraph()
            with torch.cuda.graph(g):
                self._g_mu, self._g_std, self._g_v, self._g_hid_out = self.net(self._g_obs, self._g_hid, self._sp, self.knobs)
            self._graph = g
        except Exception:
            self._graph = None

    def _forward(self, obs: torch.Tensor, hid: torch.Tensor):
        if self._graph is not None and obs.shape[0] == self.wr:
            self._g_obs.copy_(obs)
            self._g_hid.copy_(hid)
            self._graph.replay()
            return self._g_mu, self._g_std, self._g_v, self._g_hid_out
        with self._autocast():
            mu, std, v, hid_out = self.net(obs, hid, self._sp[: obs.shape[0]], self.knobs[: obs.shape[0]])
        return mu.float(), std.float(), v.float(), hid_out.float()

    def zero_hidden(self):
        self.hid.zero_()

    @torch.inference_mode()
    def act(self, obs: np.ndarray, deterministic: bool = False):
        src = np.ascontiguousarray(obs).reshape(self.wr, OBS_DIM)
        if self._obs_pin is not None:
            self._obs_pin.copy_(torch.from_numpy(src))
            self._obs_t.copy_(self._obs_pin, non_blocking=True)
            o = self._obs_t
        else:
            o = torch.from_numpy(src).to(self.device)
            self._obs_t.copy_(o)
            o = self._obs_t
        hid_before = self.hid
        mu, std, v, hid = self._forward(o, self.hid)
        if deterministic:
            a = mu.clamp(-1, 1)
        else:
            a = (mu + std * torch.randn_like(mu)).clamp(-1, 1)
        logp = _normal_logp(mu, std, a)
        self.hid = hid.clone() if self._graph is not None else hid
        env_t = policy_to_env_t(a)
        self._act_pin.copy_(env_t, non_blocking=False)
        return self._act_np, a, logp, v, hid_before, o

    @torch.inference_mode()
    def bootstrap_value(self, obs: np.ndarray) -> torch.Tensor:
        src = np.ascontiguousarray(obs).reshape(self.wr, OBS_DIM)
        if self._obs_pin is not None:
            self._obs_pin.copy_(torch.from_numpy(src))
            self._obs_t.copy_(self._obs_pin, non_blocking=True)
            o = self._obs_t
        else:
            o = torch.from_numpy(src).to(self.device)
        _, _, v, _ = self._forward(o, self.hid)
        return v.clone() if self._graph is not None else v

    def reset_hidden(self, mask: np.ndarray):
        if not np.any(mask):
            return
        m = torch.from_numpy(np.ascontiguousarray(mask, dtype=np.float32).reshape(-1)).to(self.device)
        self.hid = self.hid * (1.0 - m.unsqueeze(-1))

    def bc_loss(self, obs_t: torch.Tensor, hid_t: torch.Tensor, teacher_env: torch.Tensor) -> torch.Tensor:
        teacher = env_to_policy_t(teacher_env)
        mu, _, _, _ = self.net(obs_t, hid_t, self._sp, self.knobs)
        return F.mse_loss(mu, teacher)

    def update(self, obs, act, old_logp, adv, ret, hid, teacher=None, bc_coef: float | None = None):
        cfg = self.cfg
        adv = (adv - adv.mean()) / (adv.std() + 1e-8)
        n = int(adv.shape[0])
        t_steps = max(n // self.wr, 1)
        kn = self.knobs.repeat(t_steps, 1)
        sp_all = self._sp.repeat(t_steps)
        mb = min(int(cfg.minibatch), n)
        coef = cfg.bc_coef if bc_coef is None else bc_coef
        last = None
        w = torch.tensor([2.0, 1.0, 0.3, 4.0, 0.5, 0.3, 0.3], device=self.device)
        for _ in range(cfg.epochs):
            perm = torch.randperm(n, device=self.device)
            for start in range(0, n, mb):
                idx = perm[start : start + mb]
                with self._autocast():
                    mu, std, v, _ = self.net(obs[idx], hid[idx], sp_all[idx], kn[idx])
                mu, std, v = mu.float(), std.float(), v.float()
                logp = _normal_logp(mu, std, act[idx])
                ratio = (logp - old_logp[idx]).exp()
                a = adv[idx]
                surr = torch.min(ratio * a, ratio.clamp(1 - cfg.clip, 1 + cfg.clip) * a)
                policy_loss = -surr.mean()
                value_loss = F.mse_loss(v, ret[idx])
                ent = _normal_entropy(std).mean()
                loss = policy_loss + cfg.value_coef * value_loss - cfg.entropy * ent
                if teacher is not None and coef > 0:
                    tgt = env_to_policy_t(teacher[idx])
                    loss = loss + coef * ((mu - tgt).pow(2) * w).mean()
                self.opt.zero_grad(set_to_none=True)
                loss.backward()
                nn.utils.clip_grad_norm_(self.net.parameters(), 1.0)
                self.opt.step()
                last = loss
        return float(last.detach()) if last is not None else 0.0

    def save(self, path: Path):
        path.parent.mkdir(parents=True, exist_ok=True)
        torch.save(
            {
                "model": self.net.state_dict(),
                "obs_dim": OBS_DIM,
                "act_dim": ACT_DIM,
                "hidden": HIDDEN,
            },
            path,
        )

    def load(self, path: Path):
        ckpt = torch.load(path, map_location=self.device, weights_only=True)
        self.net.load_state_dict(ckpt["model"])

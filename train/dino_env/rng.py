"""Portable xorshift64 matching `dino_loco::rng`."""

import numpy as np


class Rng:
    def __init__(self, seed: int) -> None:
        self.state = np.uint64(0x9E3779B97F4A7C15 if seed == 0 else seed)

    def next_u64(self) -> np.uint64:
        x = self.state
        x ^= np.uint64(x << np.uint64(13))
        x ^= np.uint64(x >> np.uint64(7))
        x ^= np.uint64(x << np.uint64(17))
        self.state = x
        return x

    def next_u32(self) -> np.uint32:
        return np.uint32(self.next_u64() >> np.uint64(32))

    def next_f32(self) -> np.float32:
        return np.float32(self.next_u32() >> np.uint32(8)) / np.float32(16777216.0)

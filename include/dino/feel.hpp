#pragma once

#include "dino/scripted.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace dino {

/// Phase 2 feel harness. Godot wraps this; the island still ticks here.
class DinoIslandFeel {
public:
    DinoIslandFeel();
    void reset(int64_t seed, bool naive);
    void spawn_player_archetype(int32_t archetype);
    void spawn_trike();
    void set_raptor_knobs(float aggression, float persistence, float caution, float sociality, float hunger);
    void physics_process();
    void set_paused(bool paused) { paused_ = paused; }
    void set_naive(bool naive) { naive_ = naive; }
    bool naive() const { return naive_; }
    void enable_lod(bool enabled) { island_.lod_enabled = enabled; }

    int32_t agent_count() const { return static_cast<int32_t>(island_.agents.size()); }
    void agent_position(int32_t index, float& x, float& y, float& z) const;
    float agent_heading(int32_t index) const;
    int32_t agent_species(int32_t index) const;
    int32_t agent_lod(int32_t index) const;
    int32_t raptor_kills() const { return static_cast<int32_t>(island_.metrics.raptor_kills); }
    float rear_arc_fraction() const { return island_.metrics.rear_arc_fraction(); }
    float immigrant_rate() const { return island_.metrics.immigrant_rate(); }
    int32_t lod_policy_count() const;
    float arena_half_extent() const { return island_.arena.half_extent; }
    int32_t gru_resets_last_tick() const;
    float lod_l0_hz() const { return lod_hz(LodTier::L0); }
    float agent_energy_frac(int32_t index) const;
    bool agent_feeding(int32_t index) const;
    float mean_raptor_hunger() const;

    Island& island() { return island_; }
    const Island& island() const { return island_; }

private:
    Island island_;
    SpatialHash hash_;
    std::vector<Action> actions_;
    bool paused_ = false;
    bool naive_ = false;
};

std::string dump_golden_json(uint32_t n_steps);
bool write_golden_json(const std::string& path, uint32_t n_steps);

}  // namespace dino

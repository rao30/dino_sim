#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "dino/feel.hpp"

namespace godot {

class DinoIslandNode : public Node3D {
    GDCLASS(DinoIslandNode, Node3D)

    dino::DinoIslandFeel feel_;

protected:
    static void _bind_methods();

public:
    DinoIslandNode() = default;
    void _physics_process(double delta) override;
    void reset(int64_t seed, bool naive);
    void spawn_player_archetype(int32_t archetype);
    void spawn_trike();
    void set_raptor_knobs(float aggression, float persistence, float caution, float sociality, float hunger);
    int32_t agent_count() const;
    Vector3 agent_position(int32_t index) const;
    float agent_heading(int32_t index) const;
    int32_t agent_species(int32_t index) const;
    int32_t agent_lod(int32_t index) const;
    int32_t raptor_kills() const;
    float rear_arc_fraction() const;
    float immigrant_rate() const;
    void set_paused(bool paused);
    void set_naive(bool naive);
    bool is_naive() const;
    void enable_lod(bool enabled);
    int32_t lod_policy_count() const;
    float arena_half_extent() const;
    int32_t gru_resets_last_tick() const;
    float lod_l0_hz() const;
    float agent_energy_frac(int32_t index) const;
    bool agent_feeding(int32_t index) const;
    float mean_raptor_hunger() const;
};

}  // namespace godot

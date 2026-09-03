#pragma once

#include "dino/math.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace dino {

constexpr float ATTACK_THRESHOLD = 0.5f;
constexpr uint8_t SIGNAL_NONE = 0;
constexpr uint8_t SIGNAL_ALARM = 1;
constexpr uint8_t SIGNAL_CONTACT = 2;
constexpr uint8_t SIGNAL_THREAT = 3;
constexpr uint8_t SIGNAL_SOCIAL = 4;
constexpr int SPECIES_COUNT = 21;
constexpr int KNOB_COUNT = 5;

enum class Species : uint8_t {
    Utahraptor = 0,
    Gallimimus = 1,
    Triceratops = 2,
    Dodo = 3,
    Lystrosaurus = 4,
    Parasaur = 5,
    Stegosaurus = 6,
    Pachycephalosaurus = 7,
    Iguanodon = 8,
    Brontosaurus = 9,
    Ankylosaurus = 10,
    Therizinosaurus = 11,
    Compsognathus = 12,
    Dilophosaurus = 13,
    Troodon = 14,
    Pteranodon = 15,
    Carnotaurus = 16,
    Sarcosuchus = 17,
    Tyrannosaurus = 18,
    Spinosaurus = 19,
    Player = 20,
};

inline Species species_from_u8(uint8_t v) {
    return v > 20 ? Species::Player : static_cast<Species>(v);
}

struct SpeciesStats {
    float mass, max_speed, accel, brake, max_turn_rate;
    float max_health, max_energy, base_drain, sprint_drain;
    float attack_damage, attack_range;
    uint32_t attack_cooldown_ticks;
    float attack_arc, body_radius, fov, view_range, hearing_range;
    float danger_arc_origin, danger_arc;
    bool uses_pitch;
};

const SpeciesStats& species_stats(Species s);
const char* species_name(Species s);
bool is_learned_v1(Species s);
bool is_carnivore(Species s);
bool is_prey_for_raptor(Species s);

struct Action {
    float throttle = 0;
    float steer = 0;
    float pitch = 0;
    float attack = 0;
    float special = 0;
    float signal_intensity = 0;
    uint8_t signal_type = SIGNAL_NONE;

    static Action zero() { return {}; }
    Action saturate() const;
    static Action rate_limited(Action prev, Action target);
};

struct DesignerKnobs {
    float aggression = 0.5f;
    float persistence = 0.5f;
    float caution = 0.5f;
    float sociality = 0.5f;
    float hunger = 0.5f;  // designer bias: hungrier personality peels off to feed sooner

    DesignerKnobs saturate() const;
    std::array<float, KNOB_COUNT> as_array() const;
    static DesignerKnobs from_array(std::array<float, KNOB_COUNT> a);
    float hunt_energy_frac() const;
    float engage_range(float view_range) const;
    uint32_t chase_timeout_ticks() const;
    float pack_cohesion() const;
    float danger_respect() const;
};

enum class LodTier : uint8_t { L0 = 0, L1 = 1, L2 = 2, L3 = 3 };
float lod_hz(LodTier t);
uint32_t lod_ticks_per_decision(LodTier t);
bool lod_uses_full_policy(LodTier t);
bool lod_uses_scripted(LodTier t);

struct LodThresholds {
    float enter_l0 = 28, exit_l0 = 34;
    float enter_l1 = 70, exit_l1 = 82;
    float enter_l2 = 140, exit_l2 = 160;
};

struct LodState {
    LodTier tier = LodTier::L0;
    uint32_t ticks_in_tier = 0;
    uint32_t ticks_since_decision = 0;
    bool reset_gru = false;
    float ghost_pos_x = 0;
    float ghost_pos_z = 0;

    void step(float dist_to_focus, float pos_x, float pos_z, LodThresholds th);
    bool due_for_decision() const { return ticks_since_decision >= lod_ticks_per_decision(tier); }
    void mark_decided() { ticks_since_decision = 0; }
};

struct Rng {
    uint64_t state = 0x9E3779B97F4A7C15ull;
    explicit Rng(uint64_t seed = 1) { state = seed == 0 ? 0x9E3779B97F4A7C15ull : seed; }
    uint64_t next_u64();
    uint32_t next_u32() { return static_cast<uint32_t>(next_u64() >> 32); }
    float next_f32() { return static_cast<float>(next_u32() >> 8) / 16777216.0f; }
    float range(float lo, float hi) { return lo + (hi - lo) * next_f32(); }
};

enum class LocoState : uint8_t { Idle = 0, Walk = 1, Sprint = 2 };
inline LocoState loco_from_throttle(float throttle) {
    if (throttle < 0.05f) return LocoState::Idle;
    if (throttle < 0.7f) return LocoState::Walk;
    return LocoState::Sprint;
}

enum class ControlRole : uint8_t { Learned = 0, Scripted = 1, Player = 2 };
enum class PlayerArchetype : uint8_t { Kiter = 0, Tank = 1, Fleer = 2, Puller = 3, Idle = 4 };
inline PlayerArchetype player_archetype_from_u8(uint8_t v) {
    return v > 4 ? PlayerArchetype::Idle : static_cast<PlayerArchetype>(v);
}

struct Agent {
    uint32_t id = 0;
    Species species = Species::Dodo;
    ControlRole role = ControlRole::Scripted;
    std::optional<PlayerArchetype> player_archetype;
    Vec2 pos{};
    float heading = 0;
    float speed = 0;
    float pitch = 0;
    float energy = 0;
    float health = 0;
    uint32_t age_ticks = 0;
    bool alive = true;
    Action prev_action{};
    uint32_t ticks_since_steer_flip = 100;
    LocoState loco_state = LocoState::Idle;
    uint32_t ticks_in_loco_state = 100;
    uint32_t attack_cooldown = 0;
    bool feeding = false;
    float last_attack_heading_offset = 0;
    float last_damage_dealt = 0;
    bool last_kill = false;
    bool last_forced_heading = false;
    bool last_blocked_escape = false;
    DesignerKnobs knobs{};
    LodState lod{};
    uint32_t chase_ticks = 0;
    uint32_t ticks_motionless = 0;
    float path_length = 0;
    Vec2 net_origin{};

    static Agent spawn(uint32_t id, Species species, Vec2 pos, float heading);
    const SpeciesStats& stats() const { return species_stats(species); }
    float radius() const { return stats().body_radius; }
    Vec2 forward() const { return heading_vec(heading); }
    Vec2 vel() const { return forward().scale(speed); }
    float health_frac() const;
    float energy_frac() const;
    float net_displacement() const { return pos.sub(net_origin).length(); }
};

struct Carcass {
    Vec2 pos{};
    float calories = 0;
    float radius = 0;
    Species species = Species::Dodo;
};

struct Plant {
    Vec2 pos{};
    float calories = 0;
};

struct EpisodeMetrics {
    uint32_t ticks = 0;
    uint32_t raptor_kills = 0;
    uint32_t galli_deaths = 0;
    uint32_t player_kills_by_archetype[5]{};
    uint32_t player_deaths_by_archetype[5]{};
    uint32_t attacks = 0;
    uint32_t rear_arc_attacks = 0;
    uint32_t immigrants = 0;
    uint32_t births_or_respawns = 0;
    float jerk_sum = 0;
    uint32_t jerk_count = 0;
    uint32_t circling_flags = 0;
    uint32_t motionless_flags = 0;

    float rear_arc_fraction() const {
        return attacks == 0 ? 0.0f : static_cast<float>(rear_arc_attacks) / static_cast<float>(attacks);
    }
    float immigrant_rate() const {
        uint32_t den = births_or_respawns + immigrants;
        if (den < 1) den = 1;
        return static_cast<float>(immigrants) / static_cast<float>(den);
    }
    void record_attack(float heading_offset);
};

bool circling(const Agent& a);
std::size_t count_species(const std::vector<Agent>& agents, Species s);

float step_return(const Agent& agent, bool died, float gamma);
float participation_bonus(const Agent& agent);

}  // namespace dino

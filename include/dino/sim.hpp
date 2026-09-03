#pragma once

#include "dino/types.hpp"

#include <array>
#include <vector>

namespace dino {

struct LocoStepResult {
    float pos_x, pos_z, heading, speed, energy;
};

Action apply_envelope(Agent& agent, Action raw);
float heading_jerk(float h0, float h1, float h2);
LocoStepResult step_loco(float pos_x, float pos_z, float heading, float speed, float energy,
                         Action applied, const SpeciesStats& stats);
void apply_loco(Agent& agent, Action applied);
void clamp_to_arena(Agent& agent, float half);

struct CombatEvent {
    uint32_t attacker = 0;
    uint32_t victim = 0;
    float damage = 0;
    float heading_offset = 0;
    bool forced_heading = false;
    bool kill = false;
};

bool in_attack_arc(float attacker_heading, Vec2 attacker_pos, Vec2 target_pos, float arc);
float heading_offset_on_target(float target_heading, Vec2 target_pos, Vec2 attacker_pos);
bool in_danger_arc(const Agent& victim, Vec2 attacker_pos);
float angular_coverage(Vec2 prey_pos, const std::vector<std::pair<Vec2, bool>>& attacker_positions, float radius);
float escape_probability(uint32_t n_attackers, float coverage);
std::vector<CombatEvent> resolve_attacks(std::vector<Agent>& agents, const std::vector<Action>& actions);
Carcass spawn_carcass(const Agent& agent);
void feed_from_carcasses(std::vector<Agent>& agents, std::vector<Carcass>& carcasses);

struct Arena {
    float half_extent = 64.0f;
};

struct PopRule {
    Species species = Species::Dodo;
    uint32_t floor = 0;
    uint32_t ceiling = 0;
    uint32_t target = 0;
};

struct StepOutcome {
    std::vector<Action> applied;
    uint32_t raptor_kills_this_tick = 0;
};

struct Island {
    uint64_t tick = 0;
    Rng rng{1};
    Arena arena{};
    std::vector<Agent> agents;
    std::vector<Carcass> carcasses;
    std::vector<Plant> plants;
    std::vector<PopRule> pop_rules;
    EpisodeMetrics metrics{};
    LodThresholds lod_thresholds{};
    bool lod_enabled = false;
    uint32_t next_id = 1;
    Vec2 focus{};
    std::vector<std::array<float, 3>> heading_hist;
    std::vector<Action> scratch_applied;

    static Island make(uint64_t seed, Arena arena);
    static Island chase_arena(uint64_t seed, float half_extent = 48.0f);
    void seed_plants(std::size_t n);
    uint32_t spawn_agent(Species species, Vec2 pos, float heading);
    uint32_t spawn_player(PlayerArchetype archetype, Vec2 pos, float heading);
    Vec2 random_pos();
    void spawn_default_chase(std::size_t n_raptors, std::size_t n_gallis);
    void set_all_knobs(DesignerKnobs knobs);
    StepOutcome step(const std::vector<Action>& raw_actions);

    void update_focus();
    void step_lod();
    void update_heading_hist();
    void respawn_floors();
    Vec2 spawn_away_from_threats();
};

Action hold_or_zero(Action prev, bool due, Action new_action);

}  // namespace dino

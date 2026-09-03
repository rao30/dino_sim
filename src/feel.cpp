#include "dino/feel.hpp"

#include <algorithm>

namespace dino {

DinoIslandFeel::DinoIslandFeel() { reset(1, false); }

void DinoIslandFeel::reset(int64_t seed, bool naive) {
    island_ = Island::chase_arena(static_cast<uint64_t>(seed));
    island_.spawn_default_chase(3, 5);
    island_.lod_enabled = true;
    hash_ = SpatialHash::make(island_.arena.half_extent, 8.0f);
    naive_ = naive;
    actions_.assign(island_.agents.size(), Action::zero());
    paused_ = false;
}

void DinoIslandFeel::spawn_player_archetype(int32_t archetype) {
    int a = std::clamp(archetype, 0, 4);
    island_.spawn_player(player_archetype_from_u8(static_cast<uint8_t>(a)), Vec2{4.0f, 4.0f}, 0.0f);
    actions_.push_back(Action::zero());
}

void DinoIslandFeel::spawn_trike() {
    island_.spawn_agent(Species::Triceratops, Vec2{-8.0f, 6.0f}, 0.4f);
    actions_.push_back(Action::zero());
}

void DinoIslandFeel::set_raptor_knobs(float aggression, float persistence, float caution, float sociality, float hunger) {
    island_.set_all_knobs(DesignerKnobs{aggression, persistence, caution, sociality, hunger});
}

void DinoIslandFeel::physics_process() {
    if (paused_) return;
    if (naive_) {
        apply_all_scripted(island_, true);
        return;
    }
    hash_.rebuild(island_.agents);
    if (actions_.size() != island_.agents.size()) actions_.assign(island_.agents.size(), Action::zero());
    fill_scripted_actions(island_, hash_, actions_);
    island_.step(actions_);
}

void DinoIslandFeel::agent_position(int32_t index, float& x, float& y, float& z) const {
    x = y = z = 0;
    if (index < 0 || static_cast<std::size_t>(index) >= island_.agents.size()) return;
    const auto& a = island_.agents[static_cast<std::size_t>(index)];
    x = a.pos.x;
    y = 0;
    z = a.pos.z;
}

float DinoIslandFeel::agent_heading(int32_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= island_.agents.size()) return 0;
    return island_.agents[static_cast<std::size_t>(index)].heading;
}

int32_t DinoIslandFeel::agent_species(int32_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= island_.agents.size()) return -1;
    return static_cast<int32_t>(island_.agents[static_cast<std::size_t>(index)].species);
}

int32_t DinoIslandFeel::agent_lod(int32_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= island_.agents.size()) return 0;
    return static_cast<int32_t>(island_.agents[static_cast<std::size_t>(index)].lod.tier);
}

int32_t DinoIslandFeel::agent_loco_state(int32_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= island_.agents.size()) return 0;
    return static_cast<int32_t>(island_.agents[static_cast<std::size_t>(index)].loco_state);
}

float DinoIslandFeel::agent_speed(int32_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= island_.agents.size()) return 0;
    return island_.agents[static_cast<std::size_t>(index)].speed;
}

float DinoIslandFeel::agent_body_radius(int32_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= island_.agents.size()) return 0;
    return island_.agents[static_cast<std::size_t>(index)].radius();
}

bool DinoIslandFeel::agent_alive(int32_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= island_.agents.size()) return false;
    return island_.agents[static_cast<std::size_t>(index)].alive;
}

int32_t DinoIslandFeel::lod_policy_count() const {
    int32_t n = 0;
    for (const auto& a : island_.agents)
        if (a.alive && lod_uses_full_policy(a.lod.tier)) ++n;
    return n;
}

int32_t DinoIslandFeel::gru_resets_last_tick() const {
    int32_t n = 0;
    for (const auto& a : island_.agents)
        if (a.lod.reset_gru) ++n;
    return n;
}

float DinoIslandFeel::agent_energy_frac(int32_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= island_.agents.size()) return 0;
    return island_.agents[static_cast<std::size_t>(index)].energy_frac();
}

bool DinoIslandFeel::agent_feeding(int32_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= island_.agents.size()) return false;
    return island_.agents[static_cast<std::size_t>(index)].feeding;
}

float DinoIslandFeel::mean_raptor_hunger() const {
    float sum = 0;
    int n = 0;
    for (const auto& a : island_.agents) {
        if (!a.alive || a.species != Species::Utahraptor) continue;
        sum += 1.0f - a.energy_frac();
        ++n;
    }
    return n ? sum / static_cast<float>(n) : 0.0f;
}

}  // namespace dino

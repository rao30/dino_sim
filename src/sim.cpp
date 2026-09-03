#include "dino/sim.hpp"

#include <algorithm>
#include <cmath>

namespace dino {

namespace {

SpeciesStats make_stats(float mass, float max_speed, float accel, float brake, float max_turn_rate,
                        float max_health, float max_energy, float base_drain, float sprint_drain,
                        float attack_damage, float attack_range, uint32_t attack_cooldown_ticks,
                        float attack_arc, float body_radius, float fov, float view_range,
                        float hearing_range, float danger_arc_origin, float danger_arc, bool uses_pitch) {
    return SpeciesStats{mass, max_speed, accel, brake, max_turn_rate, max_health, max_energy,
                        base_drain, sprint_drain, attack_damage, attack_range, attack_cooldown_ticks,
                        attack_arc, body_radius, fov, view_range, hearing_range, danger_arc_origin,
                        danger_arc, uses_pitch};
}

const SpeciesStats kStats[SPECIES_COUNT] = {
    make_stats(8.0f, 16.0f, 28.0f, 32.0f, 3.6f, 110.0f, 100.0f, 2.0f, 8.0f, 22.0f, 2.2f, 18, 1.05f, 0.9f, 2.1f, 42.0f, 28.0f, 0.0f, 0.0f, false),
    make_stats(15.0f, 20.0f, 30.0f, 28.0f, 4.2f, 80.0f, 120.0f, 2.2f, 9.0f, 6.0f, 1.4f, 24, 0.8f, 1.1f, 3.0f, 50.0f, 22.0f, 0.0f, 0.0f, false),
    make_stats(120.0f, 8.0f, 12.0f, 18.0f, 1.15f, 420.0f, 200.0f, 3.0f, 7.0f, 70.0f, 3.5f, 36, 1.4f, 2.4f, 2.2f, 32.0f, 18.0f, 0.0f, 2.1f, false),
    make_stats(1.0f, 4.0f, 8.0f, 10.0f, 2.8f, 25.0f, 40.0f, 1.2f, 3.0f, 1.0f, 0.8f, 40, 0.6f, 0.4f, 2.6f, 16.0f, 10.0f, 0.0f, 0.0f, false),
    make_stats(2.0f, 5.0f, 9.0f, 12.0f, 2.5f, 35.0f, 50.0f, 1.3f, 3.5f, 2.0f, 0.9f, 36, 0.7f, 0.5f, 2.4f, 18.0f, 12.0f, 0.0f, 0.0f, false),
    make_stats(30.0f, 11.0f, 16.0f, 20.0f, 2.0f, 140.0f, 130.0f, 2.4f, 6.0f, 8.0f, 1.6f, 30, 0.9f, 1.5f, 3.1f, 45.0f, 24.0f, 0.0f, 0.0f, false),
    make_stats(100.0f, 7.0f, 10.0f, 16.0f, 1.0f, 380.0f, 180.0f, 2.8f, 6.5f, 55.0f, 3.2f, 32, 1.2f, 2.2f, 2.0f, 28.0f, 16.0f, PI, 2.0f, false),
    make_stats(25.0f, 13.0f, 22.0f, 24.0f, 2.4f, 120.0f, 100.0f, 2.3f, 7.0f, 28.0f, 2.0f, 28, 0.7f, 1.2f, 2.3f, 30.0f, 16.0f, 0.0f, 0.8f, false),
    make_stats(40.0f, 10.0f, 14.0f, 18.0f, 1.8f, 180.0f, 140.0f, 2.5f, 6.2f, 18.0f, 2.0f, 28, 1.0f, 1.6f, 2.8f, 38.0f, 20.0f, 0.0f, 0.0f, false),
    make_stats(1500.0f, 5.0f, 6.0f, 10.0f, 0.6f, 1200.0f, 400.0f, 4.0f, 8.0f, 40.0f, 6.0f, 48, 1.8f, 5.0f, 2.0f, 40.0f, 20.0f, PI, 1.6f, false),
    make_stats(130.0f, 6.0f, 8.0f, 14.0f, 0.9f, 500.0f, 180.0f, 2.6f, 6.0f, 60.0f, 2.8f, 40, 1.5f, 2.0f, 2.0f, 24.0f, 14.0f, PI, 1.8f, false),
    make_stats(90.0f, 9.0f, 14.0f, 18.0f, 1.6f, 320.0f, 160.0f, 2.7f, 7.0f, 50.0f, 3.0f, 26, 1.4f, 1.8f, 2.4f, 34.0f, 18.0f, 0.0f, 1.2f, false),
    make_stats(0.3f, 9.0f, 20.0f, 24.0f, 4.5f, 12.0f, 25.0f, 1.0f, 4.0f, 2.0f, 0.6f, 12, 0.9f, 0.25f, 2.8f, 14.0f, 10.0f, 0.0f, 0.0f, false),
    make_stats(3.0f, 12.0f, 22.0f, 24.0f, 3.0f, 70.0f, 80.0f, 1.8f, 6.0f, 12.0f, 8.0f, 40, 0.5f, 0.8f, 2.4f, 36.0f, 20.0f, 0.0f, 0.0f, false),
    make_stats(2.0f, 11.0f, 22.0f, 24.0f, 3.4f, 40.0f, 70.0f, 1.6f, 5.5f, 10.0f, 1.4f, 20, 1.0f, 0.6f, 2.6f, 30.0f, 26.0f, 0.0f, 0.0f, false),
    make_stats(2.0f, 22.0f, 18.0f, 16.0f, 2.2f, 35.0f, 90.0f, 2.0f, 10.0f, 6.0f, 1.2f, 20, 0.8f, 0.7f, 3.2f, 55.0f, 18.0f, 0.0f, 0.0f, true),
    make_stats(60.0f, 18.0f, 26.0f, 28.0f, 2.4f, 260.0f, 160.0f, 3.0f, 9.0f, 40.0f, 2.8f, 22, 1.0f, 1.7f, 2.2f, 40.0f, 22.0f, 0.0f, 0.0f, false),
    make_stats(120.0f, 9.0f, 14.0f, 18.0f, 1.4f, 380.0f, 180.0f, 2.4f, 6.0f, 55.0f, 3.0f, 30, 1.2f, 2.2f, 2.4f, 28.0f, 16.0f, 0.0f, 0.0f, false),
    make_stats(300.0f, 11.0f, 16.0f, 20.0f, 1.3f, 700.0f, 280.0f, 3.5f, 9.0f, 90.0f, 3.8f, 28, 1.1f, 2.8f, 2.0f, 48.0f, 24.0f, 0.0f, 0.0f, false),
    make_stats(350.0f, 10.0f, 14.0f, 18.0f, 1.2f, 720.0f, 300.0f, 3.4f, 8.5f, 80.0f, 4.0f, 30, 1.2f, 3.0f, 2.1f, 44.0f, 22.0f, 0.0f, 0.0f, false),
    make_stats(80.0f, 7.5f, 20.0f, 24.0f, 3.5f, 150.0f, 9999.0f, 0.0f, 0.0f, 18.0f, 2.0f, 16, 1.0f, 0.6f, 3.4f, 60.0f, 40.0f, 0.0f, 0.0f, false),
};

const char* kNames[SPECIES_COUNT] = {
    "utahraptor", "gallimimus", "triceratops", "dodo", "lystrosaurus", "parasaur", "stegosaurus",
    "pachycephalosaurus", "iguanodon", "brontosaurus", "ankylosaurus", "therizinosaurus",
    "compsognathus", "dilophosaurus", "troodon", "pteranodon", "carnotaurus", "sarcosuchus",
    "tyrannosaurus", "spinosaurus", "player",
};

}  // namespace

const SpeciesStats& species_stats(Species s) { return kStats[static_cast<int>(s)]; }
const char* species_name(Species s) { return kNames[static_cast<int>(s)]; }

bool is_learned_v1(Species s) {
    return s == Species::Utahraptor || s == Species::Gallimimus || s == Species::Triceratops;
}

bool is_carnivore(Species s) {
    switch (s) {
        case Species::Utahraptor:
        case Species::Compsognathus:
        case Species::Dilophosaurus:
        case Species::Troodon:
        case Species::Pteranodon:
        case Species::Carnotaurus:
        case Species::Sarcosuchus:
        case Species::Tyrannosaurus:
        case Species::Spinosaurus:
            return true;
        default:
            return false;
    }
}

bool is_prey_for_raptor(Species s) {
    return s == Species::Gallimimus || s == Species::Dodo || s == Species::Parasaur || s == Species::Lystrosaurus;
}

Action Action::saturate() const {
    Action a = *this;
    a.throttle = clamp(a.throttle, 0.0f, 1.0f);
    a.steer = clamp(a.steer, -1.0f, 1.0f);
    a.pitch = clamp(a.pitch, -1.0f, 1.0f);
    a.attack = clamp(a.attack, 0.0f, 1.0f);
    a.special = clamp(a.special, 0.0f, 1.0f);
    a.signal_intensity = clamp(a.signal_intensity, 0.0f, 1.0f);
    if (a.signal_type > SIGNAL_SOCIAL) a.signal_type = SIGNAL_NONE;
    return a;
}

Action Action::rate_limited(Action prev, Action target) {
    Action t = target.saturate();
    Action o;
    o.throttle = prev.throttle + clamp(t.throttle - prev.throttle, -MAX_THROTTLE_DELTA, MAX_THROTTLE_DELTA);
    o.steer = prev.steer + clamp(t.steer - prev.steer, -MAX_STEER_DELTA, MAX_STEER_DELTA);
    o.pitch = prev.pitch + clamp(t.pitch - prev.pitch, -MAX_PITCH_DELTA, MAX_PITCH_DELTA);
    o.attack = t.attack;
    o.special = t.special;
    o.signal_intensity = t.signal_intensity;
    o.signal_type = t.signal_type;
    return o.saturate();
}

static float lerp_range(float t, float lo, float hi) { return lo + (hi - lo) * clamp(t, 0.0f, 1.0f); }

DesignerKnobs DesignerKnobs::saturate() const {
    DesignerKnobs k = *this;
    k.aggression = clamp(k.aggression, 0.0f, 1.0f);
    k.persistence = clamp(k.persistence, 0.0f, 1.0f);
    k.caution = clamp(k.caution, 0.0f, 1.0f);
    k.sociality = clamp(k.sociality, 0.0f, 1.0f);
    k.hunger = clamp(k.hunger, 0.0f, 1.0f);
    return k;
}

std::array<float, KNOB_COUNT> DesignerKnobs::as_array() const {
    return {aggression, persistence, caution, sociality, hunger};
}

DesignerKnobs DesignerKnobs::from_array(std::array<float, KNOB_COUNT> a) {
    return DesignerKnobs{a[0], a[1], a[2], a[3], a[4]}.saturate();
}

float DesignerKnobs::hunt_energy_frac() const { return clamp(0.55f - 0.35f * hunger, 0.15f, 0.7f); }
float DesignerKnobs::engage_range(float view_range) const { return lerp_range(aggression, view_range * 0.35f, view_range * 0.95f); }
uint32_t DesignerKnobs::chase_timeout_ticks() const { return static_cast<uint32_t>(90.0f + 400.0f * persistence); }
float DesignerKnobs::pack_cohesion() const { return lerp_range(sociality, 4.0f, 18.0f); }
float DesignerKnobs::danger_respect() const { return lerp_range(caution, 0.15f, 1.0f); }

float lod_hz(LodTier t) {
    switch (t) {
        case LodTier::L0: return 15.0f;
        case LodTier::L1: return 5.0f;
        case LodTier::L2: return 2.0f;
        case LodTier::L3: return 0.2f;
    }
    return 15.0f;
}

uint32_t lod_ticks_per_decision(LodTier t) {
    switch (t) {
        case LodTier::L0: return 4;
        case LodTier::L1: return 12;
        case LodTier::L2: return 30;
        case LodTier::L3: return 300;
    }
    return 4;
}

bool lod_uses_full_policy(LodTier t) { return t == LodTier::L0 || t == LodTier::L1; }
bool lod_uses_scripted(LodTier t) { return t == LodTier::L2; }

void LodState::step(float dist_to_focus, float pos_x, float pos_z, LodThresholds th) {
    reset_gru = false;
    ticks_in_tier = sat_add(ticks_in_tier);
    ticks_since_decision = sat_add(ticks_since_decision);
    LodTier prev = tier;
    LodTier next = tier;
    switch (tier) {
        case LodTier::L0:
            if (dist_to_focus > th.exit_l0) {
                if (dist_to_focus > th.exit_l1) next = dist_to_focus > th.exit_l2 ? LodTier::L3 : LodTier::L2;
                else next = LodTier::L1;
            }
            break;
        case LodTier::L1:
            if (dist_to_focus < th.enter_l0) next = LodTier::L0;
            else if (dist_to_focus > th.exit_l1) next = dist_to_focus > th.exit_l2 ? LodTier::L3 : LodTier::L2;
            break;
        case LodTier::L2:
            if (dist_to_focus < th.enter_l1) next = dist_to_focus < th.enter_l0 ? LodTier::L0 : LodTier::L1;
            else if (dist_to_focus > th.exit_l2) next = LodTier::L3;
            break;
        case LodTier::L3:
            if (dist_to_focus < th.enter_l2) {
                if (dist_to_focus < th.enter_l1) next = dist_to_focus < th.enter_l0 ? LodTier::L0 : LodTier::L1;
                else next = LodTier::L2;
            }
            break;
    }
    if (next != prev) {
        bool promoting = lod_uses_full_policy(next) && !lod_uses_full_policy(prev);
        tier = next;
        ticks_in_tier = 0;
        ticks_since_decision = 999;
        if (promoting) reset_gru = true;
    }
    if (tier == LodTier::L3) {
        ghost_pos_x = pos_x;
        ghost_pos_z = pos_z;
    }
}

uint64_t Rng::next_u64() {
    uint64_t x = state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    state = x;
    return x;
}

Agent Agent::spawn(uint32_t id, Species species, Vec2 pos, float heading) {
    Agent a;
    a.id = id;
    a.species = species;
    a.role = species == Species::Player ? ControlRole::Player : ControlRole::Scripted;
    a.pos = pos;
    a.heading = heading;
    a.energy = species_stats(species).max_energy;
    a.health = species_stats(species).max_health;
    a.net_origin = pos;
    return a;
}

float Agent::health_frac() const {
    float m = stats().max_health;
    return m <= 1e-6f ? 0.0f : health / m;
}
float Agent::energy_frac() const {
    float m = stats().max_energy;
    return m <= 1e-6f ? 0.0f : energy / m;
}

void EpisodeMetrics::record_attack(float heading_offset) {
    attacks += 1;
    if (std::fabs(heading_offset) > PI * 0.5f) rear_arc_attacks += 1;
}

bool circling(const Agent& a) {
    return a.path_length > 40.0f && a.net_displacement() < 4.0f && a.age_ticks > 240;
}

std::size_t count_species(const std::vector<Agent>& agents, Species s) {
    std::size_t n = 0;
    for (const auto& a : agents)
        if (a.alive && a.species == s) ++n;
    return n;
}

float step_return(const Agent& agent, bool died, float /*gamma*/) {
    if (died || !agent.alive) return -1.0f;
    // Energy is the hunger meter: empty hurts, feeding is how you refill.
    float hunger = 1.0f - agent.energy_frac();
    float r = 0.01f - 0.22f * hunger * hunger + 0.08f * agent.health_frac();
    if (agent.feeding) r += 0.40f;
    return r;
}

float participation_bonus(const Agent& agent) {
    float s = 0.05f * clamp(agent.last_damage_dealt / 20.0f, 0.0f, 1.0f);
    if (agent.last_forced_heading) s += 0.03f;
    if (agent.last_blocked_escape) s += 0.03f;
    return s;
}

Action apply_envelope(Agent& agent, Action raw) {
    raw = raw.saturate();
    if (!agent.stats().uses_pitch) raw.pitch = 0.0f;
    Action applied = Action::rate_limited(agent.prev_action, raw);
    float prev_s = agent.prev_action.steer;
    float new_s = applied.steer;
    bool flip = signum(prev_s) != signum(new_s) && std::fabs(prev_s) > STEER_FLIP_EPS && std::fabs(new_s) > STEER_FLIP_EPS;
    if (flip && agent.ticks_since_steer_flip < MIN_STEER_DWELL_TICKS) {
        applied.steer = prev_s * 0.85f;
    } else if (flip) {
        agent.ticks_since_steer_flip = 0;
    } else {
        agent.ticks_since_steer_flip = sat_add(agent.ticks_since_steer_flip);
    }
    LocoState desired = loco_from_throttle(applied.throttle);
    if (desired != agent.loco_state && agent.ticks_in_loco_state < MIN_LOCO_DWELL_TICKS) {
        switch (agent.loco_state) {
            case LocoState::Idle: applied.throttle = 0.0f; break;
            case LocoState::Walk: applied.throttle = 0.4f; break;
            case LocoState::Sprint: applied.throttle = 0.85f; break;
        }
    } else if (desired != agent.loco_state) {
        agent.loco_state = desired;
        agent.ticks_in_loco_state = 0;
    } else {
        agent.ticks_in_loco_state = sat_add(agent.ticks_in_loco_state);
    }
    applied = applied.saturate();
    agent.prev_action = applied;
    return applied;
}

float heading_jerk(float h0, float h1, float h2) {
    return std::fabs(angle_diff(h2, h1) - angle_diff(h1, h0));
}

LocoStepResult step_loco(float pos_x, float pos_z, float heading, float speed, float energy, Action applied,
                         const SpeciesStats& stats) {
    float energy_frac = stats.max_energy <= 1e-6f ? 1.0f : energy / stats.max_energy;
    float speed_cap = energy_frac < 0.05f ? stats.max_speed * 0.35f : stats.max_speed;
    float desired = clamp(applied.throttle, 0.0f, 1.0f) * speed_cap;
    float speed_frac = stats.max_speed <= 1e-6f ? 0.0f : clamp(speed / stats.max_speed, 0.0f, 1.0f);
    float turn_scale = 1.0f - 0.65f * speed_frac;
    float max_turn = stats.max_turn_rate * turn_scale;
    float turn = clamp(applied.steer, -1.0f, 1.0f) * max_turn * DT;
    heading = wrap_angle(heading + turn);
    if (desired > speed) speed = std::min(speed + stats.accel * DT, desired);
    else speed = std::max(speed - stats.brake * DT, desired);
    speed = std::max(speed, 0.0f);
    Vec2 fwd = heading_vec(heading);
    pos_x += fwd.x * speed * DT;
    pos_z += fwd.z * speed * DT;
    float sprint_factor = applied.throttle > 0.7f ? (applied.throttle - 0.7f) / 0.3f : 0.0f;
    float drain = (stats.base_drain + sprint_factor * stats.sprint_drain) * DT;
    energy = std::max(energy - drain, 0.0f);
    return {pos_x, pos_z, heading, speed, energy};
}

void apply_loco(Agent& agent, Action applied) {
    if (!agent.alive) return;
    Vec2 before = agent.pos;
    auto r = step_loco(agent.pos.x, agent.pos.z, agent.heading, agent.speed, agent.energy, applied, agent.stats());
    agent.pos.x = r.pos_x;
    agent.pos.z = r.pos_z;
    agent.heading = r.heading;
    agent.speed = r.speed;
    agent.energy = r.energy;
    agent.age_ticks = sat_add(agent.age_ticks);
    float step_len = agent.pos.sub(before).length();
    agent.path_length += step_len;
    if (step_len < 0.01f) agent.ticks_motionless = sat_add(agent.ticks_motionless);
    else agent.ticks_motionless = 0;
}

void clamp_to_arena(Agent& agent, float half) {
    float lim = std::max(half - agent.radius(), 0.5f);
    agent.pos.x = clamp(agent.pos.x, -lim, lim);
    agent.pos.z = clamp(agent.pos.z, -lim, lim);
}

bool in_attack_arc(float attacker_heading, Vec2 attacker_pos, Vec2 target_pos, float arc) {
    Vec2 to = target_pos.sub(attacker_pos);
    if (to.length_sq() < 1e-8f) return true;
    return abs_angle_diff(to.heading(), attacker_heading) <= arc * 0.5f;
}

float heading_offset_on_target(float target_heading, Vec2 target_pos, Vec2 attacker_pos) {
    Vec2 to_atk = attacker_pos.sub(target_pos);
    if (to_atk.length_sq() < 1e-8f) return 0.0f;
    return wrap_angle(to_atk.heading() - target_heading);
}

bool in_danger_arc(const Agent& victim, Vec2 attacker_pos) {
    const auto& stats = victim.stats();
    if (stats.danger_arc <= 1e-4f) return false;
    float off = heading_offset_on_target(victim.heading, victim.pos, attacker_pos);
    return abs_angle_diff(off, stats.danger_arc_origin) <= stats.danger_arc * 0.5f;
}

float angular_coverage(Vec2 prey_pos, const std::vector<std::pair<Vec2, bool>>& attacker_positions, float radius) {
    static thread_local std::vector<float> angles;
    angles.clear();
    for (auto [p, alive] : attacker_positions) {
        if (!alive) continue;
        if (p.sub(prey_pos).length() > radius) continue;
        angles.push_back(p.sub(prey_pos).heading());
    }
    if (angles.size() < 2) return 0.0f;
    std::sort(angles.begin(), angles.end());
    float max_gap = 0.0f;
    std::size_t n = angles.size();
    for (std::size_t i = 0; i < n; ++i) {
        float a = angles[i];
        float b = i + 1 == n ? angles[0] + 2.0f * PI : angles[i + 1];
        max_gap = std::max(max_gap, b - a);
    }
    float covered = std::max(2.0f * PI - max_gap, 0.0f);
    return clamp(covered / (2.0f * PI), 0.0f, 1.0f);
}

float escape_probability(uint32_t n_attackers, float coverage) {
    float count_term = 1.0f / (1.0f + 0.65f * static_cast<float>(n_attackers));
    float cover_term = 1.0f - 0.75f * coverage;
    return clamp(count_term * cover_term, 0.02f, 0.95f);
}

static bool same_team(Species a, Species b) {
    if (a == b) return true;
    if (a == Species::Player || b == Species::Player) return false;
    return is_carnivore(a) == is_carnivore(b);
}

std::vector<CombatEvent> resolve_attacks(std::vector<Agent>& agents, const std::vector<Action>& actions) {
    std::size_t n = agents.size();
    static thread_local std::vector<CombatEvent> events;
    events.clear();
    for (auto& a : agents) {
        a.last_damage_dealt = 0;
        a.last_kill = false;
        a.last_forced_heading = false;
        a.last_blocked_escape = false;
        a.last_attack_heading_offset = 0;
        if (a.attack_cooldown > 0) a.attack_cooldown -= 1;
    }
    struct Pose { bool alive; Vec2 pos; float heading; Species species; float radius; };
    static thread_local std::vector<Pose> poses;
    poses.clear();
    poses.reserve(n);
    for (const auto& ag : agents) poses.push_back({ag.alive, ag.pos, ag.heading, ag.species, ag.stats().body_radius});

    for (std::size_t i = 0; i < n; ++i) {
        if (!agents[i].alive || actions[i].attack < ATTACK_THRESHOLD || agents[i].attack_cooldown > 0 || agents[i].feeding)
            continue;
        const auto& stats = agents[i].stats();
        int best_j = -1;
        float best_d = 0;
        for (std::size_t j = 0; j < n; ++j) {
            if (i == j || !poses[j].alive) continue;
            if (same_team(agents[i].species, poses[j].species)) continue;
            float dist = poses[i].pos.sub(poses[j].pos).length() - poses[i].radius - poses[j].radius;
            if (dist > stats.attack_range) continue;
            if (!in_attack_arc(poses[i].heading, poses[i].pos, poses[j].pos, stats.attack_arc)) continue;
            if (best_j < 0 || dist < best_d) {
                best_j = static_cast<int>(j);
                best_d = dist;
            }
        }
        if (best_j < 0) continue;
        std::size_t j = static_cast<std::size_t>(best_j);
        bool danger = in_danger_arc(agents[j], poses[i].pos);
        float caution = agents[i].knobs.danger_respect();
        float damage = stats.attack_damage;
        if (danger) damage *= std::max(1.0f - 0.85f * caution, 0.1f);
        float offset = heading_offset_on_target(poses[j].heading, poses[j].pos, poses[i].pos);
        float prev_heading = agents[j].heading;
        agents[j].health -= damage;
        float away = poses[j].pos.sub(poses[i].pos).heading();
        agents[j].heading = wrap_angle(agents[j].heading * 0.7f + away * 0.3f);
        bool forced = abs_angle_diff(agents[j].heading, prev_heading) > 0.08f;
        agents[i].attack_cooldown = stats.attack_cooldown_ticks;
        agents[i].last_damage_dealt = damage;
        agents[i].last_attack_heading_offset = offset;
        agents[i].last_forced_heading = forced;
        agents[i].last_blocked_escape = abs_angle_diff(offset, 0.0f) < 0.7f;
        bool kill = agents[j].health <= 0.0f;
        if (kill) {
            agents[j].alive = false;
            agents[j].health = 0;
            agents[j].speed = 0;
            agents[i].last_kill = true;
        }
        if (danger && agents[j].alive) {
            float counter = agents[j].stats().attack_damage * 0.35f * std::max(caution, 0.4f);
            agents[i].health -= counter;
            if (agents[i].health <= 0.0f) {
                agents[i].alive = false;
                agents[i].health = 0;
            }
        }
        events.push_back(CombatEvent{agents[i].id, agents[j].id, damage, offset, forced, kill});
    }
    return events;
}

Carcass spawn_carcass(const Agent& agent) {
    return Carcass{agent.pos, agent.stats().mass * CALORIES_PER_MASS, std::max(agent.radius(), 0.6f), agent.species};
}

void feed_from_carcasses(std::vector<Agent>& agents, std::vector<Carcass>& carcasses) {
    constexpr float feed_range = 2.5f;
    for (auto& c : carcasses) {
        if (c.calories <= 0.0f) continue;
        int best = -1;
        float best_mass = -1.0f;
        for (std::size_t i = 0; i < agents.size(); ++i) {
            const auto& a = agents[i];
            if (!a.alive || !is_carnivore(a.species)) continue;
            if (a.pos.sub(c.pos).length() > c.radius + feed_range) continue;
            float mass = a.stats().mass;
            if (mass > best_mass) {
                best_mass = mass;
                best = static_cast<int>(i);
            }
        }
        if (best < 0) continue;
        auto& a = agents[static_cast<std::size_t>(best)];
        float room = a.stats().max_energy - a.energy;
        if (room <= 0.5f) {
            a.feeding = false;
            continue;
        }
        float take = std::min({FEED_RATE_PER_SEC * DT, room, c.calories});
        a.energy += take;
        c.calories -= take;
        a.feeding = true;
    }
    for (auto& a : agents)
        if (!a.alive) a.feeding = false;
}

static void graze_plants(std::vector<Agent>& agents, std::vector<Plant>& plants) {
    for (auto& a : agents) {
        if (!a.alive || is_carnivore(a.species) || a.species == Species::Player) continue;
        if (a.energy_frac() > 0.9f) continue;
        for (auto& p : plants) {
            if (p.calories <= 0.0f) continue;
            if (a.pos.sub(p.pos).length() > 2.0f) continue;
            float take = std::min({8.0f * DT, p.calories, a.stats().max_energy - a.energy});
            a.energy += take;
            p.calories -= take;
            break;
        }
        for (auto& p : plants)
            if (p.calories < 40.0f) p.calories = std::min(p.calories + 0.02f, 40.0f);
    }
}

Island Island::make(uint64_t seed, Arena ar) {
    Island i;
    i.rng = Rng(seed);
    i.arena = ar;
    i.next_id = 1;
    return i;
}

Island Island::chase_arena(uint64_t seed, float half_extent) {
    float half = clamp(half_extent, 16.0f, 512.0f);
    Island island = make(seed, Arena{half});
    island.pop_rules = {
        {Species::Utahraptor, 2, 6, 3},
        {Species::Gallimimus, 3, 10, 5},
    };
    float area = (half / 48.0f) * (half / 48.0f);
    island.seed_plants(static_cast<std::size_t>(std::max(24.0f, 24.0f * area + 0.5f)));
    return island;
}

void Island::seed_plants(std::size_t n) {
    plants.clear();
    for (std::size_t i = 0; i < n; ++i) {
        float x = rng.range(-arena.half_extent * 0.85f, arena.half_extent * 0.85f);
        float z = rng.range(-arena.half_extent * 0.85f, arena.half_extent * 0.85f);
        plants.push_back(Plant{{x, z}, 40.0f});
    }
}

uint32_t Island::spawn_agent(Species species, Vec2 pos, float heading) {
    uint32_t id = next_id++;
    agents.push_back(Agent::spawn(id, species, pos, heading));
    heading_hist.push_back({heading, heading, heading});
    metrics.births_or_respawns += 1;
    return id;
}

uint32_t Island::spawn_player(PlayerArchetype archetype, Vec2 pos, float heading) {
    uint32_t id = spawn_agent(Species::Player, pos, heading);
    for (auto it = agents.rbegin(); it != agents.rend(); ++it) {
        if (it->id == id) {
            it->role = ControlRole::Player;
            it->player_archetype = archetype;
            break;
        }
    }
    return id;
}

Vec2 Island::random_pos() {
    float h = arena.half_extent * 0.8f;
    return {rng.range(-h, h), rng.range(-h, h)};
}

void Island::spawn_default_chase(std::size_t n_raptors, std::size_t n_gallis) {
    for (std::size_t i = 0; i < n_raptors; ++i)
        spawn_agent(Species::Utahraptor, random_pos(), rng.range(-PI, PI));
    for (std::size_t i = 0; i < n_gallis; ++i)
        spawn_agent(Species::Gallimimus, random_pos(), rng.range(-PI, PI));
}

void Island::set_all_knobs(DesignerKnobs knobs) {
    for (auto& a : agents)
        if (a.species == Species::Utahraptor) a.knobs = knobs;
}

void Island::update_focus() {
    for (const auto& a : agents)
        if (a.alive && a.species == Species::Player) {
            focus = a.pos;
            return;
        }
    for (const auto& a : agents)
        if (a.alive && a.species == Species::Utahraptor) {
            focus = a.pos;
            return;
        }
}

void Island::step_lod() {
    Vec2 f = focus;
    LodThresholds th = lod_thresholds;
    for (auto& a : agents) {
        if (!a.alive) continue;
        float dist = a.pos.sub(f).length();
        LodTier prev = a.lod.tier;
        a.lod.step(dist, a.pos.x, a.pos.z, th);
        if (a.lod.reset_gru && prev == LodTier::L3) {
            Vec2 dir = a.pos.sub(f);
            float len = std::max(dir.length(), 1.0f);
            float place_at = std::max(th.enter_l2, 40.0f);
            a.pos = f.add(dir.scale(place_at / len));
            clamp_to_arena(a, arena.half_extent);
        }
    }
}

void Island::update_heading_hist() {
    if (heading_hist.size() != agents.size()) heading_hist.resize(agents.size(), {0, 0, 0});
    for (std::size_t i = 0; i < agents.size(); ++i) {
        auto& h = heading_hist[i];
        h[0] = h[1];
        h[1] = h[2];
        h[2] = agents[i].heading;
        metrics.jerk_sum += heading_jerk(h[0], h[1], h[2]);
        metrics.jerk_count += 1;
    }
}

Vec2 Island::spawn_away_from_threats() {
    Vec2 best = random_pos();
    float best_score = -1e9f;
    for (int k = 0; k < 8; ++k) {
        Vec2 p = random_pos();
        float min_d = 1e9f;
        for (const auto& a : agents) {
            if (!a.alive || !is_carnivore(a.species)) continue;
            min_d = std::min(min_d, p.sub(a.pos).length());
        }
        if (min_d > best_score) {
            best_score = min_d;
            best = p;
        }
    }
    return best;
}

void Island::respawn_floors() {
    auto rules = pop_rules;
    for (const auto& rule : rules) {
        uint32_t n = static_cast<uint32_t>(count_species(agents, rule.species));
        if (n > rule.ceiling) {
            uint32_t extra = n - rule.ceiling;
            for (auto& a : agents) {
                if (extra == 0) break;
                if (a.alive && a.species == rule.species) {
                    a.alive = false;
                    extra -= 1;
                }
            }
        }
        if (n < rule.floor) {
            uint32_t need = rule.floor - n;
            for (uint32_t k = 0; k < need; ++k) {
                Vec2 pos = spawn_away_from_threats();
                float h = rng.range(-PI, PI);
                spawn_agent(rule.species, pos, h);
                metrics.immigrants += 1;
                metrics.births_or_respawns = sat_sub(metrics.births_or_respawns);
            }
        }
    }
}

StepOutcome Island::step(const std::vector<Action>& raw_actions) {
    StepOutcome out;
    if (raw_actions.size() != agents.size()) return out;
    metrics.ticks += 1;
    update_focus();
    if (lod_enabled) step_lod();
    scratch_applied.resize(agents.size());
    for (std::size_t i = 0; i < agents.size(); ++i) {
        Action act = agents[i].alive ? apply_envelope(agents[i], raw_actions[i]) : Action::zero();
        apply_loco(agents[i], act);
        clamp_to_arena(agents[i], arena.half_extent);
        scratch_applied[i] = act;
    }
    auto events = resolve_attacks(agents, scratch_applied);
    for (const auto& e : events) {
        metrics.record_attack(e.heading_offset);
        if (!e.kill) continue;
        for (const auto& v : agents) {
            if (v.id != e.victim) continue;
            if (v.species == Species::Gallimimus) {
                metrics.galli_deaths += 1;
                metrics.raptor_kills += 1;
            }
            if (v.species == Species::Player && v.player_archetype)
                metrics.player_deaths_by_archetype[static_cast<int>(*v.player_archetype)] += 1;
            break;
        }
        for (const auto& atk : agents) {
            if (atk.id != e.attacker) continue;
            if (atk.species == Species::Player && atk.player_archetype)
                metrics.player_kills_by_archetype[static_cast<int>(*atk.player_archetype)] += 1;
            break;
        }
        if (e.kill) out.raptor_kills_this_tick += 1;
    }
    for (const auto& e : events)
        if (e.kill)
            for (const auto& v : agents)
                if (v.id == e.victim) {
                    carcasses.push_back(spawn_carcass(v));
                    break;
                }
    for (auto& a : agents) {
        if (a.alive && a.energy <= 0.0f && a.species != Species::Player) {
            a.alive = false;
            a.health = 0;
            carcasses.push_back(spawn_carcass(a));
        }
    }
    feed_from_carcasses(agents, carcasses);
    graze_plants(agents, plants);
    for (auto& c : carcasses) c.calories = std::max(c.calories - CARCASS_DECAY_PER_SEC * DT, 0.0f);
    carcasses.erase(std::remove_if(carcasses.begin(), carcasses.end(), [](const Carcass& c) { return c.calories <= 1.0f; }),
                    carcasses.end());
    update_heading_hist();
    for (const auto& a : agents) {
        if (circling(a)) metrics.circling_flags += 1;
        if (a.alive && a.ticks_motionless > 180) metrics.motionless_flags += 1;
    }
    respawn_floors();
    tick += 1;
    out.applied = scratch_applied;
    return out;
}

Action hold_or_zero(Action prev, bool due, Action new_action) { return due ? new_action : prev; }

}  // namespace dino

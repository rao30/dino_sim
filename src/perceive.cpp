#include "dino/perceive.hpp"

#include <algorithm>
#include <cmath>

namespace dino {

SpatialHash SpatialHash::make(float half_extent, float cell) {
    SpatialHash h;
    h.cell = cell;
    float span = std::max(half_extent * 2.0f + cell * 2.0f, cell);
    h.cols = static_cast<int>(std::ceil(span / cell)) + 2;
    h.rows = h.cols;
    h.origin = {-half_extent - cell, -half_extent - cell};
    h.buckets.assign(static_cast<std::size_t>(h.cols * h.rows), {});
    return h;
}

void SpatialHash::clear() {
    for (auto& b : buckets) b.clear();
}

void SpatialHash::rebuild(const std::vector<Agent>& agents) {
    clear();
    for (std::size_t i = 0; i < agents.size(); ++i) {
        if (!agents[i].alive) continue;
        int cx = static_cast<int>(std::floor((agents[i].pos.x - origin.x) / cell));
        int cz = static_cast<int>(std::floor((agents[i].pos.z - origin.z) / cell));
        if (cx < 0 || cz < 0 || cx >= cols || cz >= rows) continue;
        buckets[static_cast<std::size_t>(cz * cols + cx)].push_back(i);
    }
}

void SpatialHash::query(Vec2 pos, float radius, std::vector<std::size_t>& out) const {
    out.clear();
    float r = std::max(radius, 0.0f);
    int cx0 = static_cast<int>(std::floor((pos.x - r - origin.x) / cell));
    int cx1 = static_cast<int>(std::floor((pos.x + r - origin.x) / cell));
    int cz0 = static_cast<int>(std::floor((pos.z - r - origin.z) / cell));
    int cz1 = static_cast<int>(std::floor((pos.z + r - origin.z) / cell));
    for (int cz = cz0; cz <= cz1; ++cz)
        for (int cx = cx0; cx <= cx1; ++cx) {
            if (cx < 0 || cz < 0 || cx >= cols || cz >= rows) continue;
            const auto& b = buckets[static_cast<std::size_t>(cz * cols + cx)];
            out.insert(out.end(), b.begin(), b.end());
        }
}

static bool in_fov(float heading, Vec2 pos, Vec2 other, float fov) {
    Vec2 to = other.sub(pos);
    if (to.length_sq() < 1e-8f) return true;
    return abs_angle_diff(to.heading(), heading) <= fov * 0.5f;
}

static EntityFeat pack_entity(const Agent& viewer, const Agent& other) {
    Vec2 rel = other.pos.sub(viewer.pos);
    Vec2 rv = other.vel().sub(viewer.vel());
    EntityFeat f;
    f.rel_x = rel.x;
    f.rel_z = rel.z;
    f.rel_vx = rv.x;
    f.rel_vz = rv.z;
    f.species = static_cast<uint8_t>(other.species);
    f.radius = other.radius();
    f.health = other.health_frac();
    f.signal_type = other.prev_action.signal_type;
    f.signal_int = other.prev_action.signal_intensity;
    f.in_danger_arc = in_danger_arc(other, viewer.pos) ? 1.0f : 0.0f;
    return f;
}

static uint8_t take_k(std::vector<std::pair<float, EntityFeat>>& scored, std::size_t k, EntityFeat* dst) {
    if (scored.size() > k) {
        std::partial_sort(scored.begin(), scored.begin() + static_cast<std::ptrdiff_t>(k), scored.end(),
                          [](auto& a, auto& b) { return a.first < b.first; });
    } else {
        std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.first < b.first; });
    }
    std::size_t n = std::min(scored.size(), k);
    for (std::size_t i = 0; i < n; ++i) dst[i] = scored[i].second;
    for (std::size_t i = n; i < k; ++i) dst[i] = {};
    return static_cast<uint8_t>(n);
}

Observation observe_agent(const Agent& viewer, const std::vector<Agent>& agents, const std::vector<Plant>& plants,
                          const std::vector<Carcass>& carcasses, const SpatialHash& hash,
                          std::vector<std::size_t>& scratch, float time_of_day) {
    Observation obs{};
    if (!viewer.alive) return obs;
    const auto& stats = viewer.stats();
    auto kn = viewer.knobs.as_array();
    obs.self_vec[0] = viewer.energy_frac();
    obs.self_vec[1] = viewer.health_frac();
    obs.self_vec[2] = viewer.speed / std::max(stats.max_speed, 1.0f);
    obs.self_vec[3] = std::cos(viewer.heading);
    obs.self_vec[4] = std::sin(viewer.heading);
    obs.self_vec[5] = viewer.pitch;
    obs.self_vec[6] = std::min(viewer.age_ticks / 10000.0f, 1.0f);
    obs.self_vec[7] = time_of_day;
    obs.self_vec[8] = static_cast<float>(static_cast<uint8_t>(viewer.species));
    obs.self_vec[9] = kn[0];
    obs.self_vec[10] = kn[1];
    obs.self_vec[11] = kn[2];
    obs.self_vec[12] = kn[3];
    obs.self_vec[13] = kn[4];
    obs.self_vec[14] = static_cast<float>(static_cast<uint8_t>(viewer.lod.tier));
    obs.self_vec[15] = viewer.feeding ? 1.0f : 0.0f;
    obs.self_vec[16] = viewer.attack_cooldown / 40.0f;

    hash.query(viewer.pos, stats.view_range, scratch);
    static thread_local std::vector<std::pair<float, EntityFeat>> threats, cons, other, resources;
    threats.clear();
    cons.clear();
    other.clear();
    resources.clear();
    const float range_sq = stats.view_range * stats.view_range;
    for (std::size_t i : scratch) {
        const Agent& o = agents[i];
        if (o.id == viewer.id || !o.alive) continue;
        float dist_sq = o.pos.sub(viewer.pos).length_sq();
        if (dist_sq > range_sq) continue;
        if (!in_fov(viewer.heading, viewer.pos, o.pos, stats.fov)) continue;
        float dist = std::sqrt(dist_sq);
        EntityFeat feat = pack_entity(viewer, o);
        bool hostile = is_carnivore(viewer.species) != is_carnivore(o.species) || o.species == Species::Player ||
                       viewer.species == Species::Player;
        if (o.species == viewer.species) cons.push_back({dist, feat});
        else if (hostile)
            threats.push_back({dist, feat});
        else
            other.push_back({dist, feat});
    }
    if (is_carnivore(viewer.species)) {
        for (const auto& c : carcasses) {
            float dist = c.pos.sub(viewer.pos).length();
            if (dist > stats.view_range) continue;
            EntityFeat f{};
            f.rel_x = c.pos.x - viewer.pos.x;
            f.rel_z = c.pos.z - viewer.pos.z;
            f.radius = c.radius;
            f.health = std::min(c.calories / 200.0f, 1.0f);
            resources.push_back({dist, f});
        }
    } else {
        float plant_range = stats.view_range * 0.6f;
        for (const auto& p : plants) {
            float dist = p.pos.sub(viewer.pos).length();
            if (dist > plant_range) continue;
            EntityFeat f{};
            f.rel_x = p.pos.x - viewer.pos.x;
            f.rel_z = p.pos.z - viewer.pos.z;
            f.radius = 0.8f;
            f.health = std::min(p.calories / 40.0f, 1.0f);
            resources.push_back({dist, f});
        }
    }
    obs.n_threats = take_k(threats, K_THREATS, obs.threats);
    obs.n_conspecifics = take_k(cons, K_CONSPECIFICS, obs.conspecifics);
    obs.n_resources = take_k(resources, K_RESOURCES, obs.resources);
    obs.n_other = take_k(other, K_OTHER, obs.other);
    return obs;
}

static void pack_entity_feat(const EntityFeat& f, float* out) {
    out[0] = f.rel_x;
    out[1] = f.rel_z;
    out[2] = f.rel_vx;
    out[3] = f.rel_vz;
    out[4] = static_cast<float>(f.species);
    out[5] = f.radius;
    out[6] = f.health;
    out[7] = static_cast<float>(f.signal_type);
    out[8] = f.signal_int;
    out[9] = f.in_danger_arc;
}

void pack_observation(const Observation& obs, float* out) {
    for (std::size_t i = 0; i < OBS_SELF; ++i) out[i] = obs.self_vec[i];
    float* p = out + OBS_SELF;
    for (std::size_t i = 0; i < K_THREATS; ++i, p += ENT_FEAT) pack_entity_feat(obs.threats[i], p);
    for (std::size_t i = 0; i < K_CONSPECIFICS; ++i, p += ENT_FEAT) pack_entity_feat(obs.conspecifics[i], p);
    for (std::size_t i = 0; i < K_RESOURCES; ++i, p += ENT_FEAT) pack_entity_feat(obs.resources[i], p);
    for (std::size_t i = 0; i < K_OTHER; ++i, p += ENT_FEAT) pack_entity_feat(obs.other[i], p);
}

const Agent* nearest_prey(const Agent& viewer, const std::vector<Agent>& agents) {
    const Agent* best = nullptr;
    float best_d = 0;
    for (const auto& o : agents) {
        if (o.id == viewer.id || !o.alive) continue;
        bool is_prey = false;
        if (viewer.species == Species::Utahraptor)
            is_prey = is_prey_for_raptor(o.species) || o.species == Species::Player;
        else if (viewer.species == Species::Triceratops)
            is_prey = is_carnivore(o.species);
        if (!is_prey) continue;
        float d = o.pos.sub(viewer.pos).length();
        if (!best || d < best_d) {
            best = &o;
            best_d = d;
        }
    }
    return best;
}

}  // namespace dino

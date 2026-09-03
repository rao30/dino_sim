#pragma once

#include "dino/sim.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace dino {

constexpr std::size_t K_THREATS = 12;
constexpr std::size_t K_CONSPECIFICS = 12;
constexpr std::size_t K_RESOURCES = 6;
constexpr std::size_t K_OTHER = 6;
constexpr std::size_t OBS_SELF = 32;
constexpr std::size_t ENT_FEAT = 10;
constexpr std::size_t OBS_DIM = OBS_SELF + (K_THREATS + K_CONSPECIFICS + K_RESOURCES + K_OTHER) * ENT_FEAT;
constexpr std::size_t ACT_DIM = 7;
constexpr float HASH_CELL = 8.0f;

struct SpatialHash {
    float cell = HASH_CELL;
    Vec2 origin{};
    int cols = 0;
    int rows = 0;
    std::vector<std::vector<std::size_t>> buckets;

    static SpatialHash make(float half_extent, float cell = HASH_CELL);
    void clear();
    void rebuild(const std::vector<Agent>& agents);
    void query(Vec2 pos, float radius, std::vector<std::size_t>& out) const;
};

struct EntityFeat {
    float rel_x = 0, rel_z = 0, rel_vx = 0, rel_vz = 0;
    uint8_t species = 0;
    float radius = 0, health = 0;
    uint8_t signal_type = 0;
    float signal_int = 0, in_danger_arc = 0;
};

struct Observation {
    float self_vec[32]{};
    EntityFeat threats[K_THREATS]{};
    EntityFeat conspecifics[K_CONSPECIFICS]{};
    EntityFeat resources[K_RESOURCES]{};
    EntityFeat other[K_OTHER]{};
    uint8_t n_threats = 0, n_conspecifics = 0, n_resources = 0, n_other = 0;
};

Observation observe_agent(const Agent& viewer, const std::vector<Agent>& agents,
                          const std::vector<Plant>& plants, const std::vector<Carcass>& carcasses,
                          const SpatialHash& hash, std::vector<std::size_t>& scratch, float time_of_day);

void pack_observation(const Observation& obs, float* out);

const Agent* nearest_prey(const Agent& viewer, const std::vector<Agent>& agents);

}  // namespace dino

#include "dino_island_node.hpp"

using namespace godot;

void DinoIslandNode::_bind_methods() {
    ClassDB::bind_method(D_METHOD("reset", "seed", "naive"), &DinoIslandNode::reset);
    ClassDB::bind_method(D_METHOD("spawn_player_archetype", "archetype"), &DinoIslandNode::spawn_player_archetype);
    ClassDB::bind_method(D_METHOD("spawn_trike"), &DinoIslandNode::spawn_trike);
    ClassDB::bind_method(D_METHOD("set_raptor_knobs", "aggression", "persistence", "caution", "sociality", "hunger"),
                        &DinoIslandNode::set_raptor_knobs);
    ClassDB::bind_method(D_METHOD("agent_count"), &DinoIslandNode::agent_count);
    ClassDB::bind_method(D_METHOD("agent_position", "index"), &DinoIslandNode::agent_position);
    ClassDB::bind_method(D_METHOD("agent_heading", "index"), &DinoIslandNode::agent_heading);
    ClassDB::bind_method(D_METHOD("agent_species", "index"), &DinoIslandNode::agent_species);
    ClassDB::bind_method(D_METHOD("agent_lod", "index"), &DinoIslandNode::agent_lod);
    ClassDB::bind_method(D_METHOD("raptor_kills"), &DinoIslandNode::raptor_kills);
    ClassDB::bind_method(D_METHOD("rear_arc_fraction"), &DinoIslandNode::rear_arc_fraction);
    ClassDB::bind_method(D_METHOD("immigrant_rate"), &DinoIslandNode::immigrant_rate);
    ClassDB::bind_method(D_METHOD("set_paused", "paused"), &DinoIslandNode::set_paused);
    ClassDB::bind_method(D_METHOD("set_naive", "naive"), &DinoIslandNode::set_naive);
    ClassDB::bind_method(D_METHOD("is_naive"), &DinoIslandNode::is_naive);
    ClassDB::bind_method(D_METHOD("enable_lod", "enabled"), &DinoIslandNode::enable_lod);
    ClassDB::bind_method(D_METHOD("lod_policy_count"), &DinoIslandNode::lod_policy_count);
    ClassDB::bind_method(D_METHOD("arena_half_extent"), &DinoIslandNode::arena_half_extent);
    ClassDB::bind_method(D_METHOD("gru_resets_last_tick"), &DinoIslandNode::gru_resets_last_tick);
    ClassDB::bind_method(D_METHOD("lod_l0_hz"), &DinoIslandNode::lod_l0_hz);
    ClassDB::bind_method(D_METHOD("agent_energy_frac", "index"), &DinoIslandNode::agent_energy_frac);
    ClassDB::bind_method(D_METHOD("agent_feeding", "index"), &DinoIslandNode::agent_feeding);
    ClassDB::bind_method(D_METHOD("mean_raptor_hunger"), &DinoIslandNode::mean_raptor_hunger);
}

void DinoIslandNode::_physics_process(double) { feel_.physics_process(); }
void DinoIslandNode::reset(int64_t seed, bool naive) { feel_.reset(seed, naive); }
void DinoIslandNode::spawn_player_archetype(int32_t archetype) { feel_.spawn_player_archetype(archetype); }
void DinoIslandNode::spawn_trike() { feel_.spawn_trike(); }
void DinoIslandNode::set_raptor_knobs(float a, float p, float c, float s, float h) {
    feel_.set_raptor_knobs(a, p, c, s, h);
}
int32_t DinoIslandNode::agent_count() const { return feel_.agent_count(); }
Vector3 DinoIslandNode::agent_position(int32_t index) const {
    float x, y, z;
    feel_.agent_position(index, x, y, z);
    return Vector3(x, y, z);
}
float DinoIslandNode::agent_heading(int32_t index) const { return feel_.agent_heading(index); }
int32_t DinoIslandNode::agent_species(int32_t index) const { return feel_.agent_species(index); }
int32_t DinoIslandNode::agent_lod(int32_t index) const { return feel_.agent_lod(index); }
int32_t DinoIslandNode::raptor_kills() const { return feel_.raptor_kills(); }
float DinoIslandNode::rear_arc_fraction() const { return feel_.rear_arc_fraction(); }
float DinoIslandNode::immigrant_rate() const { return feel_.immigrant_rate(); }
void DinoIslandNode::set_paused(bool paused) { feel_.set_paused(paused); }
void DinoIslandNode::set_naive(bool naive) { feel_.set_naive(naive); }
bool DinoIslandNode::is_naive() const { return feel_.naive(); }
void DinoIslandNode::enable_lod(bool enabled) { feel_.enable_lod(enabled); }
int32_t DinoIslandNode::lod_policy_count() const { return feel_.lod_policy_count(); }
float DinoIslandNode::arena_half_extent() const { return feel_.arena_half_extent(); }
int32_t DinoIslandNode::gru_resets_last_tick() const { return feel_.gru_resets_last_tick(); }
float DinoIslandNode::lod_l0_hz() const { return feel_.lod_l0_hz(); }
float DinoIslandNode::agent_energy_frac(int32_t index) const { return feel_.agent_energy_frac(index); }
bool DinoIslandNode::agent_feeding(int32_t index) const { return feel_.agent_feeding(index); }
float DinoIslandNode::mean_raptor_hunger() const { return feel_.mean_raptor_hunger(); }

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef DINO_C_EXPORTS
#define DINO_C_API __declspec(dllexport)
#else
#define DINO_C_API __declspec(dllimport)
#endif
#else
#define DINO_C_API
#endif

DINO_C_API void* dino_island_create(uint64_t seed);
DINO_C_API void dino_island_destroy(void* handle);

DINO_C_API void dino_island_spawn_chase(void* handle, uint32_t n_raptors, uint32_t n_gallis, int scripted_raptors);
DINO_C_API void dino_island_spawn_chase_ex(void* handle, uint32_t n_raptors, uint32_t n_gallis, int scripted_raptors,
                                           float half_extent);
DINO_C_API uint32_t dino_island_spawn_player(void* handle, int archetype);

DINO_C_API void dino_island_set_knobs(void* handle, const float* knobs5);
DINO_C_API void dino_island_set_role_learned(void* handle, int32_t agent_index, int learned);

DINO_C_API int32_t dino_island_agent_count(void* handle);
DINO_C_API int32_t dino_island_species(void* handle, int32_t agent_index);
DINO_C_API int dino_island_alive(void* handle, int32_t agent_index);
DINO_C_API int32_t dino_island_role(void* handle, int32_t agent_index);
DINO_C_API uint32_t dino_island_id(void* handle, int32_t agent_index);

DINO_C_API int32_t dino_obs_dim(void);
DINO_C_API int32_t dino_act_dim(void);

DINO_C_API int dino_island_observe(void* handle, int32_t agent_index, float* out392);
DINO_C_API void dino_island_step(void* handle, const float* actions_n_times_7);
DINO_C_API float dino_island_last_reward(void* handle, int32_t agent_index);
DINO_C_API int dino_island_gru_reset(void* handle, int32_t agent_index);

DINO_C_API void dino_island_metrics(void* handle, uint32_t* raptor_kills, uint32_t* attacks, uint32_t* rear_arc_attacks,
                                    uint32_t* immigrants, uint32_t* births_or_respawns, uint32_t* ticks,
                                    float* rear_arc_fraction, uint32_t* player_kills, uint32_t* player_deaths);
DINO_C_API int32_t dino_island_player_archetype(void* handle);
DINO_C_API int dino_island_pos(void* handle, int32_t agent_index, float* xyzh);

/* Parallel multi-world stepper. actions/obs/rew/done are dense [n_worlds, n_raptors, ...]. */
DINO_C_API void* dino_batch_create(uint64_t seed, int32_t n_worlds, uint32_t n_raptors, uint32_t n_gallis, int player,
                                   int scripted_raptors, float half_extent);
DINO_C_API void dino_batch_destroy(void* batch);
DINO_C_API void dino_batch_set_knobs(void* batch, const float* knobs5);
DINO_C_API void dino_batch_observe(void* batch, float* out_obs_w_r_392, float* out_teacher_w_r_7);
DINO_C_API void dino_batch_step(void* batch, const float* actions_w_r_7, float* out_obs_w_r_392, float* out_rew_w_r,
                               uint8_t* out_done_w_r, float* out_teacher_w_r_7);
/* Per world: kills, attacks, rear, imm, respawn, player_kills, player_deaths, player_archetype, ticks */
DINO_C_API void dino_batch_metrics(void* batch, uint32_t* out_w_9);
DINO_C_API int32_t dino_batch_agent_count(void* batch, int32_t world);
DINO_C_API int dino_batch_pos(void* batch, int32_t world, int32_t index, float* xyzh);
DINO_C_API void dino_batch_teacher_actions(void* batch, float* out_w_r_7);
DINO_C_API void dino_batch_reset(void* batch, uint64_t seed);

#ifdef __cplusplus
}
#endif

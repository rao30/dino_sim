#include "dino/c_api.h"

#include "dino.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <new>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct DinoIsland {
    uint64_t seed = 1;
    dino::Island island;
    dino::SpatialHash hash;
    std::vector<std::size_t> scratch;
    std::vector<float> last_reward;
    std::vector<uint8_t> gru_reset;
    bool learned_raptors = true;
    int player_archetype = -1;
    struct Snap {
        uint32_t id = 0;
        bool alive = false;
    };
    std::vector<Snap> before;
    std::vector<dino::Action> acts;
    std::vector<float> action_buf;
    std::vector<int32_t> pre_idx;
    std::vector<std::pair<uint32_t, int32_t>> living;
    std::vector<char> used;
};

struct DinoBatch {
    std::vector<DinoIsland> worlds;
    uint32_t n_raptors = 0;
    uint32_t n_gallis = 0;
    int player = 0;
    int scripted_raptors = 0;
    float half_extent = 48.0f;
    std::vector<std::vector<uint32_t>> slots;
};

DinoIsland* as_island(void* handle) { return static_cast<DinoIsland*>(handle); }
DinoBatch* as_batch(void* handle) { return static_cast<DinoBatch*>(handle); }

bool valid_index(const DinoIsland* h, int32_t index) {
    return h && index >= 0 && static_cast<std::size_t>(index) < h->island.agents.size();
}

void resize_scratch(DinoIsland* h) {
    h->last_reward.assign(h->island.agents.size(), 0.0f);
    h->gru_reset.assign(h->island.agents.size(), 0);
}

void rebuild_hash(DinoIsland* h) {
    if (h->hash.cols == 0) h->hash = dino::SpatialHash::make(h->island.arena.half_extent, dino::HASH_CELL);
    h->hash.rebuild(h->island.agents);
}

void mark_learned_raptors(DinoIsland* h) {
    if (!h->learned_raptors) return;
    for (auto& a : h->island.agents)
        if (a.species == dino::Species::Utahraptor) a.role = dino::ControlRole::Learned;
}

float time_of_day(const dino::Island& island) {
    return static_cast<float>(island.tick % 7200u) / 7200.0f;
}

dino::Action action_from_floats(const float* p) {
    dino::Action a;
    a.throttle = p[0];
    a.steer = p[1];
    a.pitch = p[2];
    a.attack = p[3];
    a.special = p[4];
    a.signal_intensity = p[5];
    float st = p[6];
    if (st < 0.0f) st = 0.0f;
    a.signal_type = static_cast<uint8_t>(st + 0.5f);
    return a;
}

void action_to_floats(const dino::Action& a, float* p) {
    p[0] = a.throttle;
    p[1] = a.steer;
    p[2] = a.pitch;
    p[3] = a.attack;
    p[4] = a.special;
    p[5] = a.signal_intensity;
    p[6] = static_cast<float>(a.signal_type);
}

bool use_provided_action(const DinoIsland* h, const dino::Agent& a) {
    if (a.role == dino::ControlRole::Learned) return true;
    if (h->learned_raptors && a.species == dino::Species::Utahraptor) return true;
    return false;
}

void configure_chase(DinoIsland* h, uint32_t n_raptors, uint32_t n_gallis, int scripted_raptors, float half_extent) {
    h->learned_raptors = scripted_raptors == 0;
    h->player_archetype = -1;
    h->island = dino::Island::chase_arena(h->seed, half_extent);
    for (auto& rule : h->island.pop_rules) {
        if (rule.species == dino::Species::Utahraptor) {
            rule.floor = n_raptors;
            rule.target = n_raptors;
            rule.ceiling = n_raptors;
        } else if (rule.species == dino::Species::Gallimimus) {
            rule.floor = n_gallis;
            rule.target = n_gallis;
            rule.ceiling = n_gallis;
        }
    }
    h->island.spawn_default_chase(n_raptors, n_gallis);
    mark_learned_raptors(h);
    h->hash = dino::SpatialHash::make(h->island.arena.half_extent, dino::HASH_CELL);
    rebuild_hash(h);
    resize_scratch(h);
}

void observe_index(DinoIsland* h, int32_t index, float* out392, bool rebuild) {
    if (!out392) return;
    if (!valid_index(h, index) || !h->island.agents[static_cast<std::size_t>(index)].alive) {
        std::memset(out392, 0, dino::OBS_DIM * sizeof(float));
        return;
    }
    if (rebuild) rebuild_hash(h);
    const dino::Agent& viewer = h->island.agents[static_cast<std::size_t>(index)];
    dino::Observation obs =
        dino::observe_agent(viewer, h->island.agents, h->island.plants, h->island.carcasses, h->hash, h->scratch,
                            time_of_day(h->island));
    dino::pack_observation(obs, out392);
}

void step_island(DinoIsland* h, const float* actions_n_times_7) {
    const std::size_t n = h->island.agents.size();
    h->before.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        h->before[i].id = h->island.agents[i].id;
        h->before[i].alive = h->island.agents[i].alive;
    }
    h->acts.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const dino::Agent& a = h->island.agents[i];
        if (!a.alive) {
            h->acts[i] = dino::Action::zero();
            continue;
        }
        if (actions_n_times_7 && use_provided_action(h, a))
            h->acts[i] = action_from_floats(actions_n_times_7 + i * dino::ACT_DIM);
        else
            h->acts[i] = dino::policy_for(h->island, i, h->hash);
    }
    h->island.step(h->acts);
    dino::apply_player_ranged(h->island);
    mark_learned_raptors(h);

    const std::size_t n_after = h->island.agents.size();
    h->last_reward.assign(n_after, 0.0f);
    h->gru_reset.assign(n_after, 0);
    for (std::size_t i = 0; i < n; ++i) {
        const dino::Agent& a = h->island.agents[i];
        const bool died = h->before[i].alive && !a.alive;
        const bool replaced = a.id != h->before[i].id;
        if (h->before[i].alive) {
            float r = dino::step_return(a, died, 0.99f) + dino::participation_bonus(a);
            r += 0.45f * dino::clamp(a.last_damage_dealt / 22.0f, 0.0f, 2.0f);
            if (a.last_kill) r += 1.5f;
            if (a.species == dino::Species::Utahraptor) {
                const dino::Agent* prey = dino::nearest_prey(a, h->island.agents);
                if (prey) {
                    float dist = prey->pos.sub(a.pos).length();
                    r += 0.012f * std::exp(-dist / 10.0f);
                }
            }
            h->last_reward[i] = r;
        }
        h->gru_reset[i] = (died || replaced || a.lod.reset_gru) ? 1 : 0;
    }
    for (std::size_t i = n; i < n_after; ++i) h->gru_reset[i] = 1;
}

int32_t find_id(const DinoIsland* h, uint32_t agent_id) {
    if (!h || agent_id == 0) return -1;
    for (std::size_t i = 0; i < h->island.agents.size(); ++i)
        if (h->island.agents[i].id == agent_id) return static_cast<int32_t>(i);
    return -1;
}

void bind_slots(DinoIsland* h, std::vector<uint32_t>& slots) {
    auto& living = h->living;
    living.clear();
    for (std::size_t i = 0; i < h->island.agents.size(); ++i) {
        const auto& a = h->island.agents[i];
        if (!a.alive || a.species != dino::Species::Utahraptor) continue;
        living.push_back({a.id, static_cast<int32_t>(i)});
    }
    std::sort(living.begin(), living.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    auto& used = h->used;
    used.assign(living.size(), 0);
    auto mark_used = [&](uint32_t id) {
        for (std::size_t i = 0; i < living.size(); ++i)
            if (living[i].first == id) used[i] = 1;
    };
    for (uint32_t& sid : slots) {
        bool ok = false;
        for (const auto& item : living)
            if (item.first == sid) {
                ok = true;
                break;
            }
        if (sid && !ok) sid = 0;
        else if (sid)
            mark_used(sid);
    }
    std::size_t ui = 0;
    for (uint32_t& sid : slots) {
        if (sid != 0) continue;
        while (ui < living.size() && used[ui]) ++ui;
        if (ui < living.size()) {
            sid = living[ui].first;
            used[ui] = 1;
            ++ui;
        }
    }
}

struct IJob {
    virtual ~IJob() = default;
    virtual void run(int i) = 0;
};

struct ThreadPool {
    explicit ThreadPool(int nthreads) : nthreads_(std::max(nthreads, 1)) {
        workers_.reserve(static_cast<std::size_t>(nthreads_));
        for (int t = 0; t < nthreads_; ++t) workers_.emplace_back([this] { worker(); });
    }

    void run(int n, IJob* job) {
        if (n <= 0) return;
        if (n == 1) {
            job->run(0);
            return;
        }
        next_.store(0, std::memory_order_relaxed);
        job_ = job;
        n_ = n;
        remaining_.store(nthreads_, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lk(mu_);
            ++generation_;
        }
        cv_start_.notify_all();
        drain(job, n);
        std::unique_lock<std::mutex> lk(mu_);
        cv_done_.wait(lk, [&] { return remaining_.load(std::memory_order_acquire) == 0; });
        job_ = nullptr;
    }

private:
    void drain(IJob* job, int n) {
        for (;;) {
            int i = next_.fetch_add(1, std::memory_order_relaxed);
            if (i >= n) break;
            job->run(i);
        }
    }

    void worker() {
        unsigned seen = 0;
        for (;;) {
            std::unique_lock<std::mutex> lk(mu_);
            cv_start_.wait(lk, [&] { return stop_ || generation_ != seen; });
            if (stop_) return;
            seen = generation_;
            IJob* job = job_;
            int n = n_;
            lk.unlock();
            drain(job, n);
            if (remaining_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                std::lock_guard<std::mutex> g(mu_);
                cv_done_.notify_one();
            }
        }
    }

    std::vector<std::thread> workers_;
    std::mutex mu_;
    std::condition_variable cv_start_;
    std::condition_variable cv_done_;
    std::atomic<int> next_{0};
    std::atomic<int> remaining_{0};
    IJob* job_ = nullptr;
    int n_ = 0;
    int nthreads_ = 1;
    unsigned generation_ = 0;
    bool stop_ = false;
};

ThreadPool& thread_pool() {
    unsigned hw = std::thread::hardware_concurrency();
    int n = static_cast<int>(hw ? hw : 4u);
    static ThreadPool* pool = new ThreadPool(n);
    return *pool;
}

template <typename Fn>
void parallel_for(int n, Fn&& fn) {
    if (n <= 0) return;
    if (n == 1) {
        fn(0);
        return;
    }
    struct Wrap : IJob {
        Fn* f;
        explicit Wrap(Fn* fn) : f(fn) {}
        void run(int i) override { (*f)(i); }
    } job{&fn};
    thread_pool().run(n, &job);
}

void write_world_metrics(const DinoIsland& h, uint32_t* out9) {
    const dino::EpisodeMetrics& m = h.island.metrics;
    out9[0] = m.raptor_kills;
    out9[1] = m.attacks;
    out9[2] = m.rear_arc_attacks;
    out9[3] = m.immigrants;
    out9[4] = m.births_or_respawns;
    uint32_t pk = 0, pd = 0;
    for (int i = 0; i < 5; ++i) {
        pk += m.player_kills_by_archetype[i];
        pd += m.player_deaths_by_archetype[i];
    }
    out9[5] = pk;
    out9[6] = pd;
    out9[7] = static_cast<uint32_t>(h.player_archetype < 0 ? 0u : static_cast<uint32_t>(h.player_archetype));
    if (h.player_archetype < 0) out9[7] = 0xFFFFFFFFu;
    out9[8] = m.ticks;
}

void write_teacher(DinoIsland* h, const std::vector<uint32_t>& slots, float* dest) {
    const std::size_t n_raptors = slots.size();
    std::memset(dest, 0, n_raptors * dino::ACT_DIM * sizeof(float));
    for (std::size_t r = 0; r < n_raptors; ++r) {
        int32_t idx = find_id(h, slots[r]);
        if (idx < 0) continue;
        if (!h->island.agents[static_cast<std::size_t>(idx)].alive) continue;
        dino::Action a = dino::policy_for(h->island, static_cast<std::size_t>(idx), h->hash);
        action_to_floats(a, dest + r * dino::ACT_DIM);
    }
}

void batch_observe_world(DinoIsland* h, std::vector<uint32_t>& slots, float* out_obs, float* out_teacher) {
    bind_slots(h, slots);
    rebuild_hash(h);
    for (std::size_t r = 0; r < slots.size(); ++r) {
        float* dest = out_obs + r * dino::OBS_DIM;
        int32_t idx = find_id(h, slots[r]);
        observe_index(h, idx, dest, false);
    }
    if (out_teacher) write_teacher(h, slots, out_teacher);
}

void step_world_raptors(DinoIsland* h, std::vector<uint32_t>& slots, const float* raptor_acts, float* out_obs,
                        float* out_rew, uint8_t* out_done, float* out_teacher) {
    const uint32_t n_raptors = static_cast<uint32_t>(slots.size());
    bind_slots(h, slots);
    const std::size_t n = h->island.agents.size();
    h->action_buf.assign(n * dino::ACT_DIM, 0.0f);
    h->pre_idx.assign(n_raptors, -1);
    for (uint32_t r = 0; r < n_raptors; ++r) {
        int32_t idx = find_id(h, slots[r]);
        h->pre_idx[r] = idx;
        if (idx >= 0)
            std::memcpy(h->action_buf.data() + static_cast<std::size_t>(idx) * dino::ACT_DIM,
                        raptor_acts + r * dino::ACT_DIM, dino::ACT_DIM * sizeof(float));
    }
    step_island(h, h->action_buf.data());
    for (uint32_t r = 0; r < n_raptors; ++r) {
        int32_t idx = h->pre_idx[r];
        if (idx < 0) {
            out_rew[r] = 0.0f;
            out_done[r] = 1;
            continue;
        }
        out_rew[r] = (static_cast<std::size_t>(idx) < h->last_reward.size()) ? h->last_reward[static_cast<std::size_t>(idx)] : 0.0f;
        bool dead = static_cast<std::size_t>(idx) >= h->island.agents.size() || !h->island.agents[static_cast<std::size_t>(idx)].alive;
        bool reset = static_cast<std::size_t>(idx) < h->gru_reset.size() && h->gru_reset[static_cast<std::size_t>(idx)];
        out_done[r] = (dead || reset) ? 1 : 0;
    }
    bind_slots(h, slots);
    rebuild_hash(h);
    for (uint32_t r = 0; r < n_raptors; ++r) {
        int32_t idx = find_id(h, slots[r]);
        if (idx < 0) out_done[r] = 1;
        observe_index(h, idx, out_obs + r * dino::OBS_DIM, false);
    }
    if (out_teacher) write_teacher(h, slots, out_teacher);
}

}  // namespace

extern "C" {

DINO_C_API void* dino_island_create(uint64_t seed) {
    auto* h = new (std::nothrow) DinoIsland();
    if (!h) return nullptr;
    h->seed = seed;
    return h;
}

DINO_C_API void dino_island_destroy(void* handle) { delete as_island(handle); }

DINO_C_API void dino_island_spawn_chase(void* handle, uint32_t n_raptors, uint32_t n_gallis, int scripted_raptors) {
    dino_island_spawn_chase_ex(handle, n_raptors, n_gallis, scripted_raptors, 48.0f);
}

DINO_C_API void dino_island_spawn_chase_ex(void* handle, uint32_t n_raptors, uint32_t n_gallis, int scripted_raptors,
                                           float half_extent) {
    DinoIsland* h = as_island(handle);
    if (!h) return;
    configure_chase(h, n_raptors, n_gallis, scripted_raptors, half_extent);
}

DINO_C_API uint32_t dino_island_spawn_player(void* handle, int archetype) {
    DinoIsland* h = as_island(handle);
    if (!h) return 0;
    int a = std::clamp(archetype, 0, 4);
    h->player_archetype = a;
    uint32_t id = h->island.spawn_player(dino::player_archetype_from_u8(static_cast<uint8_t>(a)), dino::Vec2{5.0f, 5.0f}, 0.0f);
    rebuild_hash(h);
    resize_scratch(h);
    return id;
}

DINO_C_API void dino_island_set_knobs(void* handle, const float* knobs5) {
    DinoIsland* h = as_island(handle);
    if (!h || !knobs5) return;
    h->island.set_all_knobs(dino::DesignerKnobs{knobs5[0], knobs5[1], knobs5[2], knobs5[3], knobs5[4]}.saturate());
}

DINO_C_API void dino_island_set_role_learned(void* handle, int32_t agent_index, int learned) {
    DinoIsland* h = as_island(handle);
    if (!valid_index(h, agent_index)) return;
    auto& agent = h->island.agents[static_cast<std::size_t>(agent_index)];
    if (agent.role == dino::ControlRole::Player) return;
    agent.role = learned ? dino::ControlRole::Learned : dino::ControlRole::Scripted;
}

DINO_C_API int32_t dino_island_agent_count(void* handle) {
    DinoIsland* h = as_island(handle);
    return h ? static_cast<int32_t>(h->island.agents.size()) : 0;
}

DINO_C_API int32_t dino_island_species(void* handle, int32_t agent_index) {
    DinoIsland* h = as_island(handle);
    if (!valid_index(h, agent_index)) return -1;
    return static_cast<int32_t>(h->island.agents[static_cast<std::size_t>(agent_index)].species);
}

DINO_C_API int dino_island_alive(void* handle, int32_t agent_index) {
    DinoIsland* h = as_island(handle);
    if (!valid_index(h, agent_index)) return 0;
    return h->island.agents[static_cast<std::size_t>(agent_index)].alive ? 1 : 0;
}

DINO_C_API int32_t dino_island_role(void* handle, int32_t agent_index) {
    DinoIsland* h = as_island(handle);
    if (!valid_index(h, agent_index)) return -1;
    return static_cast<int32_t>(h->island.agents[static_cast<std::size_t>(agent_index)].role);
}

DINO_C_API uint32_t dino_island_id(void* handle, int32_t agent_index) {
    DinoIsland* h = as_island(handle);
    if (!valid_index(h, agent_index)) return 0;
    return h->island.agents[static_cast<std::size_t>(agent_index)].id;
}

DINO_C_API int32_t dino_obs_dim(void) { return static_cast<int32_t>(dino::OBS_DIM); }
DINO_C_API int32_t dino_act_dim(void) { return static_cast<int32_t>(dino::ACT_DIM); }

DINO_C_API int dino_island_observe(void* handle, int32_t agent_index, float* out392) {
    DinoIsland* h = as_island(handle);
    if (!h || !out392) return -1;
    if (!valid_index(h, agent_index)) return -1;
    observe_index(h, agent_index, out392, true);
    return 0;
}

DINO_C_API void dino_island_step(void* handle, const float* actions_n_times_7) {
    DinoIsland* h = as_island(handle);
    if (!h) return;
    step_island(h, actions_n_times_7);
}

DINO_C_API float dino_island_last_reward(void* handle, int32_t agent_index) {
    DinoIsland* h = as_island(handle);
    if (!h || agent_index < 0 || static_cast<std::size_t>(agent_index) >= h->last_reward.size()) return 0.0f;
    return h->last_reward[static_cast<std::size_t>(agent_index)];
}

DINO_C_API int dino_island_gru_reset(void* handle, int32_t agent_index) {
    DinoIsland* h = as_island(handle);
    if (!h || agent_index < 0 || static_cast<std::size_t>(agent_index) >= h->gru_reset.size()) return 0;
    return h->gru_reset[static_cast<std::size_t>(agent_index)] ? 1 : 0;
}

DINO_C_API void dino_island_metrics(void* handle, uint32_t* raptor_kills, uint32_t* attacks, uint32_t* rear_arc_attacks,
                                    uint32_t* immigrants, uint32_t* births_or_respawns, uint32_t* ticks,
                                    float* rear_arc_fraction, uint32_t* player_kills, uint32_t* player_deaths) {
    DinoIsland* h = as_island(handle);
    if (!h) return;
    uint32_t tmp[9];
    write_world_metrics(*h, tmp);
    if (raptor_kills) *raptor_kills = tmp[0];
    if (attacks) *attacks = tmp[1];
    if (rear_arc_attacks) *rear_arc_attacks = tmp[2];
    if (immigrants) *immigrants = tmp[3];
    if (births_or_respawns) *births_or_respawns = tmp[4];
    if (player_kills) *player_kills = tmp[5];
    if (player_deaths) *player_deaths = tmp[6];
    if (ticks) *ticks = tmp[8];
    if (rear_arc_fraction) *rear_arc_fraction = h->island.metrics.rear_arc_fraction();
}

DINO_C_API int32_t dino_island_player_archetype(void* handle) {
    DinoIsland* h = as_island(handle);
    return h ? static_cast<int32_t>(h->player_archetype) : -1;
}

DINO_C_API int dino_island_pos(void* handle, int32_t agent_index, float* xyzh) {
    DinoIsland* h = as_island(handle);
    if (!valid_index(h, agent_index) || !xyzh) return -1;
    const dino::Agent& a = h->island.agents[static_cast<std::size_t>(agent_index)];
    xyzh[0] = a.pos.x;
    xyzh[1] = 0.0f;
    xyzh[2] = a.pos.z;
    xyzh[3] = a.heading;
    return 0;
}

void spawn_world(DinoBatch* b, int32_t w, uint64_t seed) {
    DinoIsland& h = b->worlds[static_cast<std::size_t>(w)];
    h.seed = seed;
    configure_chase(&h, b->n_raptors, b->n_gallis, b->scripted_raptors, b->half_extent);
    if (b->player) {
        int arch = w % 5;
        h.player_archetype = arch;
        h.island.spawn_player(dino::player_archetype_from_u8(static_cast<uint8_t>(arch)), dino::Vec2{5.0f, 5.0f}, 0.0f);
        mark_learned_raptors(&h);
        rebuild_hash(&h);
        resize_scratch(&h);
    }
    b->slots[static_cast<std::size_t>(w)].assign(b->n_raptors, 0);
    bind_slots(&h, b->slots[static_cast<std::size_t>(w)]);
}

DINO_C_API void* dino_batch_create(uint64_t seed, int32_t n_worlds, uint32_t n_raptors, uint32_t n_gallis, int player,
                                   int scripted_raptors, float half_extent) {
    if (n_worlds <= 0 || n_raptors == 0) return nullptr;
    auto* b = new (std::nothrow) DinoBatch();
    if (!b) return nullptr;
    b->n_raptors = n_raptors;
    b->n_gallis = n_gallis;
    b->player = player ? 1 : 0;
    b->scripted_raptors = scripted_raptors ? 1 : 0;
    b->half_extent = half_extent;
    b->worlds.resize(static_cast<std::size_t>(n_worlds));
    b->slots.resize(static_cast<std::size_t>(n_worlds));
    for (int32_t w = 0; w < n_worlds; ++w) spawn_world(b, w, seed + static_cast<uint64_t>(w));
    return b;
}

DINO_C_API void dino_batch_destroy(void* batch) { delete as_batch(batch); }

DINO_C_API void dino_batch_set_knobs(void* batch, const float* knobs5) {
    DinoBatch* b = as_batch(batch);
    if (!b || !knobs5) return;
    auto kn = dino::DesignerKnobs{knobs5[0], knobs5[1], knobs5[2], knobs5[3], knobs5[4]}.saturate();
    for (auto& h : b->worlds) h.island.set_all_knobs(kn);
}

DINO_C_API void dino_batch_observe(void* batch, float* out_obs_w_r_392, float* out_teacher_w_r_7) {
    DinoBatch* b = as_batch(batch);
    if (!b || !out_obs_w_r_392) return;
    const int n = static_cast<int>(b->worlds.size());
    const std::size_t stride = static_cast<std::size_t>(b->n_raptors) * dino::OBS_DIM;
    const std::size_t act_stride = static_cast<std::size_t>(b->n_raptors) * dino::ACT_DIM;
    parallel_for(n, [&](int w) {
        const std::size_t ww = static_cast<std::size_t>(w);
        float* teacher = out_teacher_w_r_7 ? out_teacher_w_r_7 + ww * act_stride : nullptr;
        batch_observe_world(&b->worlds[ww], b->slots[ww], out_obs_w_r_392 + ww * stride, teacher);
    });
}

DINO_C_API void dino_batch_step(void* batch, const float* actions_w_r_7, float* out_obs_w_r_392, float* out_rew_w_r,
                               uint8_t* out_done_w_r, float* out_teacher_w_r_7) {
    DinoBatch* b = as_batch(batch);
    if (!b || !actions_w_r_7 || !out_obs_w_r_392 || !out_rew_w_r || !out_done_w_r) return;
    const int n = static_cast<int>(b->worlds.size());
    const std::size_t act_stride = static_cast<std::size_t>(b->n_raptors) * dino::ACT_DIM;
    const std::size_t obs_stride = static_cast<std::size_t>(b->n_raptors) * dino::OBS_DIM;
    parallel_for(n, [&](int w) {
        const std::size_t ww = static_cast<std::size_t>(w);
        float* teacher = out_teacher_w_r_7 ? out_teacher_w_r_7 + ww * act_stride : nullptr;
        step_world_raptors(&b->worlds[ww], b->slots[ww], actions_w_r_7 + ww * act_stride, out_obs_w_r_392 + ww * obs_stride,
                           out_rew_w_r + ww * b->n_raptors, out_done_w_r + ww * b->n_raptors, teacher);
    });
}

DINO_C_API void dino_batch_metrics(void* batch, uint32_t* out_w_9) {
    DinoBatch* b = as_batch(batch);
    if (!b || !out_w_9) return;
    for (std::size_t w = 0; w < b->worlds.size(); ++w) write_world_metrics(b->worlds[w], out_w_9 + w * 9);
}

DINO_C_API int32_t dino_batch_agent_count(void* batch, int32_t world) {
    DinoBatch* b = as_batch(batch);
    if (!b || world < 0 || static_cast<std::size_t>(world) >= b->worlds.size()) return 0;
    return static_cast<int32_t>(b->worlds[static_cast<std::size_t>(world)].island.agents.size());
}

DINO_C_API int dino_batch_pos(void* batch, int32_t world, int32_t index, float* xyzh) {
    DinoBatch* b = as_batch(batch);
    if (!b || !xyzh || world < 0 || static_cast<std::size_t>(world) >= b->worlds.size()) return -1;
    return dino_island_pos(&b->worlds[static_cast<std::size_t>(world)], index, xyzh);
}

DINO_C_API void dino_batch_teacher_actions(void* batch, float* out_w_r_7) {
    DinoBatch* b = as_batch(batch);
    if (!b || !out_w_r_7) return;
    const int n = static_cast<int>(b->worlds.size());
    const std::size_t act_stride = static_cast<std::size_t>(b->n_raptors) * dino::ACT_DIM;
    parallel_for(n, [&](int w) {
        const std::size_t ww = static_cast<std::size_t>(w);
        DinoIsland* h = &b->worlds[ww];
        std::vector<uint32_t>& slots = b->slots[ww];
        bind_slots(h, slots);
        float* dest = out_w_r_7 + ww * act_stride;
        write_teacher(h, slots, dest);
    });
}

DINO_C_API void dino_batch_reset(void* batch, uint64_t seed) {
    DinoBatch* b = as_batch(batch);
    if (!b) return;
    const int n = static_cast<int>(b->worlds.size());
    parallel_for(n, [&](int w) { spawn_world(b, w, seed + static_cast<uint64_t>(w)); });
}

}  // extern "C"

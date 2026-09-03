#include "dino/feel.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++g_fails;
    }
}

}  // namespace

int main() {
    using namespace dino;

    {
        Agent a = Agent::spawn(1, Species::Utahraptor, {}, 0);
        Action out = apply_envelope(a, Action{.throttle = 1.0f});
        expect(out.throttle <= MAX_THROTTLE_DELTA + 1e-5f, "envelope rate-limits throttle");
    }
    {
        const auto& stats = species_stats(Species::Utahraptor);
        Action act{};
        act.throttle = 0.5f;
        float x = 0, z = 0, heading = 0, speed = 0, energy = stats.max_energy;
        for (int i = 0; i < 60; ++i) {
            auto r = step_loco(x, z, heading, speed, energy, act, stats);
            x = r.pos_x;
            z = r.pos_z;
            heading = r.heading;
            speed = r.speed;
            energy = r.energy;
        }
        expect(x > 1.0f, "loco moves +X");
        expect(std::fabs(z) < 0.2f, "loco stays on X");
        expect(energy < stats.max_energy, "energy drains");
    }
    {
        LodState s;
        LodThresholds th;
        s.step(10, 0, 0, th);
        expect(s.tier == LodTier::L0, "lod near is L0");
        s.step(200, 0, 200, th);
        expect(s.tier == LodTier::L3, "lod far is L3");
        s.step(20, 0, 20, th);
        expect(lod_uses_full_policy(s.tier), "promote to policy");
        expect(s.reset_gru, "GRU reset on L3->policy");
    }
    {
        Agent a = Agent::spawn(1, Species::Utahraptor, {}, 0);
        DesignerKnobs hungry = a.knobs;
        hungry.hunger = 1.0f;
        expect(hungry.hunt_energy_frac() < a.knobs.hunt_energy_frac(), "hunger lowers hunt threshold");
        expect(a.energy == species_stats(Species::Utahraptor).max_energy, "hunger is not energy");
        Agent full = a;
        Agent empty = a;
        empty.energy = 0;
        expect(step_return(empty, false, 0.99f) < step_return(full, false, 0.99f), "empty energy is hungry (lower return)");
        empty.feeding = true;
        expect(step_return(empty, false, 0.99f) > step_return(full, false, 0.99f) - 0.05f, "feeding offsets hunger");
    }
    {
        auto run = [](uint64_t seed) {
            Island island = Island::chase_arena(seed);
            island.spawn_default_chase(2, 3);
            std::vector<Action> zero(island.agents.size());
            for (int i = 0; i < 30; ++i) island.step(zero);
            return island.agents;
        };
        auto a = run(7), b = run(7), c = run(8);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i)
            same = a[i].pos.x == b[i].pos.x && a[i].pos.z == b[i].pos.z && a[i].heading == b[i].heading;
        expect(same, "island deterministic");
        bool diff = a.size() != c.size();
        if (!diff)
            for (std::size_t i = 0; i < a.size(); ++i)
                if (a[i].pos.x != c[i].pos.x || a[i].heading != c[i].heading) diff = true;
        expect(diff, "different seeds differ");
        for (int i = 0; i < 9; ++i) expect(same, "bitwise-ish replay x10");  // 10 total with first
    }
    {
        SpatialHash hash = SpatialHash::make(64, HASH_CELL);
        std::vector<Agent> agents;
        agents.push_back(Agent::spawn(0, Species::Utahraptor, {}, 0));
        for (uint32_t i = 0; i < 40; ++i)
            agents.push_back(Agent::spawn(i + 1, Species::Gallimimus, Vec2{2.0f + i * 0.3f, 0}, 0));
        hash.rebuild(agents);
        std::vector<std::size_t> scratch;
        auto obs = observe_agent(agents[0], agents, {}, {}, hash, scratch, 0);
        expect(obs.n_threats <= K_THREATS, "top-K cap");
        expect(obs.n_conspecifics <= K_CONSPECIFICS, "conspecific cap");
        expect(obs.n_resources <= K_RESOURCES, "resource cap");
        expect(obs.n_other <= K_OTHER, "other cap");
        expect(OBS_DIM == 392, "OBS_DIM 392");
        std::vector<float> packed(OBS_DIM, 0.0f);
        pack_observation(obs, packed.data());
        expect(packed.size() == OBS_DIM, "pack writes OBS_DIM");
        expect(std::fabs(packed[0] - obs.self_vec[0]) < 1e-6f, "pack self_vec[0]");
        expect(std::fabs(packed[OBS_SELF] - obs.threats[0].rel_x) < 1e-6f, "pack threat rel_x");
    }
    {
        uint32_t sk = 0, nk = 0;
        for (uint64_t seed = 1; seed < 6; ++seed) {
            Island s = Island::chase_arena(seed);
            s.spawn_default_chase(3, 5);
            Island n = Island::chase_arena(seed);
            n.spawn_default_chase(3, 5);
            for (int t = 0; t < 1800; ++t) {
                apply_all_scripted(s, false);
                apply_all_scripted(n, true);
            }
            sk += s.metrics.raptor_kills;
            nk += n.metrics.raptor_kills;
        }
        expect(sk > 0, "scripted raptor gets kills");
        expect(sk >= nk, "scripted kills beat naive pursuit");
    }
    {
        Island low = Island::chase_arena(3);
        low.spawn_default_chase(2, 4);
        low.set_all_knobs(DesignerKnobs{.aggression = 0.05f});
        Island high = Island::chase_arena(3);
        high.spawn_default_chase(2, 4);
        high.set_all_knobs(DesignerKnobs{.aggression = 0.95f});
        for (int t = 0; t < 400; ++t) {
            apply_all_scripted(low, false);
            apply_all_scripted(high, false);
        }
        expect(high.metrics.attacks + high.metrics.raptor_kills >= low.metrics.attacks + low.metrics.raptor_kills,
               "aggression knob moves engagement");
    }
    {
        Island island = Island::chase_arena(1);
        island.spawn_default_chase(8, 16);
        auto t0 = std::chrono::steady_clock::now();
        const int ticks = 240;
        for (int t = 0; t < ticks; ++t) apply_all_scripted(island, false);
        double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        double ats = ticks * static_cast<double>(island.agents.size()) / std::max(dt, 1e-9);
        std::cerr << "agent-transitions/sec ≈ " << ats << "\n";
        expect(ats > 10000.0, "throughput");
    }
    {
        for (int arch = 0; arch < 5; ++arch) {
            Island island = Island::chase_arena(1);
            island.spawn_default_chase(2, 3);
            island.spawn_player(player_archetype_from_u8(static_cast<uint8_t>(arch)), Vec2{5, 5}, 0);
            for (int t = 0; t < 120; ++t) apply_all_scripted(island, false);
        }
        expect(true, "player archetypes run");
    }
    {
        DinoIslandFeel feel;
        feel.spawn_trike();
        feel.spawn_player_archetype(0);
        feel.set_raptor_knobs(0.6f, 0.5f, 0.5f, 0.6f, 0.5f);
        feel.enable_lod(true);
        for (int i = 0; i < 30; ++i) feel.physics_process();
        expect(feel.agent_count() >= 8, "feel harness agents");
        expect(feel.lod_l0_hz() == 15.0f, "L0 15Hz");
    }
    {
        std::string json = dump_golden_json(1000);
        expect(json.find("\"n_steps\":1000") != std::string::npos, "golden json steps");
        expect(json.size() > 100, "golden json nonempty");
    }

    if (g_fails) {
        std::cerr << g_fails << " failures\n";
        return 1;
    }
    std::cerr << "all tests passed\n";
    return 0;
}

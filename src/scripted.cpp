#include "dino/scripted.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dino {

static Action steer_toward(const Agent& agent, Vec2 target, float throttle, bool attack) {
    Vec2 to = target.sub(agent.pos);
    float dist = to.length();
    float desired = dist < 1e-3f ? agent.heading : to.heading();
    float err = angle_diff(desired, agent.heading);
    float steer = clamp(err / 0.7f, -1.0f, 1.0f);
    Action a = Action::zero();
    a.throttle = throttle;
    a.steer = steer;
    a.attack = attack ? 1.0f : 0.0f;
    return a;
}

static float largest_gap_dir(Vec2 pos, const std::vector<Vec2>& threats) {
    if (threats.empty()) return 0.0f;
    static thread_local std::vector<float> angs;
    angs.clear();
    angs.reserve(threats.size());
    for (auto t : threats) angs.push_back(t.sub(pos).heading());
    std::sort(angs.begin(), angs.end());
    float best_gap = 0, best_mid = angs[0] + PI;
    std::size_t n = angs.size();
    for (std::size_t i = 0; i < n; ++i) {
        float a = angs[i];
        float b = i + 1 == n ? angs[0] + 2.0f * PI : angs[i + 1];
        float gap = b - a;
        if (gap > best_gap) {
            best_gap = gap;
            best_mid = a + gap * 0.5f;
        }
    }
    return wrap_angle(best_mid);
}

static const Carcass* nearest_carcass(const Agent& agent, const std::vector<Carcass>& carcasses) {
    const Carcass* best = nullptr;
    float bd = 0;
    for (const auto& c : carcasses) {
        if (c.calories <= 1.0f) continue;
        float d = c.pos.sub(agent.pos).length_sq();
        if (!best || d < bd) {
            best = &c;
            bd = d;
        }
    }
    return best;
}

static Action raptor_flank(const Island& island, std::size_t index) {
    const Agent& agent = island.agents[index];
    DesignerKnobs kn = agent.knobs;
    const auto& stats = agent.stats();
    const Agent* prey = nearest_prey(agent, island.agents);
    if (!prey) {
        if (agent.energy_frac() < kn.hunt_energy_frac()) {
            if (const Carcass* meal = nearest_carcass(agent, island.carcasses))
                return steer_toward(agent, meal->pos, 0.55f, false);
        }
        Action a = Action::zero();
        a.throttle = 0.2f;
        a.steer = 0.1f;
        a.signal_type = SIGNAL_CONTACT;
        a.signal_intensity = 0.3f;
        return a;
    }
    float dist = prey->pos.sub(agent.pos).length();
    float offset = heading_offset_on_target(prey->heading, prey->pos, agent.pos);
    bool rear_ok = std::fabs(offset) > PI * 0.45f;
    bool dangerous = prey->stats().danger_arc > 0.1f;
    float t_lead = 0.0f;
    if (!dangerous && prey->speed > 12.0f && dist > 14.0f)
        t_lead = clamp(dist / std::max(stats.max_speed, 1.0f), 0.0f, 0.4f);
    Vec2 target = prey->pos.add(prey->vel().scale(t_lead));
    if (dangerous && !rear_ok) {
        float rear = wrap_angle(prey->heading + PI);
        target = prey->pos.add(heading_vec(rear).scale(4.0f + 3.0f * kn.caution));
    }
    if (in_danger_arc(*prey, agent.pos) && kn.danger_respect() > 0.4f && dangerous) {
        target = prey->pos.add(heading_vec(wrap_angle(prey->heading + PI)).scale(6.0f));
    }
    float throttle = clamp(0.45f + 1.1f * kn.aggression, 0.45f, 1.0f);
    if (dist > kn.engage_range(stats.view_range) && kn.aggression < 0.35f) throttle = 0.4f;
    float bite_range = stats.attack_range + agent.radius() + prey->radius() + 0.6f;
    bool bite = dist < bite_range && (!dangerous || rear_ok || dist < stats.attack_range);
    Action act = steer_toward(agent, target, throttle, bite);
    std::size_t pack_n = 0;
    for (const auto& a : island.agents)
        if (a.alive && a.species == Species::Utahraptor && a.pos.sub(prey->pos).length() < 40.0f) ++pack_n;
    if (pack_n >= 2) {
        act.signal_type = SIGNAL_THREAT;
        act.signal_intensity = 0.6f;
    }
    return act;
}

static Action galli_flee(const Island& island, std::size_t index) {
    const Agent& agent = island.agents[index];
    static thread_local std::vector<Vec2> threats;
    threats.clear();
    const float range_sq = agent.stats().view_range * agent.stats().view_range;
    for (const auto& o : island.agents) {
        if (!o.alive || o.id == agent.id) continue;
        if (!is_carnivore(o.species) && o.species != Species::Player) continue;
        if (o.pos.sub(agent.pos).length_sq() < range_sq) threats.push_back(o.pos);
    }
    if (threats.empty()) {
        if (!island.plants.empty()) {
            const Plant* best = nullptr;
            float bd = 0;
            for (const auto& p : island.plants) {
                float d = p.pos.sub(agent.pos).length_sq();
                if (!best || d < bd) {
                    best = &p;
                    bd = d;
                }
            }
            if (best) return steer_toward(agent, best->pos, 0.28f, false);
        }
        Action a = Action::zero();
        a.throttle = 0.15f;
        return a;
    }
    float cx = 0, cz = 0;
    for (auto t : threats) {
        cx += t.x;
        cz += t.z;
    }
    Vec2 centroid{cx / static_cast<float>(threats.size()), cz / static_cast<float>(threats.size())};
    Vec2 away = agent.pos.sub(centroid);
    static thread_local std::vector<std::pair<Vec2, bool>> cov;
    cov.clear();
    cov.reserve(threats.size());
    for (auto t : threats) cov.push_back({t, true});
    float cover = angular_coverage(agent.pos, cov, 20.0f);
    float p_esc = escape_probability(static_cast<uint32_t>(threats.size()), cover);
    float flee_dir = cover > 0.45f ? largest_gap_dir(agent.pos, threats)
                                   : (away.length() > 1e-3f ? away.heading() : agent.heading);
    Vec2 target = agent.pos.add(Vec2{std::cos(flee_dir), std::sin(flee_dir)}.scale(20.0f));
    Action act = steer_toward(agent, target, p_esc < 0.4f ? 1.0f : 0.75f, false);
    act.signal_type = SIGNAL_ALARM;
    act.signal_intensity = 0.8f;
    return act;
}

static Action trike_defend(const Island& island, std::size_t index) {
    const Agent& agent = island.agents[index];
    const Agent* nearest = nullptr;
    float bd = 0;
    for (const auto& o : island.agents) {
        if (!o.alive || !is_carnivore(o.species)) continue;
        float d = o.pos.sub(agent.pos).length();
        if (!nearest || d < bd) {
            nearest = &o;
            bd = d;
        }
    }
    if (!nearest) {
        Action a = Action::zero();
        a.throttle = 0.15f;
        return a;
    }
    if (bd > 18.0f) {
        Action a = Action::zero();
        a.throttle = 0.2f;
        return a;
    }
    Action act = steer_toward(agent, nearest->pos, 0.7f, bd < 6.0f);
    if (bd < 8.0f) act.attack = 1.0f;
    return act;
}

static Action stub_wander(const Island& island, std::size_t index) {
    const Agent& agent = island.agents[index];
    float t = (static_cast<float>(island.tick) * 0.01f + static_cast<float>(agent.id)) * 0.7f;
    Action a = Action::zero();
    a.throttle = 0.2f;
    a.steer = std::sin(t) * 0.4f;
    return a;
}

static Action player_policy(const Island& island, std::size_t index) {
    const Agent& agent = island.agents[index];
    PlayerArchetype arch = agent.player_archetype.value_or(PlayerArchetype::Idle);
    const Agent* r = nullptr;
    float bd = 0;
    for (const auto& a : island.agents) {
        if (!a.alive || a.species != Species::Utahraptor) continue;
        float d = a.pos.sub(agent.pos).length_sq();
        if (!r || d < bd) {
            r = &a;
            bd = d;
        }
    }
    if (!r) return Action::zero();
    float dist = r->pos.sub(agent.pos).length();
    switch (arch) {
        case PlayerArchetype::Idle:
            return Action::zero();
        case PlayerArchetype::Tank: {
            Action a = steer_toward(agent, r->pos, 0.55f, dist < 3.0f);
            a.attack = dist < 3.2f ? 1.0f : 0.0f;
            return a;
        }
        case PlayerArchetype::Kiter: {
            float desired = 16.0f;
            if (dist < desired - 2.0f) {
                Vec2 t = agent.pos.add(agent.pos.sub(r->pos).normalized().scale(12.0f));
                Action a = steer_toward(agent, t, 0.7f, false);
                a.special = 1.0f;
                return a;
            }
            if (dist > desired + 4.0f) {
                Action a = steer_toward(agent, r->pos, 0.45f, false);
                a.special = 1.0f;
                return a;
            }
            Action a = Action::zero();
            a.special = 1.0f;
            a.throttle = 0.15f;
            return a;
        }
        case PlayerArchetype::Fleer:
            return steer_toward(agent, Vec2{island.arena.half_extent * 0.9f, island.arena.half_extent * 0.9f}, 1.0f, false);
        case PlayerArchetype::Puller:
            if ((island.tick / 90) % 2 == 0) return steer_toward(agent, r->pos, 0.6f, dist < 4.0f);
            return steer_toward(agent, agent.pos.add(agent.pos.sub(r->pos).normalized().scale(20.0f)), 0.8f, false);
    }
    return Action::zero();
}

Action policy_for(const Island& island, std::size_t index, const SpatialHash&) {
    const Agent& agent = island.agents[index];
    if (!agent.alive) return Action::zero();
    if (agent.role == ControlRole::Player) return player_policy(island, index);
    switch (agent.species) {
        case Species::Utahraptor: return raptor_flank(island, index);
        case Species::Gallimimus: return galli_flee(island, index);
        case Species::Triceratops:
        case Species::Stegosaurus: return trike_defend(island, index);
        case Species::Player: return player_policy(island, index);
        default: return stub_wander(island, index);
    }
}

void fill_scripted_actions(const Island& island, const SpatialHash& hash, std::vector<Action>& out) {
    out.resize(island.agents.size());
    for (std::size_t i = 0; i < island.agents.size(); ++i) out[i] = policy_for(island, i, hash);
}

Action naive_pursuit(const Island& island, std::size_t index) {
    const Agent& agent = island.agents[index];
    if (!agent.alive) return Action::zero();
    if (agent.species != Species::Utahraptor) {
        SpatialHash h = SpatialHash::make(island.arena.half_extent, 8.0f);
        return policy_for(island, index, h);
    }
    const Agent* prey = nearest_prey(agent, island.agents);
    if (!prey) {
        Action a = Action::zero();
        a.throttle = 0.25f;
        a.steer = 0.15f;
        return a;
    }
    float dist = prey->pos.sub(agent.pos).length();
    float bite_range = agent.stats().attack_range + agent.radius() + prey->radius() + 0.6f;
    return steer_toward(agent, prey->pos, 1.0f, dist < bite_range);
}

void apply_player_ranged(Island& island) {
    uint64_t tick = island.tick;
    std::vector<std::size_t> victims;
    for (std::size_t i = 0; i < island.agents.size(); ++i) {
        const auto& a = island.agents[i];
        if (!a.alive || a.species != Species::Player) continue;
        if (a.player_archetype != PlayerArchetype::Kiter) continue;
        if (a.prev_action.special < 0.5f) continue;
        if (tick % 20 != 0) continue;
        int best = -1;
        float bd = 0;
        for (std::size_t j = 0; j < island.agents.size(); ++j) {
            const auto& o = island.agents[j];
            if (!o.alive || o.species != Species::Utahraptor) continue;
            float d = o.pos.sub(a.pos).length_sq();
            if (best < 0 || d < bd) {
                best = static_cast<int>(j);
                bd = d;
            }
        }
        if (best >= 0 && island.agents[static_cast<std::size_t>(best)].pos.sub(a.pos).length() < 25.0f)
            victims.push_back(static_cast<std::size_t>(best));
    }
    for (std::size_t j : victims) {
        auto& r = island.agents[j];
        r.health -= 8.0f;
        if (r.health <= 0.0f) {
            r.alive = false;
            r.health = 0;
        }
    }
}

void apply_all_scripted(Island& island, bool naive_raptors) {
    SpatialHash hash = SpatialHash::make(island.arena.half_extent, 8.0f);
    hash.rebuild(island.agents);
    std::vector<Action> acts(island.agents.size());
    for (std::size_t i = 0; i < island.agents.size(); ++i) {
        if (naive_raptors && island.agents[i].species == Species::Utahraptor) acts[i] = naive_pursuit(island, i);
        else acts[i] = policy_for(island, i, hash);
    }
    island.step(acts);
    apply_player_ranged(island);
}

}  // namespace dino

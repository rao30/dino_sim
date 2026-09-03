#include "dino/feel.hpp"

#include <iomanip>
#include <limits>
#include <fstream>
#include <sstream>
#include <vector>

namespace dino {

static void append_action(std::ostringstream& o, const Action& a) {
    o << "{\"throttle\":" << a.throttle << ",\"steer\":" << a.steer << ",\"pitch\":" << a.pitch
      << ",\"attack\":" << a.attack << ",\"special\":" << a.special
      << ",\"signal_intensity\":" << a.signal_intensity << ",\"signal_type\":" << static_cast<int>(a.signal_type)
      << "}";
}

std::string dump_golden_json(uint32_t n_steps) {
    Island island = Island::make(1, Arena{80.0f});
    island.spawn_agent(Species::Utahraptor, Vec2{0, 0}, 0);
    island.spawn_agent(Species::Gallimimus, Vec2{12.0f, 2.0f}, PI);
    std::size_t n = island.agents.size();
    std::vector<Action> actions(n);
    actions[0].throttle = 0.65f;
    actions[0].steer = 0.22f;
    actions[1].throttle = 0.45f;
    actions[1].steer = -0.18f;

    std::ostringstream o;
    o << std::setprecision(std::numeric_limits<float>::max_digits10);
    o << "{\"dt\":" << DT << ",\"n_steps\":" << n_steps << ",\"n_agents\":" << n << ",\"species\":[";
    for (std::size_t i = 0; i < n; ++i) {
        if (i) o << ",";
        o << static_cast<int>(island.agents[i].species);
    }
    o << "],\"init_pos\":[";
    for (std::size_t i = 0; i < n; ++i) {
        if (i) o << ",";
        o << "[" << island.agents[i].pos.x << "," << island.agents[i].pos.z << "]";
    }
    o << "],\"init_heading\":[";
    for (std::size_t i = 0; i < n; ++i) {
        if (i) o << ",";
        o << island.agents[i].heading;
    }
    o << "],\"init_speed\":[";
    for (std::size_t i = 0; i < n; ++i) {
        if (i) o << ",";
        o << island.agents[i].speed;
    }
    o << "],\"init_energy\":[";
    for (std::size_t i = 0; i < n; ++i) {
        if (i) o << ",";
        o << island.agents[i].energy;
    }
    o << "],\"init_health\":[";
    for (std::size_t i = 0; i < n; ++i) {
        if (i) o << ",";
        o << island.agents[i].health;
    }
    o << "],\"actions\":[";
    for (std::size_t i = 0; i < n; ++i) {
        if (i) o << ",";
        append_action(o, actions[i]);
    }
    o << "],\"pos\":[";
    bool first_row = true;
    std::ostringstream heading;
    std::ostringstream energy;
    const auto prec = std::numeric_limits<float>::max_digits10;
    heading << std::setprecision(prec);
    energy << std::setprecision(prec);
    heading << "[";
    energy << "[";
    bool first_he = true;
    for (uint32_t t = 0; t < n_steps; ++t) {
        island.step(actions);
        if (!first_row) o << ",";
        first_row = false;
        o << "[";
        for (std::size_t i = 0; i < n; ++i) {
            if (i) o << ",";
            o << "[" << island.agents[i].pos.x << "," << island.agents[i].pos.z << "]";
        }
        o << "]";
        if (!first_he) {
            heading << ",";
            energy << ",";
        }
        first_he = false;
        heading << "[";
        energy << "[";
        for (std::size_t i = 0; i < n; ++i) {
            if (i) {
                heading << ",";
                energy << ",";
            }
            heading << island.agents[i].heading;
            energy << island.agents[i].energy;
        }
        heading << "]";
        energy << "]";
    }
    heading << "]";
    energy << "]";
    o << "],\"heading\":" << heading.str() << ",\"energy\":" << energy.str() << "}";
    return o.str();
}

bool write_golden_json(const std::string& path, uint32_t n_steps) {
    std::ofstream f(path);
    if (!f) return false;
    f << dump_golden_json(n_steps);
    return static_cast<bool>(f);
}

}  // namespace dino

#include "dino/scripted.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    uint32_t ticks = 1200;
    bool naive = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--naive") naive = true;
        else {
            try {
                ticks = static_cast<uint32_t>(std::stoul(a));
            } catch (...) {
            }
        }
    }
    std::filesystem::create_directories("train/runs");
    std::string path = naive ? "train/runs/naive.jsonl" : "train/runs/scripted.jsonl";
    std::ofstream f(path);
    dino::Island island = dino::Island::chase_arena(1);
    island.spawn_default_chase(3, 6);
    for (uint32_t t = 0; t < ticks; ++t) {
        dino::apply_all_scripted(island, naive);
        f << "{\"tick\":" << island.tick << ",\"agents\":[";
        bool first = true;
        for (const auto& a : island.agents) {
            if (!first) f << ",";
            first = false;
            f << "{\"id\":" << a.id << ",\"species\":" << static_cast<int>(a.species) << ",\"x\":" << a.pos.x
              << ",\"z\":" << a.pos.z << ",\"heading\":" << a.heading << ",\"alive\":" << (a.alive ? "true" : "false")
              << "}";
        }
        f << "]}\n";
    }
    std::cerr << "wrote " << path << " ticks=" << island.metrics.ticks << " kills=" << island.metrics.raptor_kills
              << " rear_frac=" << island.metrics.rear_arc_fraction() << " immigrants=" << island.metrics.immigrants
              << " circling=" << island.metrics.circling_flags << " motionless=" << island.metrics.motionless_flags
              << "\n";
    return 0;
}

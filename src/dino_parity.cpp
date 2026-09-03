#include "dino/feel.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    uint32_t steps = 1000;
    std::string out = "train/goldens/canonical.json";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "dump") continue;
        if (a == "--steps" && i + 1 < argc) steps = static_cast<uint32_t>(std::stoul(argv[++i]));
        else if (a == "--out" && i + 1 < argc) out = argv[++i];
    }
    std::filesystem::path p(out);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    if (!dino::write_golden_json(out, steps)) {
        std::cerr << "failed to write " << out << "\n";
        return 1;
    }
    std::cerr << "wrote " << steps << " steps to " << out << "\n";
    return 0;
}

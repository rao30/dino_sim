#pragma once

#include "dino/perceive.hpp"

namespace dino {

Action policy_for(const Island& island, std::size_t index, const SpatialHash& hash);
void fill_scripted_actions(const Island& island, const SpatialHash& hash, std::vector<Action>& out);
Action naive_pursuit(const Island& island, std::size_t index);
void apply_player_ranged(Island& island);
void apply_all_scripted(Island& island, bool naive_raptors);

}  // namespace dino

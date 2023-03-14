#pragma once
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "match.h"

struct player_scores {
	std::string name;
	std::vector<std::optional<double>> scores;
	double total {};
};

struct round_info {
	std::string name;
	std::string game_mode;
	double weight {};
};

struct scoring_results {
	std::vector<round_info> rounds;
	std::vector<player_scores> players;
};

std::map<std::string, double> score_match(const match_data& match);

scoring_results score(const event_data& event);

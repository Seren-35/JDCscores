#pragma once
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

enum class stats_type {
	none,
	end,
	reset,
	change,
	current,
};

struct stat_value {
	int value {};
	bool ordinal {};
};

struct player_stats {
	std::string name;
	bool renamed {};
	std::set<std::string> ips;
	std::string team;
	std::map<std::string, stat_value> stats;
};

struct match_data {
	std::string level_filename;
	std::string game_mode;
	std::string winner;
	std::map<std::string, int> team_scores;
	std::vector<player_stats> players;
	stats_type stats_source = stats_type::none;
	int start_time = -1;
	int end_time = -1;
};

struct event_data {
	std::vector<match_data> matches;
	double max_score = 100.0;
};

constexpr int duration(const match_data& match) {
	int result = match.end_time - match.start_time;
	if (result < 0)
		result += 24 * 60 * 60;
	return result;
}

bool is_string_with_number_suffix(std::string_view first, std::string_view second);
bool same_player_name(std::string_view first, std::string_view second);
bool players_qualify_to_auto_rename(const player_stats& first, const player_stats& second);
bool players_qualify_to_auto_merge(const player_stats& first, const player_stats& second);
bool prefers_secondary_name(const player_stats& first, const player_stats& second);
void merge_players(player_stats& main_player, player_stats&& secondary_player);

void rename_player(match_data& match, std::size_t index, std::string_view name);
void merge_players(match_data& match, std::size_t first, std::size_t second);
void set_game_mode(match_data& match, std::string_view mode);
void remove_player(match_data& match, std::size_t index);
void remove_match(event_data& event, std::size_t index);

void default_process(event_data& event);

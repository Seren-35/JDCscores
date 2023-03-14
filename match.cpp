#include "match.h"
#include <iostream>
#include "algorithm.h"
#include "string.h"

bool is_string_with_number_suffix(std::string_view first, std::string_view second) {
	return first.starts_with(second) && is_digits(first.substr(second.size()));
}

bool same_player_name(std::string_view first, std::string_view second) {
	return is_string_with_number_suffix(first, second) || is_string_with_number_suffix(second, first);
}

bool players_qualify_to_auto_merge(const player_stats& first, const player_stats& second) {
	if (!first.team.empty() && !second.team.empty() && first.team != second.team)
		return false;
	return sets_overlap(first.ips, second.ips) && same_player_name(first.name, second.name);
}

void rename_player(match_data& match, std::size_t index, std::string_view name) {
	if (index >= match.players.size()) {
		std::cerr << "ERROR: player index out of range\n";
		return;
	}
	auto& player = match.players[index];
	player.name = name;
	player.renamed = true;
}

void merge_players(match_data& match, std::size_t first, std::size_t second) {
	if (first == second) {
		std::cerr << "ERROR: cannot merge a player with themselves\n";
		return;
	}
	if (first >= match.players.size() || second >= match.players.size()) {
		std::cerr << "ERROR: player index out of range\n";
		return;
	}
	auto& main_player = match.players[first];
	auto& secondary_player = match.players[second];
	if (main_player.team != secondary_player.team) {
		if (main_player.team.empty())
			main_player.team = std::move(secondary_player.team);
		else if (!secondary_player.team.empty())
			std::cerr << "WARNING: merged players have different teams - " << main_player.team << " and " << secondary_player.team << '\n';
	}
	if (!main_player.renamed && (secondary_player.renamed || is_string_with_number_suffix(main_player.name, secondary_player.name))) {
		main_player.name = std::move(secondary_player.name);
		main_player.renamed = secondary_player.renamed;
	}
	main_player.ips.merge(secondary_player.ips);
	for (auto&& [key, value] : main_player.stats) {
		auto other = secondary_player.stats.find(key)->second;
		if (!value.ordinal)
			value.value += other.value;
		else if (other.value != 0 && (value.value == 0 || other.value < value.value))
			value.value = other.value;
	}
	match.players.erase(match.players.begin() + second);
}

void set_game_mode(match_data& match, std::string_view mode) {
	match.game_mode = mode;
}

void remove_player(match_data& match, std::size_t index) {
	if (index >= match.players.size()) {
		std::cerr << "ERROR: player index out of range\n";
		return;
	}
	match.players.erase(match.players.begin() + index);
}

void remove_match(event_data& event, std::size_t index) {
	if (index >= event.matches.size()) {
		std::cerr << "ERROR: match index out of range\n";
		return;
	}
	event.matches.erase(event.matches.begin() + index);
}

void default_process(event_data& event) {
	for (auto&& match : event.matches) {
		for (std::size_t i = 0; i < match.players.size(); i++) {
			const auto& first = match.players[i];
			for (std::size_t j = i + 1; j < match.players.size(); j++) {
				const auto& second = match.players[j];
				if (players_qualify_to_auto_merge(first, second)) {
					merge_players(match, i, j);
					j--;
				}
			}
		}
	}
}

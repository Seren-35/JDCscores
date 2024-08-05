#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <utility>
#include "playlog.h"
#include "string.h"

using namespace std::string_view_literals;

void playlog_parser::report_warning(std::string_view message) {
	std::cerr << "WARNING (line " << line_count << "): " << message << '\n';
}

void playlog_parser::interpret_long_timestamp_line(std::string_view line) {
	bool stats_start = consume_prefix(line, "*** ");
	if (stats_start) {
		if (!consume_suffix(line, " ***")) {
			report_warning("expected \" ***\" at the end of the line");
			return;
		}
		if (consume_prefix(line, "Game End Stats ")) {
			stats_source = stats_type::end;
		} else if (consume_prefix(line, "Game Reset Stats ")) {
			stats_source = stats_type::reset;
		} else if (consume_prefix(line, "Game Change Stats ")) {
			stats_source = stats_type::change;
		} else if (consume_prefix(line, "Current Stats ")) {
			stats_source = stats_type::current;
		} else {
			report_warning("unknown stats declaration");
			return;
		}
	} else {
		end_level();
	}
	if (!consume_prefix(line, "[[") || !consume_suffix(line, "]]")) {
		report_warning("expected \"[[\" and \"]]\" around the timestamp");
		return;
	}
	if (line.size() < 8) {
		report_warning("invalid timestamp");
		return;
	}
	auto timestamp = hhmmss_to_seconds(line.substr(line.size() - 8));
	if (!timestamp) {
		report_warning("failed to parse timestamp");
		return;
	}
	if (stats_start)
		match.end_time = *timestamp;
}

void playlog_parser::interpret_info_line(std::string_view line) {
	consume_prefix(line, "**");
	auto colon_index = line.find(": ");
	if (colon_index == std::string_view::npos) {
		report_warning("info line contains no \": \" sequence");
		return;
	}
	auto key = line.substr(0, colon_index);
	auto value = line.substr(colon_index + 2);
	if (key == "Current level") {
		auto quote_index = value.rfind("\" - ");
		if (quote_index == std::string_view::npos) {
			report_warning("unexpected level name format");
			return;
		}
		auto filename = value.substr(quote_index + 4);
		consume_suffix(filename, ".j2l");
		level_filename = filename;
		return;
	}
	if (key == "Next level")
		return;
	if (key == "Game Mode") {
		game_mode = value;
		return;
	}
	if (key == "Custom Mode") {
		custom_mode = value;
		return;
	}
	report_warning("unrecognized key");
}

void playlog_parser::interpret_game_alert(std::string_view line, int timestamp) {
	const std::string_view ignored_messages[] {
		"Game End",
		"Server Close",
		"DLL Unloaded",
		"Reset Settings",
		"Loaded Settings",
		"Team Shuffle",
	};
	if (std::ranges::count(ignored_messages, line))
		return;
	if (line == "Game Start") {
		match.start_time = timestamp;
		return;
	}
	if (consume_prefix(line, "Mode: ")) {
		match.start_time = timestamp;
		game_mode = line;
		custom_mode.clear();
		return;
	}
	report_warning("unknown game alert");
}

void playlog_parser::enqueue_player_leave(std::string_view line) {
	leaving_players.push_back(line_info {.line = std::string(line), .number = line_count});
}

void playlog_parser::interpret_player_leave(std::string_view line) {
	player_stats stats;
	for (auto it = table_header.begin(); it != table_header.end(); ++it) {
		const auto& header = it->header;
		if (!consume_prefix(line, header)) {
			report_warning("expected \"" + header + '"');
			return;
		}
		if (!consume_prefix(line, ": ")) {
			// Special case to get around a JJ2+ bug.
			if (!(header == "FRoasts" && consume_prefix(line, " "))) {
				report_warning("expected \": \"");
				return;
			}
		}
		if (header != "Name") {
			auto space_index = line.find_first_of(" \t");
			bool success = interpret_table_cell(line.substr(0, space_index), header, stats);
			if (!success)
				return;
			line = space_index != std::string_view::npos ? without_leading_whitespace(line.substr(space_index)) : ""sv;
		} else {
			std::size_t space_index = std::string_view::npos;
			if (auto next = std::next(it); next != table_header.end()) {
				const auto& next_header = next->header;
				auto key = ' ' + next_header + ": ";
				space_index = line.rfind(key);
				if (space_index == std::string_view::npos) {
					report_warning("expected \"" + next_header + '"');
					return;
				}
			}
			bool success = interpret_table_cell(without_trailing_whitespace(line.substr(0, space_index)), header, stats);
			if (!success)
				return;
			if (space_index != std::string_view::npos)
				line.remove_prefix(space_index + 1);
		}
	}
	for (auto& other : match.players) {
		if (same_player_name(stats.name, other.name)) {
			merge_players(other, std::move(stats));
			return;
		}
	}
	match.players.push_back(std::move(stats));
}

void playlog_parser::interpret_event_line(std::string_view line) {
	if (line.size() < 11) {
		report_warning("timestamped line shorter than expected");
		return;
	}
	auto timestamp = hhmmss_to_seconds(line.substr(1, 8));
	if (!timestamp || line[9] != ']' || !is_space(line[10])) {
		report_warning("failed to parse timestamp");
		return;
	}
	line.remove_prefix(11);
	if (consume_prefix(line, ">>> ")) {
		interpret_game_alert(line, *timestamp);
		return;
	}
	if (line.starts_with("ID: ")) {
		enqueue_player_leave(line);
		return;
	}
	report_warning("unrecognized timestamped message");
}

void playlog_parser::interpret_winner_line(std::string_view line) {
	if (stats_source == stats_type::none) {
		report_warning("unexpected winner declaration");
		return;
	}
	if (stats_source == stats_type::current)
		return;
	if (consume_prefix(line, ">> Winner: ")) {
		match.winner = line;
		return;
	}
	if (consume_suffix(line, "-way tie")) {
		match.winner = '|' + std::string(line) + '|';
		return;
	}
	report_warning("incorrectly formatted winner declaration");
}

void playlog_parser::interpret_table_header_line(std::string_view line) {
	if (stats_source == stats_type::none) {
		report_warning("unexpected table header");
		return;
	}
	if (stats_source == stats_type::current)
		return;
	table_header.clear();
	while (!line.empty()) {
		column_info column;
		for (const auto& special_header : {"IP Address"sv, "Player Name"sv}) {
			if (consume_prefix(line, special_header)) {
				column.header = special_header.substr(special_header.starts_with("Player ") ? 7 : 0);
				column.size = special_header.size();
			}
		}
		while (!line.empty() && !is_space(line.front())) {
			column.header.push_back(line.front());
			column.size++;
			line.remove_prefix(1);
		}
		while (!line.empty() && is_space(line.front())) {
			column.size++;
			line.remove_prefix(1);
		}
		table_header.push_back(std::move(column));
	}
}

bool playlog_parser::interpret_table_cell(std::string_view cell, std::string_view label, player_stats& stats) {
	if (label == "ID")
		return true;
	if (label == "Name") {
		stats.name = cell;
		return true;
	}
	if (label == "IP Address") {
		stats.ips.emplace(cell);
		return true;
	}
	if (label == "Team") {
		stats.team = cell;
		return true;
	}
	bool ordinal = cell == "N/A";
	for (const auto& ordinal_suffix : {"th", "st", "nd", "rd"}) {
		if (!ordinal && consume_suffix(cell, ordinal_suffix))
			ordinal = true;
	}
	auto numeric_value = cell != "N/A" ? to_int(cell) : 0;
	if (!numeric_value) {
		report_warning("could not parse the value in column \"" + std::string(label) + '"');
		return false;
	}
	stats.stats[to_lower(label)] = {.value = *numeric_value, .ordinal = ordinal};
	return true;
}

void playlog_parser::interpret_table_row_line(std::string_view line) {
	if (stats_source == stats_type::none) {
		report_warning("unexpected table row");
		return;
	}
	if (stats_source == stats_type::current)
		return;
	if (table_header.empty()) {
		report_warning("table row found before header");
		return;
	}
	player_stats stats;
	for (const auto& column : table_header) {
		auto cell = line.substr(0, column.size);
		if (cell.empty()) {
			report_warning("table row too short");
			return;
		}
		line.remove_prefix(cell.size());
		bool success = interpret_table_cell(without_trailing_whitespace(cell), column.header, stats);
		if (!success)
			return;
	}
	match.players.push_back(std::move(stats));
}

void playlog_parser::interpret_team_score_line(std::string_view line, std::size_t name_length) {
	if (stats_source == stats_type::none) {
		report_warning("unexpected team score line");
		return;
	}
	if (stats_source == stats_type::current)
		return;
	auto team_name = line.substr(0, name_length);
	auto value = line.substr(name_length + 8);
	auto numeric_value = to_int(value);
	if (!numeric_value) {
		report_warning("could not parse team score value");
		return;
	}
	auto [it, success] = match.team_scores.emplace(std::string(team_name), *numeric_value);
	if (!success)
		report_warning("duplicate team score value");
}

void playlog_parser::interpret_line(std::string_view line) {
	if (is_spaces(line))
		return;
	consume_prefix(line, "\r");
	consume_suffix(line, "\r");
	if (line.starts_with("[[") || line.starts_with("*** ")) {
		interpret_long_timestamp_line(line);
		return;
	}
	if (line.starts_with("**")) {
		interpret_info_line(line);
		return;
	}
	if (line.starts_with("[")) {
		interpret_event_line(line);
		return;
	}
	if (line.starts_with(">>")) {
		interpret_winner_line(line);
		return;
	}
	if (line.starts_with("ID")) {
		interpret_table_header_line(line);
		return;
	}
	if (!line.empty() && is_digit(line.front())) {
		interpret_table_row_line(line);
		return;
	}
	if (auto name_length = line.find(" Score: "); name_length != std::string_view::npos) {
		interpret_team_score_line(line, name_length);
		return;
	}
	report_warning("unrecognized line type");
}

void playlog_parser::generate_table_header_from_leaving_players() {
	std::cerr << "INFO (line " << leaving_players.front().number << "): match has leaving players but no game stats\n";
	std::cerr << "INFO: this may lead to unexpected results if all player names contain colons\n";
	std::vector<std::string_view> result;
	for (const auto& leaving_player : leaving_players) {
		std::vector<std::string_view> header;
		const auto& line = leaving_player.line;
		for (auto colon = line.find(':'); colon != std::string::npos; colon = line.find(':', colon + 1)) {
			auto start = line.rfind(' ', colon) + 1;
			header.emplace_back(line.begin() + start, line.begin() + colon);
		}
		if (result.empty()) {
			result = std::move(header);
		} else {
			std::vector<std::string_view> intersection;
			auto first = result.begin();
			auto second = header.begin();
			while (first != result.end() && second != header.end()) {
				if (*first == *second) {
					intersection.push_back(*first);
					++first;
					++second;
				} else {
					auto first_remaining = std::distance(first, result.end());
					auto second_remaining = std::distance(second, header.end());
					if (first_remaining >= second_remaining)
						++first;
					if (first_remaining <= second_remaining)
						++second;
				}
			}
			result = std::move(intersection);
		}
	}
	auto address = std::ranges::find(result, "Address");
	if (address != result.end())
		*address = "IP Address";
	for (const auto& column_name : result) {
		table_header.push_back(column_info {.header = std::string(column_name)});
	}
}

void playlog_parser::finalize_table() {
	if (stats_source == stats_type::current) {
		stats_source = stats_type::none;
		return;
	}
	auto original_line_count = line_count;
	if (table_header.empty() && !leaving_players.empty())
		generate_table_header_from_leaving_players();
	for (const auto& leaving_player : leaving_players) {
		line_count = leaving_player.number;
		interpret_player_leave(leaving_player.line);
	}
	line_count = original_line_count;
	leaving_players.clear();
	table_header.clear();
	match.level_filename = level_filename;
	match.game_mode = custom_mode.empty() || custom_mode == "OFF" ? game_mode : custom_mode;
	match.stats_source = stats_source;
	stats_source = stats_type::none;
	auto end_time = match.end_time;
	if (!match.players.empty())
		result.matches.push_back(std::move(match));
	match = {};
	match.start_time = end_time;
}

void playlog_parser::end_level() {
	finalize_table();
	level_filename.clear();
	game_mode.clear();
	custom_mode.clear();
}

void playlog_parser::clean_up() {
	end_level();
}

playlog_parser::playlog_parser(event_data& result)
	: result(result) {}

void playlog_parser::parse(std::istream& input) {
	std::string line;
	while (std::getline(input, line)) {
		line_count++;
		interpret_line(line);
	}
	clean_up();
}

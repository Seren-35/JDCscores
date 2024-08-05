#pragma once
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include "match.h"

class playlog_parser {
private:
	struct line_info {
		std::string line;
		std::size_t number {};
	};
	struct column_info {
		std::string header;
		std::size_t size {};
	};
	void report_warning(std::string_view message);
	void interpret_long_timestamp_line(std::string_view line);
	void interpret_info_line(std::string_view line);
	void interpret_game_alert(std::string_view line, int timestamp);
	void enqueue_player_leave(std::string_view line);
	void interpret_player_leave(std::string_view line);
	void interpret_event_line(std::string_view line);
	void interpret_winner_line(std::string_view line);
	void interpret_table_header_line(std::string_view line);
	bool interpret_table_cell(std::string_view cell, std::string_view label, player_stats& stats);
	void interpret_table_row_line(std::string_view line);
	void interpret_team_score_line(std::string_view line, std::size_t name_length);
	void interpret_line(std::string_view line);
	void generate_table_header_from_leaving_players();
	void finalize_table();
	void end_level();
	void clean_up();
public:
	playlog_parser(event_data& result);
	void parse(std::istream& input);
private:
	std::size_t line_count {};
	event_data& result;
	match_data match;
	std::string level_filename;
	std::string game_mode;
	std::string custom_mode;
	stats_type stats_source = stats_type::none;
	std::vector<column_info> table_header;
	std::vector<line_info> leaving_players;
};

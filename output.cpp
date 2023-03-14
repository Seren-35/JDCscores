#include "output.h"

std::string sanitized_csv_string(std::string_view sv) {
	auto format_character_index = sv.find_first_of(",\"\r\n");
	if (format_character_index == std::string_view::npos)
		return std::string(sv);
	auto double_quote_index = sv.find('"');
	if (double_quote_index == std::string_view::npos)
		return '"' + std::string(sv) + '"';
	std::string result(1, '"');
	for (const auto& c : sv) {
		result += c;
		if (c == '"')
			result += c;
	}
	result += '"';
	return result;
}

void output_as_csv(std::ostream& os, const scoring_results& results) {
	os << "Round,";
	for (const auto& round : results.rounds) {
		os << sanitized_csv_string(round.name) << ',';
	}
	os << "Total\n";
	os << "Weight,";
	for (const auto& round : results.rounds) {
		os << round.weight << ',';
	}
	os << '\n';
	for (const auto& player : results.players) {
		os << sanitized_csv_string(player.name) << ',';
		for (const auto& round_score : player.scores) {
			if (round_score)
				os << *round_score;
			os << ',';
		}
		os << player.total << '\n';
	}
}

#pragma once
#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>

inline bool is_space(char c) {
	return std::isspace(c & 255);
}

inline bool is_digit(char c) {
	return std::isdigit(c & 255);
}

inline bool is_spaces(std::string_view sv) {
	return std::ranges::all_of(sv, is_space);
}

inline bool is_digits(std::string_view sv) {
	return std::ranges::all_of(sv, is_digit);
}

inline std::optional<int> to_int(std::string_view sv) {
	int result {};
	auto info = std::from_chars(sv.data(), sv.data() + sv.size(), result);
	if (info.ec != std::errc {})
		return std::nullopt;
	return result;
}

inline std::optional<int> hhmmss_to_seconds(std::string_view sv) {
	if (sv.size() != 8 || sv[2] != ':' || sv[5] != ':')
		return std::nullopt;
	auto hh = to_int(sv.substr(0, 2));
	auto mm = to_int(sv.substr(3, 2));
	auto ss = to_int(sv.substr(6, 2));
	if (!hh || !mm || !ss)
		return std::nullopt;
	return *hh * 3600 + *mm * 60 + *ss;
}

inline std::string to_lower(std::string_view sv) {
	std::string result(sv);
	for (auto&& c : result) {
		c = std::tolower(c);
	}
	return result;
}

inline std::string_view without_leading_whitespace(std::string_view sv) {
	while (!sv.empty() && is_space(sv.front())) {
		sv.remove_prefix(1);
	}
	return sv;
}

inline std::string_view without_trailing_whitespace(std::string_view sv) {
	while (!sv.empty() && is_space(sv.back())) {
		sv.remove_suffix(1);
	}
	return sv;
}

inline bool consume_prefix(std::string_view& sv, std::string_view prefix) {
	if (sv.starts_with(prefix)) {
		sv.remove_prefix(prefix.size());
		return true;
	}
	return false;
}

inline bool consume_suffix(std::string_view& sv, std::string_view suffix) {
	if (sv.ends_with(suffix)) {
		sv.remove_suffix(suffix.size());
		return true;
	}
	return false;
}

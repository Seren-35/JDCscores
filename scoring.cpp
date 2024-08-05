#include "scoring.h"
#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <numeric>
#include <sol/sol.hpp>
#include "string.h"

void set_up_lua(sol::state& lua, const match_data& match, const player_stats& player) {
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
    lua["gamemode"] = match.game_mode;
    lua["duration"] = duration(match);
    auto team_scores_table = lua.create_table();
    for (const auto& [name, score] : match.team_scores) {
        team_scores_table.set(name, score);
    }
    lua["teamscores"] = std::move(team_scores_table);
    lua["team"] = player.team;
    lua["iswinner"] = match.team_scores.empty() ? match.winner == player.name : match.winner == player.team + " Team";
    for (const auto& [name, value] : player.stats) {
        lua[to_lower(name)] = value.value;
    }
    auto players_table = lua.create_table();
    for (const auto& other_player : match.players) {
        auto stats_table = lua.create_table_with(
            "current", &other_player == &player,
            "team", other_player.team,
            "iswinner", match.team_scores.empty() ? match.winner == other_player.name : match.winner == other_player.team + " Team"
        );
        for (const auto& [name, value] : other_player.stats) {
            stats_table.set(to_lower(name), value.value);
        }
        players_table.add(std::move(stats_table));
    }
    lua["players"] = std::move(players_table);
}

std::map<std::string, double> score_match(const match_data& match) {
    std::map<std::string, double> result;
    std::string filename = to_lower(match.game_mode);
    std::ranges::replace(filename, ' ', '_');
    filename += ".lua";
    std::filesystem::path path("scoring");
    path /= filename;
    if (!std::filesystem::is_regular_file(path)) {
        std::cerr << "WARNING: no regular file named " << filename << " found\n";
        return {};
    }
    for (const auto& player : match.players) {
        try {
            sol::state lua;
            set_up_lua(lua, match, player);
            auto returned_value = lua.safe_script_file(path.string(), [](lua_State*, auto pfr) -> decltype(pfr) {
                throw pfr.get<sol::error>();
            });
            result[player.name] = returned_value.get<double>();
        } catch (const sol::error& e) {
            std::cerr << "WARNING: error running scoring script for level " << match.level_filename << ", player " << player.name << '\n';
            std::cerr << "INFO: " << e.what();
            return {};
        }
    }
    return result;
}

struct game_mode_data {
    int total_rounds {};
    int total_time {};
    double total_score {};
};

scoring_results score(const event_data& event) {
    scoring_results results;
    std::map<std::string, game_mode_data> game_modes;
    for (const auto& match : event.matches) {
        auto round_scores = score_match(match);
        bool any_points = std::ranges::any_of(round_scores, [](const auto& player_score) {
            return player_score.second != 0.0;
        });
        if (any_points) {
            double total_score = 0.0;
            auto& round = results.rounds.emplace_back();
            round.name = match.level_filename;
            round.game_mode = match.game_mode;
            for (auto&& player : results.players) {
                auto it = round_scores.find(player.name);
                if (it != round_scores.end()) {
                    player.scores.emplace_back(it->second);
                    total_score += it->second;
                    round_scores.erase(it);
                } else {
                    player.scores.emplace_back(std::nullopt);
                }
            }
            for (const auto& [name, score] : round_scores) {
                auto& player = results.players.emplace_back();
                player.name = name;
                player.scores.resize(results.rounds.size());
                player.scores.back() = score;
                total_score += score;
            }
            auto& game_mode = game_modes[round.game_mode];
            game_mode.total_rounds++;
            game_mode.total_time += duration(match);
            if (total_score > 0.0)
                game_mode.total_score += total_score;
        }
    }
    for (auto&& round : results.rounds) {
        const auto& game_mode = game_modes.find(round.game_mode)->second;
        if (game_mode.total_score > 0.0)
            round.weight = game_mode.total_time / game_mode.total_score;
    }
    for (auto&& player : results.players) {
        auto& scores = player.scores;
        auto transform = [](auto score, const auto& round) {
            return (score ? *score : 0.0) * round.weight;
        };
        player.total = std::transform_reduce(scores.begin(), scores.end(), results.rounds.begin(), 0.0, std::plus {}, transform);
    }
    std::ranges::sort(results.players, [](const player_scores& lhs, const player_scores& rhs) {
        return lhs.total > rhs.total;
    });
    if (!results.players.empty()) {
        double global_weight = event.max_score / results.players.front().total;
        for (auto&& round : results.rounds) {
            round.weight *= global_weight;
        }
        for (auto&& player : results.players) {
            player.total *= global_weight;
        }
    }
    return results;
}

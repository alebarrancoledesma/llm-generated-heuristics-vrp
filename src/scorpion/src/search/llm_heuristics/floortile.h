#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <limits>
#include <cmath>
#include <map>
#include <variant>

#include "domain_dependent_heuristic.h"

namespace llm_heuristics {
std::vector<std::string> get_parts(const std::string &fact) {
    std::string fact_no_paren = fact.substr(1, fact.length() - 2);
    std::stringstream ss(fact_no_paren);
    std::string part;
    std::vector<std::string> parts;
    while (std::getline(ss, part, ' ')) {
        parts.push_back(part);
    }
    return parts;
}

std::pair<int, int> parse_tile(const std::string &tile_name) {
    std::stringstream ss(tile_name);
    std::string part;
    std::vector<std::string> parts;
    while (std::getline(ss, part, '_')) {
        parts.push_back(part);
    }
    return {std::stoi(parts[1]), std::stoi(parts[2])};
}

class FloortileHeuristic : public DomainDependentHeuristic {
public:
    std::map<std::string, std::string> goal_tiles;

    FloortileHeuristic(const Task &task) {
        for (const std::string &goal : task.goals) {
            std::vector<std::string> parts = get_parts(goal);
            if (parts[0] == "painted") {
                std::string tile = parts[1];
                std::string color = parts[2];
                goal_tiles[tile] = color;
            }
        }
    }

    int operator()(const std::vector<std::string> &state) override {
        std::map<std::string, std::string> current_painted;
        std::map<std::string, std::map<std::string, std::variant<std::pair<int, int>, std::string>>> robots;

        for (const std::string &fact : state) {
            std::vector<std::string> parts = get_parts(fact);
            if (parts[0] == "painted") {
                std::string tile = parts[1];
                std::string color = parts[2];
                current_painted[tile] = color;
            } else if (parts[0] == "robot-at") {
                std::string robot = parts[1];
                std::string tile = parts[2];
                std::pair<int, int> pos = parse_tile(tile);
                if (robots.find(robot) == robots.end()) {
                    robots[robot]["pos"] = std::make_pair(-1, -1);
                    robots[robot]["color"] = "";
                }
                robots[robot]["pos"] = pos;
            } else if (parts[0] == "robot-has") {
                std::string robot = parts[1];
                std::string color = parts[2];
                if (robots.find(robot) == robots.end()) {
                    robots[robot]["pos"] = std::make_pair(-1, -1);
                    robots[robot]["color"] = "";
                }
                robots[robot]["color"] = color;
            }
        }

        int total_cost = 0;

        for (const auto & [tile, req_color] : goal_tiles) {
            if (current_painted.count(tile) > 0 && current_painted[tile] == req_color) {
                continue; // Already painted correctly
            }

            std::pair<int, int> tile_coords = parse_tile(tile);
            int tx = tile_coords.first;
            int ty = tile_coords.second;

            int min_distance = std::numeric_limits<int>::max();
            std::vector<std::map<std::string, std::variant<std::pair<int, int>, std::string>> *> candidates;

            for (auto & [robot_name, robot_data] : robots) {
                auto &robot = robots[robot_name];
                if (std::get<std::pair<int, int>>(robot["pos"]).first == -1) {
                    continue; // Skip if robot's position is unknown (invalid state)
                }

                int rx = std::get<std::pair<int, int>>(robot["pos"]).first;
                int ry = std::get<std::pair<int, int>>(robot["pos"]).second;
                int distance_to_t = std::abs(rx - tx) + std::abs(ry - ty);
                int adj_distance = distance_to_t - 1; // Distance to adjacent tile

                if (adj_distance < min_distance) {
                    min_distance = adj_distance;
                    candidates.clear();
                    candidates.push_back(&robot);
                } else if (adj_distance == min_distance) {
                    candidates.push_back(&robot);
                }
            }

            if (candidates.empty()) {
                continue; // No robots available; assume this is handled elsewhere
            }

            // Check if any closest robot has the required color
            bool has_required_color = false;
            for (auto *r : candidates) {
                if (std::get<std::string>((*r)["color"]) == req_color) {
                    has_required_color = true;
                    break;
                }
            }

            int color_cost = has_required_color ? 0 : 1;
            int cost = min_distance + color_cost + 1; // +1 for paint action
            total_cost += cost;
        }

        return total_cost;
    }
};
} // namespace llm_heuristics

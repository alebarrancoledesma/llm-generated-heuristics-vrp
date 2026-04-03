#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <algorithm>
#include <limits>
#include <sstream>

#include "domain_dependent_heuristic.h"

namespace std {
template<>
struct hash<std::pair<std::string, std::string>> {
    std::size_t operator()(const std::pair<std::string, std::string> &p) const {
        auto h1 = std::hash<std::string>{}(p.first);
        auto h2 = std::hash<std::string>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};
} // namespace std

namespace llm_heuristics {
class MiconicHeuristic : public DomainDependentHeuristic {
public:
    MiconicHeuristic(const Task &task);

    int operator()(const std::vector<std::string> &state) override;

private:
    std::unordered_map<std::string, std::string> destin_;
    std::unordered_map<std::string, std::vector<std::string>> graph_;
    std::unordered_map<std::pair<std::string, std::string>, int> distances_;

    std::vector<std::string> get_parts(const std::string &fact);
};

MiconicHeuristic::MiconicHeuristic(const Task &task) {
    // Extract 'destin' predicates from static facts
    for (const std::string &fact : task.static_facts) {
        std::vector<std::string> parts = get_parts(fact);
        if (parts[0] == "destin") {
            std::string passenger = parts[1];
            std::string floor = parts[2];
            destin_[passenger] = floor;
        }
    }

    // Build undirected graph from 'above' facts
    for (const std::string &fact : task.static_facts) {
        std::vector<std::string> parts = get_parts(fact);
        if (parts[0] == "above") {
            std::string f1 = parts[1];
            std::string f2 = parts[2];
            graph_[f1].push_back(f2);
            graph_[f2].push_back(f1);    // Allow bidirectional movement
        }
    }

    // Precompute all-pairs shortest paths using BFS
    std::unordered_set<std::string> all_floors;
    for (const auto &pair : graph_) {
        all_floors.insert(pair.first);
        for (const std::string &neighbor : pair.second) {
            all_floors.insert(neighbor);
        }
    }

    for (const std::string &start : all_floors) {
        std::unordered_map<std::string, int> visited;
        std::queue<std::string> queue;
        visited[start] = 0;
        queue.push(start);

        while (!queue.empty()) {
            std::string current = queue.front();
            queue.pop();

            if (graph_.count(current)) {
                for (const std::string &neighbor : graph_.at(current)) {
                    if (visited.find(neighbor) == visited.end()) {
                        visited[neighbor] = visited[current] + 1;
                        queue.push(neighbor);
                    }
                }
            }
        }

        for (const auto &pair : visited) {
            distances_[{start, pair.first}] = pair.second;
        }
    }
}

int MiconicHeuristic::operator()(const std::vector<std::string> &state) {
    std::string current_lift;
    std::unordered_map<std::string, std::string> current_origins;
    std::unordered_set<std::string> boarded;
    std::unordered_set<std::string> served;

    // Extract current lift position, origins, boarded, and served passengers
    for (const std::string &fact : state) {
        std::vector<std::string> parts = get_parts(fact);
        if (parts[0] == "lift-at") {
            current_lift = parts[1];
        } else if (parts[0] == "origin") {
            std::string passenger = parts[1];
            std::string floor = parts[2];
            current_origins[passenger] = floor;
        } else if (parts[0] == "boarded") {
            boarded.insert(parts[1]);
        } else if (parts[0] == "served") {
            served.insert(parts[1]);
        }
    }

    if (current_lift.empty()) {
        return 0;    // Invalid state, assume goal reached
    }

    int total = 0;
    for (const auto &pair : destin_) {
        const std::string &passenger = pair.first;
        const std::string &dest_floor = pair.second;

        if (served.count(passenger)) {
            continue;
        }

        if (boarded.count(passenger)) {
            // Passenger is boarded: need to move to destination and depart
            auto it = distances_.find({current_lift, dest_floor});
            int dist = (it != distances_.end()) ? it->second : 0;
            total += dist + 1;    // depart action
        } else {
            // Passenger is waiting: need to board and then depart
            auto origin_it = current_origins.find(passenger);
            if (origin_it == current_origins.end()) {
                continue;    // Should not happen in valid states
            }
            const std::string &origin_floor = origin_it->second;

            auto dist_to_origin_it = distances_.find({current_lift, origin_floor});
            int dist_to_origin = (dist_to_origin_it != distances_.end()) ? dist_to_origin_it->second : 0;

            auto dist_to_dest_it = distances_.find({origin_floor, dest_floor});
            int dist_to_dest = (dist_to_dest_it != distances_.end()) ? dist_to_dest_it->second : 0;

            total += dist_to_origin + 1 + dist_to_dest + 1;    // board and depart actions
        }
    }

    return total;
}

std::vector<std::string> MiconicHeuristic::get_parts(const std::string &fact) {
    std::string fact_no_parentheses = fact.substr(1, fact.length() - 2);    // Remove parentheses
    std::vector<std::string> parts;
    std::string current_part;
    std::stringstream ss(fact_no_parentheses);
    while (ss >> current_part) {
        parts.push_back(current_part);
    }
    return parts;
}
} // namespace llm_heuristics

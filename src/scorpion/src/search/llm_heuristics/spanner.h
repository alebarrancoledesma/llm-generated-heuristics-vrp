#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <queue>
#include <map>
#include <set>
#include <limits>

#include "domain_dependent_heuristic.h"

namespace llm_heuristics {
class SpannerHeuristic : public DomainDependentHeuristic {
public:
    SpannerHeuristic(const Task &task) : task_(task) {
        // Extract static link facts to build a directed graph of locations.
        for (const std::string &fact : task_.static_facts) {
            if (fact.rfind("(link ", 0) == 0) {
                std::stringstream ss(fact.substr(1, fact.size() - 2));     // Remove parentheses
                std::string token;
                std::vector<std::string> parts;
                while (ss >> token) {
                    parts.push_back(token);
                }
                if (parts.size() == 3 && parts[0] == "link") {
                    static_links_[parts[1]].push_back(parts[2]);
                }
            }
        }

        // Precompute shortest paths between all locations
        std::set<std::string> all_locations_set;
        for (const auto &pair : static_links_) {
            all_locations_set.insert(pair.first);
            for (const std::string &end : pair.second) {
                all_locations_set.insert(end);
            }
        }
        std::vector<std::string> all_locations(all_locations_set.begin(), all_locations_set.end());

        for (const std::string &loc : all_locations) {
            std::map<std::string, int> distances;
            std::queue<std::string> queue;
            distances[loc] = 0;
            queue.push(loc);

            while (!queue.empty()) {
                std::string current = queue.front();
                queue.pop();

                for (const std::string &neighbor : static_links_[current]) {
                    if (distances.find(neighbor) == distances.end()) {
                        distances[neighbor] = distances[current] + 1;
                        queue.push(neighbor);
                    }
                }
            }
            shortest_paths_[loc] = distances;
        }
    }

    virtual int operator()(const std::vector<std::string> &state) override {
        // Find man's location (assumed to be 'bob')
        std::string man_location;
        for (const std::string &fact : state) {
            if (fact.rfind("(at bob ", 0) == 0) {
                std::stringstream ss(fact.substr(1, fact.size() - 2));
                std::string token;
                std::vector<std::string> parts;
                while (ss >> token) {
                    parts.push_back(token);
                }
                if (parts.size() == 3 && parts[0] == "at" && parts[1] == "bob") {
                    man_location = parts[2];
                    break;
                }
            }
        }
        if (man_location.empty()) {
            return DEAD_END;      // Invalid state
        }

        // Collect loose nuts and their locations
        std::vector<std::string> loose_nuts;
        std::map<std::string, std::string> nut_locations;
        for (const std::string &fact : state) {
            if (fact.rfind("(loose ", 0) == 0) {
                std::stringstream ss(fact.substr(1, fact.size() - 2));
                std::string token;
                std::vector<std::string> parts;
                while (ss >> token) {
                    parts.push_back(token);
                }
                if (parts.size() == 2 && parts[0] == "loose") {
                    loose_nuts.push_back(parts[1]);
                }
            } else if (fact.rfind("(at ", 0) == 0 && fact.find("nut") != std::string::npos) {
                std::stringstream ss(fact.substr(1, fact.size() - 2));
                std::string token;
                std::vector<std::string> parts;
                while (ss >> token) {
                    parts.push_back(token);
                }
                if (parts.size() == 3 && parts[0] == "at") {
                    nut_locations[parts[1]] = parts[2];
                }
            }
        }

        // Collect usable spanners and their locations
        std::vector<std::string> usable_spanners;
        std::vector<std::string> carried_spanners;
        std::map<std::string, std::string> spanner_locations;
        for (const std::string &fact : state) {
            if (fact.rfind("(usable ", 0) == 0) {
                std::stringstream ss(fact.substr(1, fact.size() - 2));
                std::string token;
                std::vector<std::string> parts;
                while (ss >> token) {
                    parts.push_back(token);
                }
                if (parts.size() == 2 && parts[0] == "usable") {
                    usable_spanners.push_back(parts[1]);
                }
            } else if (fact.rfind("(carrying bob ", 0) == 0) {
                std::stringstream ss(fact.substr(1, fact.size() - 2));
                std::string token;
                std::vector<std::string> parts;
                while (ss >> token) {
                    parts.push_back(token);
                }
                if (parts.size() == 3 && parts[0] == "carrying" && parts[1] == "bob") {
                    carried_spanners.push_back(parts[2]);
                }
            } else if (fact.rfind("(at ", 0) == 0 && fact.find("spanner") != std::string::npos) {
                std::stringstream ss(fact.substr(1, fact.size() - 2));
                std::string token;
                std::vector<std::string> parts;
                while (ss >> token) {
                    parts.push_back(token);
                }
                if (parts.size() == 3 && parts[0] == "at") {
                    spanner_locations[parts[1]] = parts[2];
                }
            }
        }

        // Prepare available spanners (carried or on ground)
        std::vector<std::tuple<std::string, std::string, bool>> available_spanners;
        for (const std::string &spanner : usable_spanners) {
            bool is_carried = false;
            for (const std::string &carried_spanner : carried_spanners) {
                if (spanner == carried_spanner) {
                    is_carried = true;
                    break;
                }
            }
            if (is_carried) {
                available_spanners.emplace_back(spanner, man_location, true);
            } else if (spanner_locations.find(spanner) != spanner_locations.end()) {
                available_spanners.emplace_back(spanner, spanner_locations[spanner], false);
            }
        }

        // Assign spanners to nuts greedily
        int total_cost = 0;
        std::set<std::string> used_spanners;
        for (const std::string &nut : loose_nuts) {
            auto it = nut_locations.find(nut);
            if (it == nut_locations.end()) {
                continue;      // Skip if nut location not found
            }
            const std::string &nut_loc = it->second;

            int min_cost = std::numeric_limits<int>::max();
            std::string best_spanner;

            for (const auto &spanner_info : available_spanners) {
                const std::string &spanner = std::get<0>(spanner_info);
                const std::string &s_loc = std::get<1>(spanner_info);
                bool is_carried = std::get<2>(spanner_info);

                if (used_spanners.count(spanner) > 0) {
                    continue;
                }

                int cost;
                if (is_carried) {
                    auto dist_it = shortest_paths_[man_location].find(nut_loc);
                    int distance = (dist_it != shortest_paths_[man_location].end()) ? dist_it->second : std::numeric_limits<int>::max();
                    cost = (distance == std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : distance + 1;
                } else {
                    auto dist1_it = shortest_paths_[man_location].find(s_loc);
                    auto dist2_it = shortest_paths_[s_loc].find(nut_loc);

                    int d1 = (dist1_it != shortest_paths_[man_location].end()) ? dist1_it->second : std::numeric_limits<int>::max();
                    int d2 = (dist2_it != shortest_paths_[s_loc].end()) ? dist2_it->second : std::numeric_limits<int>::max();

                    cost = (d1 == std::numeric_limits<int>::max() || d2 == std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : d1 + 1 + d2 + 1;
                }

                if (cost < min_cost) {
                    min_cost = cost;
                    best_spanner = spanner;
                }
            }

            if (!best_spanner.empty()) {
                total_cost += min_cost;
                used_spanners.insert(best_spanner);
            } else {
                total_cost += 1000000;      // Penalize for missing spanner
            }
        }

        return total_cost;
    }

private:
    Task task_;
    std::map<std::string, std::vector<std::string>> static_links_;
    std::map<std::string, std::map<std::string, int>> shortest_paths_;
};
}

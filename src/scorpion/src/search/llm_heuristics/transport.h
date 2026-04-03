#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <limits>

#include "domain_dependent_heuristic.h"

namespace llm_heuristics {
class TransportHeuristic : public DomainDependentHeuristic {
public:
    TransportHeuristic(const Task &task) {
        // Extract goal locations for each package
        for (const std::string &goal : task.goals) {
            std::stringstream ss(goal.substr(1, goal.size() - 2));     // Remove parentheses
            std::string part;
            std::vector<std::string> parts;
            while (ss >> part) {
                parts.push_back(part);
            }
            if (parts[0] == "at" && parts[1].rfind('p', 0) == 0) {     // starts with 'p'
                packageGoalLocations[parts[1]] = parts[2];
            }
        }

        // Extract static facts: roads and capacity-predecessors
        for (const std::string &fact : task.static_facts) {
            std::stringstream ss(fact.substr(1, fact.size() - 2));     // Remove parentheses
            std::string part;
            std::vector<std::string> parts;
            while (ss >> part) {
                parts.push_back(part);
            }

            if (parts[0] == "road") {
                std::string l1 = parts[1];
                std::string l2 = parts[2];
                roadMap[l1].push_back(l2);
                roadMap[l2].push_back(l1);
            } else if (parts[0] == "capacity-predecessor") {
                capacityPredecessors.insert(parts[2]);
            }
        }

        // Precompute shortest paths between all locations using BFS
        std::unordered_set<std::string> locations;
        for (const auto &pair : roadMap) {
            locations.insert(pair.first);
        }

        for (const std::string &startLoc : locations) {
            shortestPaths[startLoc] = std::unordered_map<std::string, int>();
            std::queue<std::string> q;
            q.push(startLoc);
            shortestPaths[startLoc][startLoc] = 0;
            std::unordered_set<std::string> visited;
            visited.insert(startLoc);

            while (!q.empty()) {
                std::string current = q.front();
                q.pop();

                for (const std::string &neighbor : roadMap[current]) {
                    if (visited.find(neighbor) == visited.end()) {
                        shortestPaths[startLoc][neighbor] = shortestPaths[startLoc][current] + 1;
                        visited.insert(neighbor);
                        q.push(neighbor);
                    }
                }
            }
        }
    }

    int operator()(const std::vector<std::string> &state) override {
        std::unordered_map<std::string, std::string> currentPackageLocations;
        std::unordered_map<std::string, std::string> currentPackageVehicles;
        std::unordered_map<std::string, std::string> currentVehicleLocations;
        std::unordered_map<std::string, std::string> currentVehicleCapacities;

        // Parse current state
        for (const std::string &fact : state) {
            std::stringstream ss(fact.substr(1, fact.size() - 2));     // Remove parentheses
            std::string part;
            std::vector<std::string> parts;
            while (ss >> part) {
                parts.push_back(part);
            }

            if (parts.empty()) {
                continue;
            }

            if (parts[0] == "at") {
                std::string obj = parts[1];
                std::string loc = parts[2];
                if (obj.rfind('p', 0) == 0) {     // starts with 'p'
                    currentPackageLocations[obj] = loc;
                } else if (obj.rfind('v', 0) == 0) {     // starts with 'v'
                    currentVehicleLocations[obj] = loc;
                }
            } else if (parts[0] == "in") {
                std::string package = parts[1];
                std::string vehicle = parts[2];
                currentPackageVehicles[package] = vehicle;
            } else if (parts[0] == "capacity") {
                std::string vehicle = parts[1];
                std::string capacity = parts[2];
                currentVehicleCapacities[vehicle] = capacity;
            }
        }

        int totalCost = 0;

        for (const auto &pair : packageGoalLocations) {
            const std::string &package = pair.first;
            const std::string &goalLoc = pair.second;

            if (currentPackageVehicles.count(package)) {
                // Package is in a vehicle
                std::string vehicle = currentPackageVehicles[package];
                std::string vehicleLoc = currentVehicleLocations.count(vehicle) ? currentVehicleLocations[vehicle] : "";
                if (vehicleLoc.empty()) {
                    continue;     // Vehicle location unknown, skip
                }

                // Drive from vehicle's current location to goal
                int distance = std::numeric_limits<int>::max();
                if (shortestPaths.count(vehicleLoc) && shortestPaths[vehicleLoc].count(goalLoc)) {
                    distance = shortestPaths[vehicleLoc][goalLoc];
                }

                if (distance == std::numeric_limits<int>::max()) {
                    distance = 1000;     // Penalize unreachable goals
                }
                totalCost += distance + 1;     // Drop action
            } else if (currentPackageLocations.count(package)) {
                std::string currentLoc = currentPackageLocations[package];
                if (currentLoc == goalLoc) {
                    continue;     // Already at goal
                }

                // Find best vehicle to pick up the package
                int minCost = std::numeric_limits<int>::max();
                for (const auto &vehiclePair : currentVehicleCapacities) {
                    const std::string &vehicle = vehiclePair.first;
                    const std::string &capacity = vehiclePair.second;

                    if (capacityPredecessors.count(capacity)) {
                        continue;     // Vehicle cannot pick up
                    }

                    std::string vehicleLoc = currentVehicleLocations.count(vehicle) ? currentVehicleLocations[vehicle] : "";
                    if (vehicleLoc.empty()) {
                        continue;
                    }

                    // Distance from vehicle to package's current location
                    int dist1 = std::numeric_limits<int>::max();
                    if (shortestPaths.count(vehicleLoc) && shortestPaths[vehicleLoc].count(currentLoc)) {
                        dist1 = shortestPaths[vehicleLoc][currentLoc];
                    }

                    // Distance from package's location to goal
                    int dist2 = std::numeric_limits<int>::max();
                    if (shortestPaths.count(currentLoc) && shortestPaths[currentLoc].count(goalLoc)) {
                        dist2 = shortestPaths[currentLoc][goalLoc];
                    }


                    int cost = (dist1 == std::numeric_limits<int>::max() || dist2 == std::numeric_limits<int>::max()) ? 1000 : dist1 + dist2 + 2;     // Pick-up and drop
                    if (cost < minCost) {
                        minCost = cost;
                    }
                }

                if (minCost == std::numeric_limits<int>::max()) {
                    minCost = 1000;     // Fallback cost
                }
                totalCost += minCost;
            } else {
                // Package not found in state (invalid state)
                // Do nothing, cost is unchanged.
            }
        }

        return totalCost;
    }

private:
    std::unordered_map<std::string, std::string> packageGoalLocations;
    std::unordered_set<std::string> capacityPredecessors;
    std::unordered_map<std::string, std::vector<std::string>> roadMap;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> shortestPaths;
};
}

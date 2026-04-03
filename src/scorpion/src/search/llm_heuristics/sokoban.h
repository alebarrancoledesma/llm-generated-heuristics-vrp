#include "domain_dependent_heuristic.h"
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <limits>
#include <sstream> // Make sure sstream is included

// Assuming DEAD_END is defined appropriately elsewhere, e.g.:
const int DEAD_END = std::numeric_limits<int>::max();

namespace llm_heuristics {
class SokobanHeuristic : public DomainDependentHeuristic {
public:
    SokobanHeuristic(const Task &task) {
        // Extract goal locations for each box
        for (const std::string &goal : task.goals) {
            if (goal.rfind("(at ", 0) == 0) {
                // Content is "at boxN locM"
                std::string content = goal.substr(1, goal.length() - 2);
                std::stringstream ss(content);
                std::string predicate, box, loc;
                ss >> predicate >> box >> loc; // Read "at", "boxN", "locM"
                if (predicate == "at" && !box.empty() && !loc.empty() && box.rfind("box", 0) == 0) { // Check if it's a box goal
                    goal_locations[box] = loc;
                }
            }
        }

        // Build adjacency list from static facts
        for (const std::string &fact : task.static_facts) {
            if (fact.rfind("(adjacent ", 0) == 0) {
                // Content is "adjacent l1 l2"
                std::string content = fact.substr(1, fact.length() - 2);
                std::stringstream ss(content);
                std::string predicate, l1, l2;
                ss >> predicate >> l1 >> l2; // Read "adjacent", "l1", "l2"
                if (predicate == "adjacent" && !l1.empty() && !l2.empty()) {
                    adjacency[l1].push_back(l2);
                    // Assuming adjacency is symmetric, add the other direction too if necessary
                    // adjacency[l2].push_back(l1);
                }
            }
        }

        // --- BFS Precomputation (Seems okay, but ensure all reachable locations are added) ---
        std::unordered_set<std::string> locations;
        // Collect all unique locations from adjacency and goals
        for (const auto &pair : adjacency) {
            locations.insert(pair.first);
            for (const std::string &neighbor : pair.second) {
                locations.insert(neighbor);
            }
        }
        for (const auto &pair : goal_locations) {
            // Make sure goal locations themselves are in the set if they might be isolated
            locations.insert(pair.second);
            // Also consider adding initial box/robot locations if they might not be in adjacency keys/values
        }
        // If the task definition provides a list of all locations, use that instead for robustness.


        for (const std::string &loc : locations) {
            std::unordered_map<std::string, int> visited;
            std::queue<std::string> queue;

            visited[loc] = 0;
            queue.push(loc);
            distances[loc][loc] = 0; // Distance to self is 0

            while (!queue.empty()) {
                std::string current = queue.front();
                queue.pop();
                int current_dist = visited[current];

                // Check if current location exists in adjacency list before iterating
                if (adjacency.count(current)) {
                    for (const std::string &neighbor : adjacency.at(current)) {
                        if (visited.find(neighbor) == visited.end() || current_dist + 1 < visited[neighbor]) {
                            visited[neighbor] = current_dist + 1;
                            distances[loc][neighbor] = current_dist + 1; // Store distance
                            queue.push(neighbor);
                        }
                    }
                }
            }
        }
    }

    // --- CORRECTED operator() ---
    virtual int operator()(const std::vector<std::string> &state) override {
        // Extract robot's current location
        std::string robot_loc;
        for (const std::string &fact : state) {
            if (fact.rfind("(at-robot ", 0) == 0) {
                // Content is "at-robot loc"
                std::string content = fact.substr(1, fact.length() - 2);
                std::stringstream ss(content);
                std::string predicate;
                ss >> predicate >> robot_loc; // Read "at-robot", "loc"
                if (predicate == "at-robot" && !robot_loc.empty()) {
                    break;
                } else {
                    robot_loc.clear(); // Ensure it's empty if parsing failed
                }
            }
        }
        if (robot_loc.empty()) {
            // std::cerr << "Warning: Robot location not found in state." << std::endl; // Optional debug
            return DEAD_END; // Robot has no location (invalid state)
        }

        // Extract current box locations
        std::unordered_map<std::string, std::string> current_boxes;
        for (const std::string &fact : state) {
            if (fact.rfind("(at ", 0) == 0) {
                // Corrected Parsing: Parse the full content within parentheses
                std::string content = fact.substr(1, fact.length() - 2); // "at boxN locM"
                std::stringstream ss(content);
                std::string predicate, arg1, arg2;
                ss >> predicate >> arg1 >> arg2; // Read "at", "boxN", "locM"

                // Check if it's a box location predicate
                if (predicate == "at" && !arg1.empty() && arg1.rfind("box", 0) == 0 && !arg2.empty()) {
                    current_boxes[arg1] = arg2; // Store box -> location mapping
                }
            }
        }

        int sum_box_dist = 0;
        int min_robot_dist = DEAD_END; // Initialize with infinity/max value
        bool any_box_not_at_goal = false;

        for (const auto &goal_pair : goal_locations) {
            const std::string &box = goal_pair.first;
            const std::string &goal_loc = goal_pair.second;

            // Find the current location of the box
            const auto box_it = current_boxes.find(box);
            if (box_it == current_boxes.end()) {
                // std::cerr << "Warning: Box " << box << " not found in current state." << std::endl; // Optional debug
                return DEAD_END; // Box mentioned in goal is not in state -> invalid/unsolvable
            }
            const std::string &current_loc = box_it->second;

            if (current_loc == goal_loc) {
                continue; // Box is already at its goal
            }

            any_box_not_at_goal = true; // Found at least one box not at its goal

            // --- Calculate box to goal distance ---
            int box_to_goal = DEAD_END; // Default to infinity
            const auto dist_current_it = distances.find(current_loc);
            if (dist_current_it != distances.end()) {
                const auto dist_goal_it = dist_current_it->second.find(goal_loc);
                if (dist_goal_it != dist_current_it->second.end()) {
                    box_to_goal = dist_goal_it->second;
                }
            }

            if (box_to_goal == DEAD_END) {
                // std::cerr << "Warning: Box " << box << " cannot reach goal " << goal_loc << " from " << current_loc << std::endl; // Optional debug
                return DEAD_END; // Box cannot reach goal -> unsolvable state
            }
            sum_box_dist += box_to_goal;

            // --- Calculate robot to box distance ---
            int robot_to_box = DEAD_END; // Default to infinity
            const auto dist_robot_it = distances.find(robot_loc);
            if (dist_robot_it != distances.end()) {
                const auto dist_box_curr_it = dist_robot_it->second.find(current_loc);
                if (dist_box_curr_it != dist_robot_it->second.end()) {
                    robot_to_box = dist_box_curr_it->second;
                }
            }

            // Update minimum robot-to-box distance *only* if the robot can reach this box
            if (robot_to_box != DEAD_END && robot_to_box < min_robot_dist) {
                min_robot_dist = robot_to_box;
            }
        }

        if (!any_box_not_at_goal) {
            // If the loop finished and no box was found out of place
            return 0; // All boxes are at their goals
        }

        // If there were boxes not at goals, but the robot couldn't reach any of them
        if (min_robot_dist == DEAD_END) {
            // std::cerr << "Warning: Robot at " << robot_loc << " cannot reach any misplaced box." << std::endl; // Optional debug
            return DEAD_END; // Robot cannot reach any box that needs moving
        }

        return sum_box_dist + min_robot_dist;
    }

private:
    std::unordered_map<std::string, std::string> goal_locations;
    std::unordered_map<std::string, std::vector<std::string>> adjacency;
    // Stores shortest path distances: distances[from_loc][to_loc] = dist
    std::unordered_map<std::string, std::unordered_map<std::string, int>> distances;
};
} // namespace llm_heuristics

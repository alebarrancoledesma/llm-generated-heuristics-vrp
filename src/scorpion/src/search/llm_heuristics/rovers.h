#include "domain_dependent_heuristic.h" // Provides DomainDependentHeuristic base class

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map> // Use map for rover_graphs if waypoint order matters, otherwise unordered_map
#include <set> // Use set for visible_to_lander, supports, etc. if order matters, otherwise unordered_set
#include <queue>
#include <sstream>
#include <limits> // Required for numeric_limits
#include <cmath> // Required for std::min
#include <algorithm> // Required for std::min, std::find

namespace llm_heuristics {
// Helper function to split strings like "(predicate arg1 arg2)"
std::vector<std::string> get_parts_rovers(const std::string &fact) {
    std::vector<std::string> parts;
    if (fact.length() < 2 || fact.front() != '(' || fact.back() != ')') {
        // Handle malformed fact string if necessary, or return empty
        return parts;
    }
    std::string content = fact.substr(1, fact.length() - 2);
    std::stringstream ss(content);
    std::string part;
    while (ss >> part) {
        parts.push_back(part);
    }
    return parts;
}


class RoversHeuristic : public DomainDependentHeuristic {
private:
    // --- Static Information Members ---
    std::vector<std::string> goals_;
    std::string lander_waypoint_;
    std::unordered_set<std::string> visible_to_lander_;

    // Map: Rover -> Map: Waypoint -> Set: Reachable Waypoints
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_set<std::string>>> rover_graphs_;

    // Map: Rover -> Map: EquipmentType -> bool (true if equipped)
    std::unordered_map<std::string, std::unordered_map<std::string, bool>> rover_equipment_;

    // Map: Camera -> Map: Property -> Value (string or set<string>)
    struct CameraDetails {
        std::string on_rover;
        std::unordered_set<std::string> supports;
        std::string calibration_target;
    };
    std::unordered_map<std::string, CameraDetails> camera_info_;

    // Map: Objective -> Set: Waypoints from which objective is visible
    std::unordered_map<std::string, std::unordered_set<std::string>> objective_visible_waypoints_; // Redundant? visible_from_ seems identical
    std::unordered_map<std::string, std::unordered_set<std::string>> visible_from_;

    // Map: Store -> Rover owning the store
    std::unordered_map<std::string, std::string> store_to_rover_;

    // Map: Camera -> Calibration Objective
    std::unordered_map<std::string, std::string> calibration_targets_; // Redundant? Stored in camera_info_

    static constexpr int INFINITY_COST = std::numeric_limits<int>::max();

    // --- Helper Methods ---

    // BFS to find shortest path distance for a rover
    int get_distance(const std::string &rover, const std::string &start, const std::string &end) const {
        if (start == end) {
            return 0;
        }

        // Check if rover exists in the graph data
        auto rover_graph_it = rover_graphs_.find(rover);
        if (rover_graph_it == rover_graphs_.end()) {
            return INFINITY_COST; // Rover has no known traversals
        }
        const auto &graph = rover_graph_it->second;

        // Check if start node exists in this rover's graph
        if (graph.find(start) == graph.end() && start != end) {
            // If start node isn't in the graph, it can't reach anything unless it's already at the end
            // Note: A waypoint might exist but have no outgoing edges, BFS handles this.
            // We only return infinity if the start node isn't part of the graph *at all*.
            // However, the graph only stores nodes with *outgoing* edges. A node might be reachable but have no exits.
            // BFS correctly handles this by not finding a path if start exists but can't reach end.
            // Let BFS handle reachability checks.
        }


        std::queue<std::pair<std::string, int>> queue;
        std::unordered_set<std::string> visited;

        queue.push({start, 0});
        visited.insert(start);

        while (!queue.empty()) {
            std::string current_node = queue.front().first;
            int current_dist = queue.front().second;
            queue.pop();

            if (current_node == end) {
                return current_dist;
            }

            // Find neighbors in the specific rover's graph
            auto node_it = graph.find(current_node);
            if (node_it != graph.end()) {
                for (const std::string &neighbor : node_it->second) {
                    if (visited.find(neighbor) == visited.end()) {
                        visited.insert(neighbor);
                        queue.push({neighbor, current_dist + 1});
                    }
                }
            }
        }

        return INFINITY_COST; // No path found
    }


public:
    // Constructor taking the Task object
    explicit RoversHeuristic(const Task &task) : goals_(task.goals) {
        // Process static facts
        for (const std::string &fact : task.static_facts) {
            std::vector<std::string> parts = get_parts_rovers(fact);
            if (parts.empty())
                continue;                // Skip malformed facts

            const std::string &predicate = parts[0];

            if (predicate == "at_lander" && parts.size() == 3) {
                lander_waypoint_ = parts[2];
            } else if (predicate == "visible" && parts.size() == 3) {
                const std::string &from_wp = parts[1];
                const std::string &to_wp = parts[2];
                // Check lander_waypoint_ is already set before using it
                if (!lander_waypoint_.empty() && to_wp == lander_waypoint_) {
                    visible_to_lander_.insert(from_wp);
                }
            } else if (predicate == "can_traverse" && parts.size() == 4) {
                const std::string &rover = parts[1];
                const std::string &from_wp = parts[2];
                const std::string &to_wp = parts[3];
                rover_graphs_[rover][from_wp].insert(to_wp);
            } else if (predicate == "equipped_for_soil_analysis" && parts.size() == 2) {
                rover_equipment_[parts[1]]["soil"] = true;
            } else if (predicate == "equipped_for_rock_analysis" && parts.size() == 2) {
                rover_equipment_[parts[1]]["rock"] = true;
            } else if (predicate == "equipped_for_imaging" && parts.size() == 2) {
                rover_equipment_[parts[1]]["imaging"] = true;
            } else if (predicate == "on_board" && parts.size() == 3) {
                camera_info_[parts[1]].on_rover = parts[2];
            } else if (predicate == "supports" && parts.size() == 3) {
                camera_info_[parts[1]].supports.insert(parts[2]);
            } else if (predicate == "calibration_target" && parts.size() == 3) {
                camera_info_[parts[1]].calibration_target = parts[2];
                calibration_targets_[parts[1]] = parts[2]; // Store separately if needed, though redundant
            } else if (predicate == "visible_from" && parts.size() == 3) {
                const std::string &obj = parts[1];
                const std::string &wp = parts[2];
                objective_visible_waypoints_[obj].insert(wp); // Keep both if Python logic used both?
                visible_from_[obj].insert(wp);
            } else if (predicate == "store_of" && parts.size() == 3) {
                store_to_rover_[parts[1]] = parts[2];
            }
            // Handle other static predicates if necessary
        }
        // Ensure lander visibility is processed correctly if defined after 'visible' facts
        if (!lander_waypoint_.empty()) {
            for (const std::string &fact : task.static_facts) {
                std::vector<std::string> parts = get_parts_rovers(fact);
                if (parts.size() == 3 && parts[0] == "visible") {
                    const std::string &from_wp = parts[1];
                    const std::string &to_wp = parts[2];
                    if (to_wp == lander_waypoint_) {
                        visible_to_lander_.insert(from_wp);
                    }
                }
            }
        }
    }

    // The heuristic evaluation function (operator())
    int operator()(const std::vector<std::string> &state) override {
        int total_cost = 0;
        constexpr int FALLBACK_COST = 10; // Default cost if exact plan unclear

        // --- Parse Current State ---
        std::unordered_map<std::string, std::string> current_rovers_pos;
        std::unordered_map<std::string, std::unordered_set<std::string>> have_soil; // Rover -> Set<Waypoint>
        std::unordered_map<std::string, std::unordered_set<std::string>> have_rock; // Rover -> Set<Waypoint>
        // Rover -> Set<Pair<Objective, Mode>> - Need a hash for pair or use map
        using ImageKey = std::pair<std::string, std::string>;
        struct ImageKeyHash {
            std::size_t operator()(const ImageKey &k) const {
                return std::hash<std::string>()(k.first) ^ (std::hash<std::string>()(k.second) << 1);
            }
        };
        std::unordered_map<std::string, std::unordered_set<ImageKey, ImageKeyHash>> have_image;

        std::unordered_map<std::string, std::unordered_set<std::string>> calibrated; // Rover -> Set<Camera>
        std::unordered_map<std::string, std::string> stores_status; // Rover -> "empty" or "full"

        // Use a set for faster goal checking
        std::unordered_set<std::string> current_state_facts(state.begin(), state.end());

        for (const std::string &fact : state) {
            std::vector<std::string> parts = get_parts_rovers(fact);
            if (parts.empty())
                continue;
            const std::string &predicate = parts[0];

            // Note: Python code handles two 'at' formats. Combining them.
            if (predicate == "at" && parts.size() >= 4) { // e.g., (at rover waypoint) or (at thing type location)
                // Assuming rover location is always (at rover rover waypoint) or (at rover waypoint)
                // Let's stick to the Python version's specific checks
                if (parts.size() == 4 && parts[2] == "rover") { // (at rover rover waypoint) - Unlikely based on PDDL? Let's assume (at rover waypoint) is the format
                    current_rovers_pos[parts[1]] = parts[3];
                } else if (parts.size() == 4 && parts[2] == "waypoint") { // (at rover waypoint) - More standard PDDL state?
                    current_rovers_pos[parts[1]] = parts[3];
                } else if (parts.size() == 3 && predicate == "at") { // Simplified (at rover waypoint) maybe?
                    // Requires knowing which arg is rover vs waypoint. Assume parts[1]=rover, parts[2]=wp.
                    // This needs clarification based on actual PDDL state format.
                    // Let's assume the 4-part version is correct based on Python 'at ... rover'/'at ... waypoint'.
                }
            } else if (predicate == "have_soil_analysis" && parts.size() == 3) {
                have_soil[parts[1]].insert(parts[2]);
            } else if (predicate == "have_rock_analysis" && parts.size() == 3) {
                have_rock[parts[1]].insert(parts[2]);
            } else if (predicate == "have_image" && parts.size() == 4) {
                have_image[parts[1]].insert({parts[2], parts[3]});
            } else if (predicate == "calibrated" && parts.size() == 3) {
                // Assuming format (calibrated camera rover ...) - Python code uses parts[1]=cam, parts[2]=rover? No, parts[1]=camera, parts[2]=rover used in 'calibrated[parts[2]].add(parts[1])'
                // Let's match Python: rover -> set(cameras)
                calibrated[parts[2]].insert(parts[1]);
            } else if (predicate == "empty" && parts.size() == 2) {
                auto it = store_to_rover_.find(parts[1]);
                if (it != store_to_rover_.end()) {
                    stores_status[it->second] = "empty";
                }
            } else if (predicate == "full" && parts.size() == 2) {
                auto it = store_to_rover_.find(parts[1]);
                if (it != store_to_rover_.end()) {
                    stores_status[it->second] = "full";
                }
            }
        }
        // Ensure all rovers known from stores have an initial 'empty' status if not 'full'
        for (const auto &pair : store_to_rover_) {
            const std::string &rover = pair.second;
            if (stores_status.find(rover) == stores_status.end()) {
                stores_status[rover] = "empty";
            }
        }


        // --- Evaluate Goals ---
        for (const std::string &goal : goals_) {
            // Check if goal is already satisfied
            if (current_state_facts.count(goal)) {
                continue;
            }

            std::vector<std::string> parts = get_parts_rovers(goal);
            if (parts.empty())
                continue;                // Skip malformed goals
            const std::string &predicate = parts[0];

            // --- Communicated Soil Data Goal ---
            if (predicate == "communicated_soil_data" && parts.size() == 2) {
                const std::string &wp = parts[1];
                int min_cost = INFINITY_COST;

                // Option 1: Rover already has the analysis
                for (const auto &pair : have_soil) {
                    const std::string &rover = pair.first;
                    const auto &samples = pair.second;
                    if (samples.count(wp)) {
                        auto pos_it = current_rovers_pos.find(rover);
                        if (pos_it != current_rovers_pos.end()) {
                            const std::string &current_pos = pos_it->second;
                            int min_dist_comm = INFINITY_COST;
                            for (const std::string &vwp : visible_to_lander_) {
                                int dist = get_distance(rover, current_pos, vwp);
                                if (dist != INFINITY_COST) {
                                    min_dist_comm = std::min(min_dist_comm, dist);
                                }
                            }
                            if (min_dist_comm != INFINITY_COST) {
                                min_cost = std::min(min_cost, min_dist_comm + 1); // Move + Communicate
                            }
                        }
                    }
                }

                if (min_cost != INFINITY_COST) {
                    total_cost += min_cost;
                    continue; // Found cheapest way via existing analysis
                }

                // Option 2: Need to collect, then communicate
                std::string soil_sample_fact = "(at_soil_sample " + wp + ")";
                // Check if the sample exists at the location (assumed static or part of state?)
                // Python checks `f'(at_soil_sample {wp})' in state`. Assuming it's a dynamic fact.
                if (current_state_facts.count(soil_sample_fact)) {
                    int min_cost_collect = INFINITY_COST;
                    for (const auto &equip_pair : rover_equipment_) {
                        const std::string &rover = equip_pair.first;
                        const auto &equipment = equip_pair.second;
                        auto equip_it = equipment.find("soil");
                        auto store_it = stores_status.find(rover);

                        if (equip_it != equipment.end() && equip_it->second &&
                            store_it != stores_status.end() && store_it->second == "empty") {
                            auto pos_it = current_rovers_pos.find(rover);
                            if (pos_it != current_rovers_pos.end()) {
                                const std::string &current_pos = pos_it->second;
                                int dist_to_wp = get_distance(rover, current_pos, wp);
                                if (dist_to_wp != INFINITY_COST) {
                                    int min_dist_comm_from_wp = INFINITY_COST;
                                    for (const std::string &vwp : visible_to_lander_) {
                                        int dist = get_distance(rover, wp, vwp);
                                        if (dist != INFINITY_COST) {
                                            min_dist_comm_from_wp = std::min(min_dist_comm_from_wp, dist);
                                        }
                                    }
                                    if (min_dist_comm_from_wp != INFINITY_COST) {
                                        // Cost: Move to WP + Sample (1) + Move to LanderVisible + Communicate (1)
                                        int cost = dist_to_wp + 1 + min_dist_comm_from_wp + 1;
                                        min_cost_collect = std::min(min_cost_collect, cost);
                                    }
                                }
                            }
                        }
                    }
                    if (min_cost_collect != INFINITY_COST) {
                        total_cost += min_cost_collect;
                    } else {
                        // Cannot find a way to collect and communicate this specific sample
                        // Either return DEAD_END or use fallback
                        // total_cost += FALLBACK_COST; // Python's behavior
                        return DEAD_END;  // Stricter interpretation: goal unreachable
                    }
                } else {
                    // Soil sample is not at the required waypoint in the current state
                    // total_cost += FALLBACK_COST; // Python's behavior
                    return DEAD_END;  // Goal unreachable
                }
            }
            // --- Communicated Rock Data Goal --- (Similar logic to Soil)
            else if (predicate == "communicated_rock_data" && parts.size() == 2) {
                const std::string &wp = parts[1];
                int min_cost = INFINITY_COST;

                // Option 1: Rover already has the analysis
                for (const auto &pair : have_rock) {
                    const std::string &rover = pair.first;
                    const auto &samples = pair.second;
                    if (samples.count(wp)) {
                        auto pos_it = current_rovers_pos.find(rover);
                        if (pos_it != current_rovers_pos.end()) {
                            const std::string &current_pos = pos_it->second;
                            int min_dist_comm = INFINITY_COST;
                            for (const std::string &vwp : visible_to_lander_) {
                                int dist = get_distance(rover, current_pos, vwp);
                                if (dist != INFINITY_COST) {
                                    min_dist_comm = std::min(min_dist_comm, dist);
                                }
                            }
                            if (min_dist_comm != INFINITY_COST) {
                                min_cost = std::min(min_cost, min_dist_comm + 1); // Move + Communicate
                            }
                        }
                    }
                }

                if (min_cost != INFINITY_COST) {
                    total_cost += min_cost;
                    continue;
                }

                // Option 2: Need to collect, then communicate
                std::string rock_sample_fact = "(at_rock_sample " + wp + ")";
                if (current_state_facts.count(rock_sample_fact)) {
                    int min_cost_collect = INFINITY_COST;
                    for (const auto &equip_pair : rover_equipment_) {
                        const std::string &rover = equip_pair.first;
                        const auto &equipment = equip_pair.second;
                        auto equip_it = equipment.find("rock");
                        auto store_it = stores_status.find(rover);

                        if (equip_it != equipment.end() && equip_it->second &&
                            store_it != stores_status.end() && store_it->second == "empty") {
                            auto pos_it = current_rovers_pos.find(rover);
                            if (pos_it != current_rovers_pos.end()) {
                                const std::string &current_pos = pos_it->second;
                                int dist_to_wp = get_distance(rover, current_pos, wp);
                                if (dist_to_wp != INFINITY_COST) {
                                    int min_dist_comm_from_wp = INFINITY_COST;
                                    for (const std::string &vwp : visible_to_lander_) {
                                        int dist = get_distance(rover, wp, vwp);
                                        if (dist != INFINITY_COST) {
                                            min_dist_comm_from_wp = std::min(min_dist_comm_from_wp, dist);
                                        }
                                    }
                                    if (min_dist_comm_from_wp != INFINITY_COST) {
                                        int cost = dist_to_wp + 1 + min_dist_comm_from_wp + 1;
                                        min_cost_collect = std::min(min_cost_collect, cost);
                                    }
                                }
                            }
                        }
                    }
                    if (min_cost_collect != INFINITY_COST) {
                        total_cost += min_cost_collect;
                    } else {
                        // total_cost += FALLBACK_COST;
                        return DEAD_END;
                    }
                } else {
                    // total_cost += FALLBACK_COST;
                    return DEAD_END;
                }
            }
            // --- Communicated Image Data Goal ---
            else if (predicate == "communicated_image_data" && parts.size() == 3) {
                const std::string &obj = parts[1];
                const std::string &mode = parts[2];
                ImageKey image_key = {obj, mode};
                int min_cost = INFINITY_COST;

                // Option 1: Rover already has the image
                for (const auto &pair : have_image) {
                    const std::string &rover = pair.first;
                    const auto &images = pair.second;
                    if (images.count(image_key)) {
                        auto pos_it = current_rovers_pos.find(rover);
                        if (pos_it != current_rovers_pos.end()) {
                            const std::string &current_pos = pos_it->second;
                            int min_dist_comm = INFINITY_COST;
                            for (const std::string &vwp : visible_to_lander_) {
                                int dist = get_distance(rover, current_pos, vwp);
                                if (dist != INFINITY_COST) {
                                    min_dist_comm = std::min(min_dist_comm, dist);
                                }
                            }
                            if (min_dist_comm != INFINITY_COST) {
                                min_cost = std::min(min_cost, min_dist_comm + 1); // Move + Communicate
                            }
                        }
                    }
                }

                if (min_cost != INFINITY_COST) {
                    total_cost += min_cost;
                    continue;
                }

                // Option 2: Need to take image, then communicate
                int min_cost_take = INFINITY_COST;

                for (const auto &cam_pair : camera_info_) {
                    const std::string &cam = cam_pair.first;
                    const CameraDetails &details = cam_pair.second;

                    // Check if camera supports the required mode
                    if (details.supports.find(mode) == details.supports.end()) {
                        continue;
                    }

                    const std::string &rover = details.on_rover;
                    if (rover.empty())
                        continue;                // Camera not on any rover

                    // Check if rover is equipped for imaging
                    auto equip_it = rover_equipment_.find(rover);
                    if (equip_it == rover_equipment_.end() ||
                        equip_it->second.find("imaging") == equip_it->second.end() ||
                        !equip_it->second.at("imaging")) {
                        continue;
                    }

                    // Get calibration target and waypoints
                    const std::string &cal_obj = details.calibration_target;
                    if (cal_obj.empty())
                        continue;                  // Camera needs calibration target

                    auto cal_wps_it = visible_from_.find(cal_obj);
                    // auto cal_wps_it = objective_visible_waypoints_.find(cal_obj); // Use same as python?
                    const std::unordered_set<std::string> &cal_wps = (cal_wps_it != visible_from_.end())
                                                                    ? cal_wps_it->second
                                                                    : std::unordered_set<std::string>();


                    // Get image objective waypoints
                    auto img_wps_it = visible_from_.find(obj);
                    // auto img_wps_it = objective_visible_waypoints_.find(obj);
                    if (img_wps_it == visible_from_.end() || img_wps_it->second.empty()) {
                        continue; // Objective not visible from anywhere
                    }
                    const std::unordered_set<std::string> &img_wps = img_wps_it->second;


                    // Get rover's current position
                    auto pos_it = current_rovers_pos.find(rover);
                    if (pos_it == current_rovers_pos.end()) {
                        continue; // Rover's position unknown
                    }
                    const std::string &current_pos = pos_it->second;

                    // Check if camera is calibrated for this rover
                    bool is_calibrated = false;
                    auto calib_rover_it = calibrated.find(rover);
                    if (calib_rover_it != calibrated.end()) {
                        is_calibrated = calib_rover_it->second.count(cam);
                    }

                    int cal_cost = 0;
                    std::string pos_after_cal = current_pos; // Position after potential calibration step

                    if (!is_calibrated) {
                        if (cal_wps.empty())
                            continue;                   // Cannot calibrate

                        int min_dist_cal = INFINITY_COST;
                        std::string best_cal_wp = "";

                        for (const std::string &wp : cal_wps) {
                            int dist = get_distance(rover, current_pos, wp);
                            if (dist != INFINITY_COST) {
                                if (dist < min_dist_cal) {
                                    min_dist_cal = dist;
                                    best_cal_wp = wp;
                                }
                            }
                        }

                        if (min_dist_cal == INFINITY_COST) {
                            continue;  // Cannot reach any calibration waypoint
                        }
                        cal_cost = min_dist_cal + 1;  // Move + Calibrate
                        pos_after_cal = best_cal_wp;
                    }

                    // Calculate cost to take image
                    int min_dist_img = INFINITY_COST;
                    std::string best_img_wp = "";

                    for (const std::string &wp : img_wps) {
                        int dist = get_distance(rover, pos_after_cal, wp);
                        if (dist != INFINITY_COST) {
                            if (dist < min_dist_img) {
                                min_dist_img = dist;
                                best_img_wp = wp;
                            }
                        }
                    }

                    if (min_dist_img == INFINITY_COST) {
                        continue; // Cannot reach any imaging waypoint
                    }
                    int img_cost = min_dist_img + 1; // Move + TakeImage
                    std::string pos_after_img = best_img_wp;


                    // Calculate cost to communicate
                    int min_dist_comm = INFINITY_COST;
                    for (const std::string &vwp : visible_to_lander_) {
                        int dist = get_distance(rover, pos_after_img, vwp);
                        if (dist != INFINITY_COST) {
                            min_dist_comm = std::min(min_dist_comm, dist);
                        }
                    }

                    if (min_dist_comm == INFINITY_COST) {
                        continue; // Cannot reach lander communication spot
                    }
                    int comm_cost = min_dist_comm + 1; // Move + Communicate


                    int total_cost_cam = cal_cost + img_cost + comm_cost;
                    min_cost_take = std::min(min_cost_take, total_cost_cam);
                } // End loop over cameras

                if (min_cost_take != INFINITY_COST) {
                    total_cost += min_cost_take;
                } else {
                    // Cannot find a way to take and communicate this image
                    // total_cost += FALLBACK_COST; // Python's behavior
                    return DEAD_END; // Stricter interpretation: goal unreachable
                }
            } // End image goal
            else {
                // Handle unknown goal predicate type? Maybe add fallback or return DEAD_END.
                // For now, ignore unknown goal types.
            }
        } // End loop over goals

        return total_cost;
    }

    // Default destructor is fine since we use standard containers
    ~RoversHeuristic() override = default;
};
} // namespace llm_heuristics

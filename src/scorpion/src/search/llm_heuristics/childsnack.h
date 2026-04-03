#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include "domain_dependent_heuristic.h"

namespace llm_heuristics {
class ChildsnackHeuristic : public DomainDependentHeuristic {
public:
    ChildsnackHeuristic(const Task &task) : static_facts(task.static_facts) {
        for (const std::string &fact : static_facts) {
            std::string fact_content = fact.substr(1, fact.size() - 2); // Remove parentheses
            std::stringstream ss(fact_content);
            std::string part;
            std::vector<std::string> parts;
            while (ss >> part) {
                parts.push_back(part);
            }

            if (!parts.empty() && parts[0] == "allergic_gluten") {
                if (parts.size() > 1) {
                    allergic_children.insert(parts[1]);
                }
            } else if (!parts.empty() && parts[0] == "not_allergic_gluten") {
                if (parts.size() > 1) {
                    non_allergic_children.insert(parts[1]);
                }
            }
        }
    }

    int operator()(const std::vector<std::string> &state) override {
        std::unordered_set<std::string> served;
        int unserved_allergic = 0;
        int unserved_non_allergic = 0;

        // Identify served children
        for (const std::string &fact : state) {
            if (fact.rfind("(served ", 0) == 0) {
                std::string fact_content = fact.substr(1, fact.size() - 2); // Remove parentheses
                std::stringstream ss(fact_content);
                std::string part;
                std::vector<std::string> parts;
                while (ss >> part) {
                    parts.push_back(part);
                }
                if (parts.size() > 1) {
                    served.insert(parts[1]);
                }
            }
        }

        // Count unserved allergic and non-allergic children
        for (const std::string &child : allergic_children) {
            if (served.find(child) == served.end()) {
                unserved_allergic++;
            }
        }
        for (const std::string &child : non_allergic_children) {
            if (served.find(child) == served.end()) {
                unserved_non_allergic++;
            }
        }
        int total_unserved = unserved_allergic + unserved_non_allergic;

        // Count existing sandwiches
        int gluten_free = 0;
        int total_sandwiches = 0;
        int kitchen_sandwiches = 0;

        for (const std::string &fact : state) {
            if (fact.rfind("(no_gluten_sandwich ", 0) == 0) {
                gluten_free++;
            }
            if (fact.rfind("(ontray ", 0) == 0 || fact.rfind("(at_kitchen_sandwich ", 0) == 0) {
                total_sandwiches++;
            }
            if (fact.rfind("(at_kitchen_sandwich ", 0) == 0) {
                kitchen_sandwiches++;
            }
        }

        int regular = total_sandwiches - gluten_free;

        // Calculate needed sandwiches
        int needed_gluten = std::max(0, unserved_allergic - gluten_free);
        int needed_regular = std::max(0, unserved_non_allergic - regular);
        int make_cost = (needed_gluten + needed_regular) * 2;

        // Existing sandwiches needing placement
        int place_cost = kitchen_sandwiches;

        // Determine tray movement needed
        std::unordered_set<std::string> locations;
        std::unordered_set<std::string> tray_locations;

        for (const std::string &fact : state) {
            if (fact.rfind("(waiting ", 0) == 0) {
                std::string fact_content = fact.substr(1, fact.size() - 2); // Remove parentheses
                std::stringstream ss(fact_content);
                std::string part;
                std::vector<std::string> parts;
                while (ss >> part) {
                    parts.push_back(part);
                }
                if (parts.size() > 2 && served.find(parts[1]) == served.end()) {
                    locations.insert(parts[2]);
                }
            }
            if (fact.rfind("(at tray", 0) == 0) {
                std::string fact_content = fact.substr(1, fact.size() - 2); // Remove parentheses
                std::stringstream ss(fact_content);
                std::string part;
                std::vector<std::string> parts;
                while (ss >> part) {
                    parts.push_back(part);
                }
                if (parts.size() > 2) {
                    tray_locations.insert(parts[2]);
                }
            }
        }

        int move_cost = 0;
        for (const std::string &loc : locations) {
            if (tray_locations.find(loc) == tray_locations.end()) {
                move_cost++;
            }
        }

        // Total heuristic
        return make_cost + place_cost + move_cost + total_unserved;
    }

private:
    std::vector<std::string> static_facts;
    std::unordered_set<std::string> allergic_children;
    std::unordered_set<std::string> non_allergic_children;
};
} // namespace llm_heuristics

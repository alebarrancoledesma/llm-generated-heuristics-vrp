#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include "domain_dependent_heuristic.h"

namespace llm_heuristics {
class BlocksworldHeuristic : public DomainDependentHeuristic {
public:
    BlocksworldHeuristic(const Task &task) {
        for (const std::string &goal : task.goals) {
            std::string goal_copy = goal;
            goal_copy.erase(0, 1);     // Remove "("
            goal_copy.erase(goal_copy.size() - 1);     // Remove ")"

            std::stringstream ss(goal_copy);
            std::string predicate;
            ss >> predicate;

            if (predicate == "on") {
                std::string block, parent;
                ss >> block >> parent;
                goal_parent_[block] = parent;
            } else if (predicate == "on-table") {
                std::string block;
                ss >> block;
                goal_parent_[block] = "table";
            }
        }
    }

    int operator()(const std::vector<std::string> &state) override {
        std::unordered_map<std::string, std::string> current_parent;
        std::unordered_map<std::string, std::vector<std::string>> current_children;
        std::string held_block;
        bool is_holding = false;

        // Parse current state
        for (const std::string &fact : state) {
            std::string fact_copy = fact;
            fact_copy.erase(0, 1);     // Remove "("
            fact_copy.erase(fact_copy.size() - 1);     // Remove ")"

            std::stringstream ss(fact_copy);
            std::string predicate;
            ss >> predicate;

            if (predicate == "on") {
                std::string child, parent;
                ss >> child >> parent;
                current_parent[child] = parent;
                current_children[parent].push_back(child);
            } else if (predicate == "on-table") {
                std::string block;
                ss >> block;
                current_parent[block] = "table";
            } else if (predicate == "holding") {
                ss >> held_block;
                is_holding = true;
            }
        }

        // Calculate cost
        int cost = 0;

        // Check held block
        if (is_holding && goal_parent_.count(held_block) > 0) {
            std::string current_pos = "held";
            std::string goal_pos = goal_parent_[held_block];

            if ((goal_pos == "table" && current_pos != "table") ||
                (goal_pos != "table" && current_pos != goal_pos)) {
                cost += 1;
            }
        }

        // Process each block in the goal
        for (const auto &pair : goal_parent_) {
            const std::string &block = pair.first;

            if (is_holding && block == held_block) {
                continue;     // Already handled
            }

            std::string current_parent_block = "table";
            if (current_parent.count(block) > 0) {
                current_parent_block = current_parent[block];
            }
            std::string goal_parent_block = goal_parent_[block];

            if (current_parent_block != goal_parent_block) {
                // Calculate number of blocks above the current block
                std::function<int(const std::string &)> count_above =
                    [&](const std::string &x) {
                        int cnt = 0;
                        std::vector<std::string> stack = {x};
                        while (!stack.empty()) {
                            std::string current = stack.back();
                            stack.pop_back();
                            if (current_children.count(current) > 0) {
                                for (const std::string &child : current_children[current]) {
                                    cnt += 1;
                                    stack.push_back(child);
                                }
                            }
                        }
                        return cnt;
                    };

                int above = count_above(block);
                cost += 2 * (above + 1);
            }
        }

        return cost;
    }

private:
    std::unordered_map<std::string, std::string> goal_parent_;
};
} // namespace llm_heuristics

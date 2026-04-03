#ifndef LLM_HEURISTICS_DOMAIN_DEPENDENT_HEURISTIC_H
#define LLM_HEURISTICS_DOMAIN_DEPENDENT_HEURISTIC_H

#include <string>
#include <vector>

namespace llm_heuristics {
struct Task {
    std::vector<std::string> goals;
    std::vector<std::string> static_facts;
};

int DEAD_END = -1;

class DomainDependentHeuristic {
public:
    virtual ~DomainDependentHeuristic() = default;
    virtual int operator()(const std::vector<std::string> &state) = 0;
};
}

#endif

#ifndef LLM_HEURISTICS_LLM_HEURISTICS_H
#define LLM_HEURISTICS_LLM_HEURISTICS_H

#include "../heuristic.h"

namespace llm_heuristics {
class DomainDependentHeuristic;

class LLMHeuristics : public Heuristic {
    std::unique_ptr<DomainDependentHeuristic> heuristic;
protected:
    virtual int compute_heuristic(const State &ancestor_state) override;
public:
    LLMHeuristics(
        const std::string &domain_name,
        const std::shared_ptr<AbstractTask> &transform,
        bool cache_estimates,
        const std::string &description,
        utils::Verbosity verbosity);
};
}

#endif

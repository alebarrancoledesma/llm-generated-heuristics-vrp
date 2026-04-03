#include "llm_heuristics.h"

#include "domain_dependent_heuristic.h"
#include "blocksworld.h"
#include "childsnack.h"
#include "floortile.h"
#include "miconic.h"
#include "rovers.h"
#include "sokoban.h"
#include "spanner.h"
#include "transport.h"

#include "../plugins/plugin.h"

#include "../utils/logging.h"

#include <fstream>
#include <iostream>

using namespace std;

namespace llm_heuristics {
static vector<string> read_file_lines(const string &filename) {
    vector<string> lines;
    ifstream file(filename);

    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            lines.push_back(line);
        }
        file.close();
    } else {
        cerr << "Unable to open file: " << filename << endl;
    }
    return lines;
}

static string strip_atom_prefix(const string &str) {
    string prefix = "Atom ";
    if (str.rfind(prefix, 0) == 0) {
        return str.substr(prefix.length());
    }
    return str;
}

static string replace_double_spaces(string text) {
    size_t pos = text.find("  ");
    while (pos != string::npos) {
        text.replace(pos, 2, " ");
        pos = text.find("  ", pos + 1); // Start searching after the replacement
    }
    return text;
}

static std::string get_pddl_atom_name(const std::string &name) {
    // 1. Find the position of the parentheses
    size_t openParenPos = name.find('(');
    size_t closeParenPos = name.find(')'); // Find the first closing parenthesis

    // Basic validation: Check if parentheses exist and are in the correct order
    if (openParenPos == std::string::npos || closeParenPos == std::string::npos || closeParenPos < openParenPos) {
        throw std::invalid_argument("Invalid predicate format: Missing or misplaced parentheses in '" + name + "'");
    }

    // 2. Extract the parts
    // Predicate name is everything before the opening parenthesis
    std::string predicateName = name.substr(0, openParenPos);

    // Arguments part is everything between the parentheses
    size_t argsStartPos = openParenPos + 1;
    size_t argsLength = closeParenPos - argsStartPos;
    std::string argumentsPart;
    // Only extract if there's actually something between the parentheses
    if (argsLength > 0) {
        argumentsPart = name.substr(argsStartPos, argsLength);
        // Replace all commas with spaces within the extracted arguments
        std::replace(argumentsPart.begin(), argumentsPart.end(), ',', ' ');
    }

    argumentsPart = replace_double_spaces(argumentsPart);

    // 3. Put everything together in the new format
    std::string result = "(" + predicateName;
    if (!argumentsPart.empty()) { // Add space and arguments only if they exist
        result += " " + argumentsPart;
    }
    result += ")";

    return result;
}

LLMHeuristics::LLMHeuristics(
    const string &domain_name,
    const shared_ptr<AbstractTask> &transform, bool cache_estimates,
    const string &description, utils::Verbosity verbosity)
    : Heuristic(transform, cache_estimates, description, verbosity) {
    if (log.is_at_least_normal()) {
        log << "Initializing LLM heuristic for " << domain_name << "..." << endl;
    }

    vector<string> static_facts = read_file_lines("static-atoms.txt");
    for (string &fact : static_facts) {
        fact = get_pddl_atom_name(fact);
    }

    vector<string> goals;
    for (FactProxy fact : task_proxy.get_goals()) {
        goals.push_back(get_pddl_atom_name(strip_atom_prefix(fact.get_name())));
    }

    if (log.is_at_least_debug()) {
        cout << "Static facts: " << static_facts << endl;
        cout << "Goals: " << goals << endl;
    }

    Task task{move(goals), move(static_facts)};

    if (domain_name == "blocksworld") {
        heuristic = make_unique<BlocksworldHeuristic>(task);
    } else if (domain_name == "childsnack") {
        heuristic = make_unique<ChildsnackHeuristic>(task);
    } else if (domain_name == "floortile") {
        heuristic = make_unique<FloortileHeuristic>(task);
    } else if (domain_name == "miconic") {
        heuristic = make_unique<MiconicHeuristic>(task);
    } else if (domain_name == "rovers") {
        heuristic = make_unique<RoversHeuristic>(task);
    } else if (domain_name == "sokoban") {
        heuristic = make_unique<SokobanHeuristic>(task);
    } else if (domain_name == "spanner") {
        heuristic = make_unique<SpannerHeuristic>(task);
    } else if (domain_name == "transport") {
        heuristic = make_unique<TransportHeuristic>(task);
    } else {
        ABORT("Unknown domain " + domain_name);
    }
}

int LLMHeuristics::compute_heuristic(const State &ancestor_state) {
    State state = convert_ancestor_state(ancestor_state);

    vector<string> atoms;
    for (FactProxy fact : state) {
        if (fact.get_name() != "<none of those>" && fact.get_name().rfind("NegatedAtom ", 0) != 0) {
            atoms.push_back(get_pddl_atom_name(strip_atom_prefix(fact.get_name())));
        }
    }

    int h = (*heuristic)(atoms);

    if (log.is_at_least_debug()) {
        cout << "State: " << atoms << " -> " << h << endl;
    }

    return h;
}

class LLMHeuristicsFeature
    : public plugins::TypedFeature<Evaluator, LLMHeuristics> {
public:
    LLMHeuristicsFeature() : TypedFeature("llm") {
        document_title("LLM heuristic");

        add_option<string>(
            "domain",
            "domain name",
            plugins::ArgumentInfo::NO_DEFAULT);

        add_heuristic_options_to_feature(*this, "llm");

        document_language_support("action costs", "ignored");
        document_language_support("conditional effects", "supported");
        document_language_support("axioms", "supported");

        document_property("admissible", "no");
        document_property("consistent", "no");
        document_property("safe", "no");
        document_property("preferred operators", "no");
    }

    virtual shared_ptr<LLMHeuristics>
    create_component(const plugins::Options &opts) const override {
        return plugins::make_shared_from_arg_tuples<LLMHeuristics>(
            opts.get<string>("domain"),
            get_heuristic_arguments_from_options(opts)
            );
    }
};

static plugins::FeaturePlugin<LLMHeuristicsFeature> _plugin;
}

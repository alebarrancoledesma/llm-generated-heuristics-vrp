# LLM-Generated Heuristics

This repository contains code to automatically generate domain-dependent heuristic functions for classical AI planning using Large Language Models (LLMs). Given a PDDL domain, an LLM is prompted to write a Python heuristic function that can then be used directly with the [Pyperplan](https://github.com/aibasel/pyperplan) planner.

# Installation

You'll need [uv](https://docs.astral.sh/uv/getting-started/installation/) installed on your system. Then run:

```bash
uv sync
```

# API Keys

Set the environment variable for whichever API you intend to use:

| Framework | Environment variable |
|-----------|----------------------|
| Google Gemini | `GOOGLE_API_KEY` |
| DeepSeek | `DEEPSEEK_API_KEY` |
| NVIDIA | `NVIDIA_API_KEY` |
| OpenAI | `OPENAI_API_KEY` |

# Heuristic Generation

Generate a heuristic for a given domain with:

```bash
uv run llm-heuristics.py --domain DOMAIN
```

The available domains are: `blocksworld`, `childsnack`, `floortile`, `miconic`, `rovers`, `sokoban`, `spanner`, and `transport`.

All options:

| Option | Description | Default |
|--------|-------------|---------|
| `--domain` | Domain to generate a heuristic for (required) | — |
| `--framework` | LLM API to use: `gemini`, `deepseek`, `nvidia`, `openai` | `gemini` |
| `--model` | Model name (must be supported by the chosen framework) | `gemini-2.5-flash` |
| `--heuristic-name` | Class name of the generated heuristic; must end with `Heuristic` | `NewDomainDependentHeuristic` |
| `--heuristic-file` | Output file for the generated heuristic; must end with `.py` | `new-heuristic.py` |
| `--temperature` | Sampling temperature | `1.0` |
| `--top-p` | Top-p sampling parameter | `0.5` |
| `--ablation` | Run an ablation variant of the prompt (omit for the full prompt) | — |

## Running the Planner

Once you have a heuristic file, run Pyperplan with it:

```bash
uv run src/pyperplan/pyperplan.py -s gbfs_early_goal_test -H HEURISTIC_FILE /path/to/domain.pddl /path/to/instance.pddl
```

You can also use `-H hff` for the FF heuristic or `-H blind` for blind search.

## Example

Generate a heuristic for the `blocksworld` domain:

```bash
uv run llm-heuristics.py --domain blocksworld --heuristic-name BlocksworldHeuristic --heuristic-file blocksworld-heuristic.py
```

Then solve a test instance using the generated heuristic:

```bash
uv run src/pyperplan/pyperplan.py -H blocksworld-heuristic.py -s gbfs_early_goal_test benchmarks/ipc2023-learning/testing/blocksworld/easy-p03.pddl
```

# End-to-End Plan Generation

For direct plan generation (without a separate heuristic step), use:

```bash
uv run end-to-end.py --domain DOMAIN --instance INSTANCE
```

where `INSTANCE` is the path to the PDDL instance file.

All options:

| Option | Description | Default |
|--------|-------------|---------|
| `--domain` | Domain of the instance (required) | — |
| `--instance` | Path to the PDDL instance file (required) | — |
| `--framework` | LLM API to use: `gemini`, `deepseek`, `nvidia`, `openai` | `gemini` |
| `--model` | Model name (must be supported by the chosen framework) | `gemini-2.5-flash` |
| `--plan-file` | Output file for the generated plan | `plan` |
| `--temperature` | Sampling temperature | `0.1` |
| `--top-p` | Top-p sampling parameter | `0.5` |

# Benchmarks

The `benchmarks/` directory contains domains from the Learning Track of the [IPC 2023](https://ipc2023-learning.github.io/) benchmark, split into training and testing sets.

# Extending the Project

## Adding a New Domain

1. Place your benchmark files under `benchmarks/` in a directory of your choice. You need:
   - `domain.pddl`: the PDDL domain file
   - `p01.pddl`: a small/easy training instance
   - `p99.pddl` (or similar): a larger training instance
   - `example-state.out`: a Pyperplan-style `frozenset` of ground atoms representing an intermediate state during search (see existing examples for the expected format)
   - `example-static.out`: a Pyperplan-style `frozenset` of static facts for that state (often empty for simple domains)

2. Add an entry to the `SUITES` dict in `src/llm_heuristics/suites.py`:

```python
"my-domain": DomainSuite(
    "my-domain",
    "benchmarks/my-benchmarks/my-domain/domain.pddl",
    "benchmarks/my-benchmarks/my-domain/p01.pddl",
    "benchmarks/my-benchmarks/my-domain/p99.pddl",
    "benchmarks/my-benchmarks/my-domain/example-state.out",
    "benchmarks/my-benchmarks/my-domain/example-static.out",
),
```

The domain is now selectable via `--domain my-domain`.

## Adding a New Prompt Format

1. Create a new file in `src/llm_heuristics/templates/`, e.g. `my_prompt.py`. Define a `Template` using Python's `string.Template`. The available substitution variables are: `$name`, `$heuristic_name`, `$domain`, `$instance1`, `$instance2`, `$state`, `$static`, `$heuristic1`, `$heuristic2`, `$task`. See `src/llm_heuristics/templates/reordered_prompt.py` for a complete example.

2. Export the constant from `src/llm_heuristics/templates/__init__.py`:

```python
from .my_prompt import *
```

3. Add a new choice to `--prompt-format` in `llm-heuristics.py`:

```python
type=click.Choice(["neurips", "my-format"]),
```

4. Add a branch in the `create_prompt()` function in `src/llm_heuristics/prompt.py`:

```python
if ablation == "false":
    if prompt_format == "my-format":
        template = MY_PROMPT
    else:
        template = NEURIPS2025_PROMPT
```

## Adding a New Model

**For an existing framework**, add an entry to the relevant model dict in `src/llm_heuristics/models.py`. For example, to add a new Gemini model:

```python
GEMINI_MODELS = {
    ...
    "gemini-3-ultra": "gemini-3-ultra-preview-12345",
}
```

The model is then selectable via `--model gemini-3-ultra`.

**For a new framework**, add a `run_<framework>()` function to `src/llm_heuristics/models.py` following the same signature as the existing ones (takes `model_name`, `prompt`, `temperature`, `top_p`; returns the answer string). Then add the framework name to the `--framework` choices in both `llm-heuristics.py` and `end-to-end.py`, and add the corresponding `elif` branch in each script's `main()`.

# Citing this Project

You can cite this project by citing the NeurIPS paper directly:

```tex
@InProceedings{correa-et-al-neurips2025,
  author =       "Augusto B. Corr\^{e}a and Andr{\'{e}} Grahl Pereira and Jendrik Seipp",
  title =        "Classical Planning with {LLM}-Generated Heuristics: Challenging the State of the Art with {Python} Code",
  booktitle =    "Proceedings of the Thirty-Ninth Annual Conference on
                  Neural Information Processing Systems ({NeurIPS} 2025)",
  year =         "2025"
}
```

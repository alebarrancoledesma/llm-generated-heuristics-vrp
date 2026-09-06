# CVRP-STRIPS — evaluación (`testing/`)

Instancias **modo B** (demanda por destino), goal **open** (`-open`), depósito `l0`, hub `l0↔l1`.

Prefijos IPC: `easy-*` / `medium-*` → split **testing**; `p10`, `p20`, `p50` → split **training** (sin etiqueta de dificultad).

## Orden recomendado (complejidad ↑)

| # | Fichero | Procedencia IPC | Pkg | Veh | Clientes |
|---|---------|-----------------|-----|-----|----------|
| 1 | `easy-p01-open.pddl` | `ipc2023-learning/testing/transport/easy-p01.pddl` | 1 | 3 | 1 |
| 2 | `p10-open.pddl` | `ipc2023-learning/training/transport/p10.pddl` | 4 | 2 | 3 |
| 3 | `easy-p10-open.pddl` | `ipc2023-learning/testing/transport/easy-p10.pddl` | 5 | 4 | 3 |
| 4 | `p20-open.pddl` | `ipc2023-learning/training/transport/p20.pddl` | 4 | 3 | 3 |
| 5 | `medium-p01-open.pddl` | `ipc2023-learning/testing/transport/medium-p01.pddl` | 5 | 10 | 5 |
| 6 | `easy-p20-open.pddl` | `ipc2023-learning/testing/transport/easy-p20.pddl` | 10 | 5 | 7 |
| 7 | `p50-open.pddl` | `ipc2023-learning/training/transport/p50.pddl` | 9 | 5 | 7 |

`domain.pddl`: copia del dominio `transport` (STRIPS, sin costes).

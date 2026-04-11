from dataclasses import dataclass

BENCHMARKS = "benchmarks/"
IPC_2023 = "/".join([BENCHMARKS, "ipc2023-learning", "training"])

@dataclass
class DomainSuite:
    name: str
    domain: str
    instance1: str
    instance2: str
    state : str
    static : str


SUITES = {
    "blocksworld": DomainSuite(
        "blocksworld",
        f"{IPC_2023}/blocksworld/domain.pddl",
        f"{IPC_2023}/blocksworld/p01.pddl",
        f"{IPC_2023}/blocksworld/p99.pddl",
        f"{IPC_2023}/blocksworld/example-state.out",
        f"{IPC_2023}/blocksworld/example-static.out",
    ),
    "childsnack": DomainSuite(
        "childsnack",
        f"{IPC_2023}/childsnack/domain.pddl",
        f"{IPC_2023}/childsnack/p01.pddl",
        f"{IPC_2023}/childsnack/p99.pddl",
        f"{IPC_2023}/childsnack/example-state.out",
        f"{IPC_2023}/childsnack/example-static.out",
    ),
    "floortile": DomainSuite(
        "floortile",
        f"{IPC_2023}/floortile/domain.pddl",
        f"{IPC_2023}/floortile/p01.pddl",
        f"{IPC_2023}/floortile/p99.pddl",
        f"{IPC_2023}/floortile/example-state.out",
        f"{IPC_2023}/floortile/example-static.out",
    ),
    "miconic": DomainSuite(
        "miconic",
        f"{IPC_2023}/miconic/domain.pddl",
        f"{IPC_2023}/miconic/p01.pddl",
        f"{IPC_2023}/miconic/p99.pddl",
        f"{IPC_2023}/miconic/example-state.out",
        f"{IPC_2023}/miconic/example-static.out",
    ),
    "rovers": DomainSuite(
        "rovers",
        f"{IPC_2023}/rovers/domain.pddl",
        f"{IPC_2023}/rovers/p01.pddl",
        f"{IPC_2023}/rovers/p99.pddl",
        f"{IPC_2023}/rovers/example-state.out",
        f"{IPC_2023}/rovers/example-static.out",
    ),
    "sokoban": DomainSuite(
        "sokoban",
        f"{IPC_2023}/sokoban/domain.pddl",
        f"{IPC_2023}/sokoban/p01.pddl",
        f"{IPC_2023}/sokoban/p99.pddl",
        f"{IPC_2023}/sokoban/example-state.out",
        f"{IPC_2023}/sokoban/example-static.out",
    ),
    "spanner": DomainSuite(
        "spanner",
        f"{IPC_2023}/spanner/domain.pddl",
        f"{IPC_2023}/spanner/p01.pddl",
        f"{IPC_2023}/spanner/p99.pddl",
        f"{IPC_2023}/spanner/example-state.out",
        f"{IPC_2023}/spanner/example-static.out",
    ),
    "transport": DomainSuite(
        "transport",
        f"{IPC_2023}/transport/domain.pddl",
        f"{IPC_2023}/transport/p01.pddl",
        f"{IPC_2023}/transport/p99.pddl",
        f"{IPC_2023}/transport/example-state.out",
        f"{IPC_2023}/transport/example-static.out",
    ),
}

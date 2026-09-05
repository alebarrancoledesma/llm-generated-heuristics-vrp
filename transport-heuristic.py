from fnmatch import fnmatch
from heuristics.heuristic_base import Heuristic


def get_parts(fact):
    """Extract the components of a PDDL fact by removing parentheses and splitting the string."""
    return fact[1:-1].split()


def match(fact, *args):
    """
    Check if a PDDL fact matches a given pattern.

    - `fact`: The complete fact as a string, e.g., "(at p1 l1)".
    - `args`: The expected pattern (wildcards `*` allowed).
    - Returns `True` if the fact matches the pattern, else `False`.
    """
    parts = get_parts(fact)
    return all(fnmatch(part, arg) for part, arg in zip(parts, args))


class TransportHeuristic(Heuristic):
    """
    A domain-dependent heuristic for the Transport domain.

    # Summary
    This heuristic estimates the number of actions needed to transport all packages
    to their goal locations using vehicles with limited capacity.

    # Assumptions
    - Each vehicle has a capacity (number of packages it can carry simultaneously).
    - Packages can be at locations, inside vehicles, or at their goal locations.
    - Vehicles can move between connected locations (via roads).
    - A vehicle can pick up a package only if it has available capacity.
    - A vehicle can drop a package only if it is carrying it.

    # Heuristic Initialization
    - Extract goal locations for each package from the task goals.
    - Extract static facts:
        - Road connectivity between locations.
        - Capacity-predecessor relationships (to determine capacity levels).
    - Determine the maximum capacity of vehicles from the capacity-predecessor chain.

    # Step-By-Step Thinking for Computing Heuristic
    1. Extract Relevant Information:
    - Identify the current location of every package (either at a location or inside a vehicle).
    - Identify the current location of every vehicle.
    - Determine the current capacity of each vehicle (from the capacity predicate).

    2. For Each Package:
    - Determine if the package is already at its goal location. If so, no cost is added.
    - If the package is inside a vehicle, determine the vehicle's current location.
    - Compute the shortest path distance from the package's current location (or vehicle's location) to its goal location using BFS on the road graph.

    3. Estimate Actions per Package:
    - If the package is at a location and not at its goal:
        - It needs to be picked up by a vehicle (1 action).
        - The vehicle needs to drive to the goal location (distance actions).
        - The package needs to be dropped at the goal (1 action).
    - If the package is inside a vehicle:
        - The vehicle needs to drive to the goal location (distance actions).
        - The package needs to be dropped at the goal (1 action).

    4. Account for Vehicle Capacity:
    - For each vehicle, determine how many packages it is currently carrying.
    - If a vehicle is carrying packages, it cannot pick up additional packages until it drops some off.
    - When estimating, if multiple packages need to be transported by the same vehicle, consider that the vehicle can carry multiple packages at once (up to its capacity).
    - For simplicity and efficiency, we estimate the cost as if each package is transported independently, but we add a penalty for vehicles that are already at capacity.

    5. Summing the Actions:
    - The total heuristic value is the sum of all necessary actions for all packages.
    - For each package not at its goal:
        - If it's at a location: add 2 (pick-up + drop) + distance to goal.
        - If it's inside a vehicle: add 1 (drop) + distance from vehicle's location to goal.
    - Additionally, if a vehicle is at capacity and needs to pick up more packages, add a small penalty to account for the need to drop packages first.
    """

    def __init__(self, task):
        """Initialize the heuristic by extracting goal conditions and static facts."""
        self.goals = task.goals
        static_facts = task.static

        # Extract road connectivity
        self.roads = {}
        for fact in static_facts:
            if match(fact, "road", "*", "*"):
                parts = get_parts(fact)
                loc1, loc2 = parts[1], parts[2]
                if loc1 not in self.roads:
                    self.roads[loc1] = set()
                if loc2 not in self.roads:
                    self.roads[loc2] = set()
                self.roads[loc1].add(loc2)
                self.roads[loc2].add(loc1)

        # Extract capacity-predecessor relationships to determine capacity levels
        self.capacity_levels = {}
        for fact in static_facts:
            if match(fact, "capacity-predecessor", "*", "*"):
                parts = get_parts(fact)
                smaller, larger = parts[1], parts[2]
                self.capacity_levels[larger] = smaller

        # Determine the maximum capacity level (number of packages a vehicle can carry)
        # Count the length of the capacity chain
        self.max_capacity = 0
        for fact in static_facts:
            if match(fact, "capacity-predecessor", "*", "*"):
                parts = get_parts(fact)
                # Find the top of the chain
                current = parts[2]
                count = 1
                while current in self.capacity_levels:
                    current = self.capacity_levels[current]
                    count += 1
                self.max_capacity = max(self.max_capacity, count)

        # Extract goal locations for each package
        self.goal_locations = {}
        for goal in self.goals:
            parts = get_parts(goal)
            if parts[0] == "at":
                package, location = parts[1], parts[2]
                self.goal_locations[package] = location

    def _bfs_distance(self, start, goal):
        """Compute shortest path distance between two locations using BFS."""
        if start == goal:
            return 0
        if start not in self.roads or goal not in self.roads:
            return float('inf')

        visited = {start}
        queue = [(start, 0)]
        while queue:
            current, dist = queue.pop(0)
            for neighbor in self.roads.get(current, []):
                if neighbor == goal:
                    return dist + 1
                if neighbor not in visited:
                    visited.add(neighbor)
                    queue.append((neighbor, dist + 1))
        return float('inf')

    def __call__(self, node):
        """Compute an estimate of the minimal number of required actions."""
        state = node.state

        # Track where packages and vehicles are currently located
        package_locations = {}  # package -> location (or vehicle name if inside)
        vehicle_locations = {}  # vehicle -> location
        vehicle_capacities = {}  # vehicle -> current capacity level (number of packages it can still carry)

        # Parse state facts
        for fact in state:
            parts = get_parts(fact)
            if parts[0] == "at":
                obj, loc = parts[1], parts[2]
                if obj.startswith("p"):  # package
                    package_locations[obj] = loc
                elif obj.startswith("v"):  # vehicle
                    vehicle_locations[obj] = loc
            elif parts[0] == "in":
                package, vehicle = parts[1], parts[2]
                package_locations[package] = vehicle
            elif parts[0] == "capacity":
                vehicle, capacity = parts[1], parts[2]
                # Convert capacity level to number of packages it can carry
                # c0 -> 0, c1 -> 1, c2 -> 2, etc.
                capacity_num = int(capacity[1:])  # Extract number from c0, c1, etc.
                vehicle_capacities[vehicle] = capacity_num

        total_cost = 0

        # For each package that needs to reach a goal location
        for package, goal_loc in self.goal_locations.items():
            if package not in package_locations:
                continue  # Package not in state (shouldn't happen)

            current_loc = package_locations[package]

            # Check if package is already at its goal
            if current_loc == goal_loc:
                continue

            # Determine if package is inside a vehicle or at a location
            if current_loc.startswith("v"):  # Inside a vehicle
                vehicle = current_loc
                vehicle_loc = vehicle_locations.get(vehicle)
                if vehicle_loc is None:
                    continue  # Vehicle not found (shouldn't happen)

                # Distance from vehicle's location to goal
                distance = self._bfs_distance(vehicle_loc, goal_loc)
                if distance == float('inf'):
                    continue  # Unreachable

                # Need to drive to goal and drop the package
                total_cost += distance + 1  # 1 for drop action

            else:  # Package is at a location
                # Distance from current location to goal
                distance = self._bfs_distance(current_loc, goal_loc)
                if distance == float('inf'):
                    continue  # Unreachable

                # Need to pick up, drive, and drop
                total_cost += 1 + distance + 1  # pick-up + drive + drop

        # Add penalty for vehicles that are at capacity and might need to make multiple trips
        # This is a rough estimate: if a vehicle is at capacity and there are packages
        # that need to be transported, we might need extra trips.
        # For simplicity, we don't add extra penalties here since the per-package
        # estimate already accounts for the main actions.

        return total_cost

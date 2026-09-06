from fnmatch import fnmatch
from heuristics.heuristic_base import Heuristic


def get_parts(fact):
    """Extract the components of a PDDL fact by removing parentheses and splitting the string."""
    return fact[1:-1].split()


def match(fact, *args):
    """
    Check if a PDDL fact matches a given pattern.

    - `fact`: The complete fact as a string, e.g., "(at p1 l0)".
    - `args`: The expected pattern (wildcards `*` allowed).
    - Returns `True` if the fact matches the pattern, else `False`.
    """
    parts = get_parts(fact)
    return all(fnmatch(part, arg) for part, arg in zip(parts, args))


class CvrpStripsHeuristic(Heuristic):
    """
    A domain-dependent heuristic for the CVRP (Capacitated Vehicle Routing Problem) domain.

    # Summary
    This heuristic estimates the number of actions needed to deliver all packages to their
    goal locations using a fleet of vehicles with limited capacity. Each vehicle can carry
    a certain number of packages, and must drive between locations to pick up and drop off
    packages.

    # Assumptions
    - Each package occupies exactly one unit of capacity in a vehicle.
    - Vehicles can only pick up packages at their current location.
    - Vehicles can only drop off packages at their current location.
    - The road network is undirected (if there is a road from A to B, there is also a road from B to A).
    - All vehicles start at the depot (location l0) in the initial state, but may be anywhere in the current state.
    - The heuristic does not need to be admissible; it only needs to be informative for greedy best-first search.

    # Heuristic Initialization
    - Extract the goal locations for each package from the task goals.
    - Extract the road network from static facts to compute shortest path distances between locations.
    - Extract the capacity levels and their ordering from static facts.
    - Identify all vehicles and their initial capacities from the initial state.

    # Step-By-Step Thinking for Computing Heuristic
    1. Extract Relevant Information:
       - For each package, determine its current location (either at a location or inside a vehicle).
       - For each vehicle, determine its current location and its current remaining capacity.
       - Determine the goal location for each package.

    2. Compute Shortest Path Distances:
       - Use the road network to compute all-pairs shortest path distances between locations.
       - This allows us to estimate driving costs between any two locations.

    3. Handle Packages Already in Vehicles:
       - If a package is inside a vehicle, it will need to be dropped off at some location.
       - If the vehicle is already at the package's goal location, only a drop action is needed.
       - Otherwise, the vehicle must drive to the goal location and then drop the package.

    4. Handle Packages at Locations:
       - If a package is at its goal location, no action is needed for that package.
       - Otherwise, a vehicle must travel to the package's current location, pick it up, and then travel to the goal location to drop it off.

    5. Account for Vehicle Capacity:
       - Each vehicle has a limited capacity (number of packages it can carry).
       - When planning deliveries, we must ensure that a vehicle does not exceed its capacity.
       - For the heuristic, we estimate the number of trips needed based on the total number of packages that need to be moved and the total capacity of all vehicles.

    6. Estimate the Number of Actions:
       - Each pick-up action costs 1.
       - Each drop action costs 1.
       - Each drive action costs 1 (regardless of distance, but we use shortest path distances to estimate the number of drive actions needed).
       - The heuristic sums the estimated number of pick-ups, drops, and drives needed to deliver all packages.

    7. Consider Vehicle Positioning:
       - If a vehicle is not at the depot, it may need to drive to a pickup location first.
       - After delivering packages, vehicles may need to return to the depot or reposition for future pickups.
       - The heuristic accounts for the current positions of vehicles when estimating driving costs.

    8. Final Heuristic Value:
       - The heuristic value is the sum of all estimated action costs.
       - If all packages are already at their goal locations, the heuristic returns 0.
    """

    def __init__(self, task):
        """Initialize the heuristic by extracting goal conditions and static facts."""
        self.goals = task.goals
        static_facts = task.static

        # Extract road network: map each location to its neighbors
        self.roads = {}
        for fact in static_facts:
            if match(fact, "road", "*", "*"):
                loc1, loc2 = get_parts(fact)[1], get_parts(fact)[2]
                if loc1 not in self.roads:
                    self.roads[loc1] = set()
                if loc2 not in self.roads:
                    self.roads[loc2] = set()
                self.roads[loc1].add(loc2)
                self.roads[loc2].add(loc1)

        # Extract capacity predecessor relationships
        self.capacity_order = {}
        for fact in static_facts:
            if match(fact, "capacity-predecessor", "*", "*"):
                s1, s2 = get_parts(fact)[1], get_parts(fact)[2]
                self.capacity_order[s1] = s2  # s1 is predecessor of s2

        # Extract goal locations for each package
        self.goal_locations = {}
        for goal in self.goals:
            if match(goal, "at", "*", "*"):
                package, location = get_parts(goal)[1], get_parts(goal)[2]
                self.goal_locations[package] = location

        # Extract all locations from the road network
        self.locations = set(self.roads.keys())

        # Compute all-pairs shortest path distances using Floyd-Warshall
        self.distances = self._compute_all_pairs_shortest_paths()

    def _compute_all_pairs_shortest_paths(self):
        """Compute shortest path distances between all pairs of locations."""
        # Initialize distances with infinity
        dist = {loc: {other: float('inf') for other in self.locations} for loc in self.locations}
        for loc in self.locations:
            dist[loc][loc] = 0
            for neighbor in self.roads.get(loc, []):
                dist[loc][neighbor] = 1

        # Floyd-Warshall algorithm
        for k in self.locations:
            for i in self.locations:
                for j in self.locations:
                    if dist[i][k] + dist[k][j] < dist[i][j]:
                        dist[i][j] = dist[i][k] + dist[k][j]

        return dist

    def __call__(self, node):
        """Estimate the minimum cost to deliver all packages to their goals."""
        state = node.state

        # Parse current state
        package_locations = {}  # package -> location (or vehicle if inside)
        vehicle_locations = {}  # vehicle -> location
        vehicle_capacities = {}  # vehicle -> remaining capacity (as size level)
        capacity_levels = {}  # size level -> numeric capacity (0, 1, 2, ...)

        # First, identify all vehicles and their capacities
        for fact in state:
            if match(fact, "capacity", "*", "*"):
                vehicle, size = get_parts(fact)[1], get_parts(fact)[2]
                # Convert size to numeric capacity: c0=0, c1=1, c2=2, etc.
                numeric_capacity = int(size[1:])  # Extract number from c0, c1, etc.
                vehicle_capacities[vehicle] = numeric_capacity

        # Identify vehicle locations
        for fact in state:
            if match(fact, "at", "*", "*"):
                obj, loc = get_parts(fact)[1], get_parts(fact)[2]
                if obj.startswith('v'):  # It's a vehicle
                    vehicle_locations[obj] = loc

        # Identify package locations (either at a location or inside a vehicle)
        for fact in state:
            if match(fact, "at", "*", "*"):
                obj, loc = get_parts(fact)[1], get_parts(fact)[2]
                if obj.startswith('p'):  # It's a package
                    package_locations[obj] = loc
            elif match(fact, "in", "*", "*"):
                package, vehicle = get_parts(fact)[1], get_parts(fact)[2]
                package_locations[package] = vehicle  # Package is inside vehicle

        # Check if all packages are at their goals
        all_delivered = True
        for package, goal_loc in self.goal_locations.items():
            current_loc = package_locations.get(package)
            if current_loc != goal_loc:
                all_delivered = False
                break

        if all_delivered:
            return 0

        # Compute heuristic
        total_cost = 0

        # For each package that needs to be delivered, estimate the cost
        # We'll group packages by their current location to estimate vehicle trips

        # First, handle packages that are already in vehicles
        packages_in_vehicles = {}
        for package, loc in package_locations.items():
            if loc.startswith('v'):  # Package is in a vehicle
                vehicle = loc
                if vehicle not in packages_in_vehicles:
                    packages_in_vehicles[vehicle] = []
                packages_in_vehicles[vehicle].append(package)

        # For each vehicle with packages inside, estimate drop-off costs
        for vehicle, packages in packages_in_vehicles.items():
            vehicle_loc = vehicle_locations.get(vehicle, 'l0')
            for package in packages:
                goal_loc = self.goal_locations.get(package)
                if goal_loc is None:
                    continue
                if vehicle_loc == goal_loc:
                    # Vehicle is already at goal, just drop
                    total_cost += 1  # drop action
                else:
                    # Need to drive to goal and drop
                    dist = self.distances.get(vehicle_loc, {}).get(goal_loc, float('inf'))
                    if dist != float('inf'):
                        total_cost += dist  # drive actions
                    total_cost += 1  # drop action

        # Now handle packages that are at locations (not in vehicles)
        # Group packages by their current location
        packages_at_locations = {}
        for package, loc in package_locations.items():
            if not loc.startswith('v'):  # Package is at a location
                if loc not in packages_at_locations:
                    packages_at_locations[loc] = []
                packages_at_locations[loc].append(package)

        # For each location with packages to pick up, estimate the cost
        # We need to consider vehicle capacity and routing
        # For simplicity, we'll estimate that each package requires a separate trip
        # unless multiple packages at the same location can be picked up together

        # Get all vehicles and their current locations
        available_vehicles = list(vehicle_locations.keys())

        # For each location with packages to deliver
        for pickup_loc, packages in packages_at_locations.items():
            for package in packages:
                goal_loc = self.goal_locations.get(package)
                if goal_loc is None:
                    continue
                if pickup_loc == goal_loc:
                    continue  # Already at goal

                # Find the nearest vehicle to this pickup location
                # For simplicity, we'll assume we can use any vehicle
                # and estimate the cost as: drive to pickup, pick up, drive to goal, drop
                # We'll use the minimum distance from any vehicle to the pickup location
                min_vehicle_dist = float('inf')
                for vehicle in available_vehicles:
                    vehicle_loc = vehicle_locations.get(vehicle, 'l0')
                    dist = self.distances.get(vehicle_loc, {}).get(pickup_loc, float('inf'))
                    if dist < min_vehicle_dist:
                        min_vehicle_dist = dist

                if min_vehicle_dist != float('inf'):
                    total_cost += min_vehicle_dist  # drive to pickup

                total_cost += 1  # pick up

                # Drive from pickup to goal
                dist_pickup_goal = self.distances.get(pickup_loc, {}).get(goal_loc, float('inf'))
                if dist_pickup_goal != float('inf'):
                    total_cost += dist_pickup_goal  # drive to goal

                total_cost += 1  # drop

        # Account for vehicle capacity constraints
        # Count total packages that need to be moved
        packages_to_move = 0
        for package, goal_loc in self.goal_locations.items():
            current_loc = package_locations.get(package)
            if current_loc != goal_loc:
                packages_to_move += 1

        # Total capacity of all vehicles
        total_capacity = sum(vehicle_capacities.values())

        # If total capacity is less than packages to move, we need multiple trips
        # Each trip requires a vehicle to return to a pickup location
        # For simplicity, we add extra cost for additional trips
        if total_capacity > 0 and packages_to_move > total_capacity:
            extra_trips = (packages_to_move + total_capacity - 1) // total_capacity - 1
            # Each extra trip requires driving back to pickup locations
            # We estimate this as an additional drive between the depot and pickup locations
            total_cost += extra_trips * 2  # Rough estimate for return trips

        return total_cost

;; CVRP-STRIPS open | mode B | adapted from benchmarks/ipc2023-learning/testing/transport/easy-p01.pddl
;; vehicles=3, packages=1, locations=5, max_capacity=2, out_folder=testing/easy, instance_id=1, seed=1007
;; depot=l0 (hub l0<->l1) | demands: l3=1
;; packages=1 vehicles=3 locations=6
;; feasibility: structural (hub l1 reachable to all goal sites)

(define (problem cvrp-easy-p01-open)
 (:domain transport)
 (:objects
    v1 v2 v3 - vehicle
    p1 - package
    l0 l1 l2 l3 l4 l5 - location
    c0 c1 c2 - size
    )
 (:init
    (capacity v1 c1)
    (capacity v2 c1)
    (capacity v3 c2)
    (capacity-predecessor c0 c1)
    (capacity-predecessor c1 c2)
    (at p1 l0)
    (at v1 l0)
    (at v2 l0)
    (at v3 l0)
    (road l0 l1)
    (road l1 l0)
    (road l1 l3)
    (road l1 l4)
    (road l2 l3)
    (road l3 l1)
    (road l3 l2)
    (road l3 l4)
    (road l4 l1)
    (road l4 l3)
    (road l4 l5)
    (road l5 l4)
    )
 (:goal (and
    (at p1 l3)
    )))

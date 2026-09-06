;; CVRP-STRIPS open | mode B | adapted from benchmarks/ipc2023-learning/training/transport/p10.pddl
;; base case
;; depot=l0 (hub l0<->l1) | demands: l2=1 l3=1 l4=2
;; packages=4 vehicles=2 locations=5
;; feasibility: structural (hub l1 reachable to all goal sites)

(define (problem cvrp-p10-open)
 (:domain transport)
 (:objects
    v1 v2 - vehicle
    p1 p2 p3 p4 - package
    l0 l1 l2 l3 l4 - location
    c0 c1 c2 - size
    )
 (:init
    (capacity v1 c2)
    (capacity v2 c2)
    (capacity-predecessor c0 c1)
    (capacity-predecessor c1 c2)
    (at p1 l0)
    (at p2 l0)
    (at p3 l0)
    (at p4 l0)
    (at v1 l0)
    (at v2 l0)
    (road l0 l1)
    (road l1 l0)
    (road l1 l4)
    (road l2 l3)
    (road l2 l4)
    (road l3 l2)
    (road l4 l1)
    (road l4 l2)
    )
 (:goal (and
    (at p1 l2)
    (at p2 l3)
    (at p3 l4)
    (at p4 l4)
    )))

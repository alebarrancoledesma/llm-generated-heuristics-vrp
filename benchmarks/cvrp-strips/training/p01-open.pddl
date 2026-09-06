;; CVRP-STRIPS open | mode B (demand-by-destination)
;; depot=l0 | demands: l1=2, l2=1 | 1 vehicle cap 2 | map: star l0--l1, l0--l2
;; packages fungible: p1,p2→l1; p3→l2

(define (problem cvrp-p01-open)
 (:domain transport)
 (:objects
    v1 - vehicle
    p1 p2 p3 - package
    l0 l1 l2 - location
    c0 c1 c2 - size
    )
 (:init
    (capacity v1 c2)
    (capacity-predecessor c0 c1)
    (capacity-predecessor c1 c2)
    (at v1 l0)
    (at p1 l0)
    (at p2 l0)
    (at p3 l0)
    (road l0 l1)
    (road l1 l0)
    (road l0 l2)
    (road l2 l0)
    )
 (:goal (and
    (at p1 l1)
    (at p2 l1)
    (at p3 l2)
    )))

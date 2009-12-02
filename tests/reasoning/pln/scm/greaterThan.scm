; example to proof a > c using than if x > y and y > z then x > z
; axiom
(define agt (PredicateNode "AboutGreaterThan"))
(define x (VariableNode "x"))
(define y (VariableNode "y"))
(define z (VariableNode "z"))
(ImplicationLink (stv 0.8 1)
    (AndLink
        (EvaluationLink
            agt
            (ListLink x y))
        (EvaluationLink
            agt
            (ListLink y z))
        )
    (EvaluationLink
        gt
        (ListLink x z)
        )
    )
; facts
(define a (ConceptNode "a"))
(define b (ConceptNode "b"))
(define c (ConceptNode "c"))
(EvaluationLink (stv 1 1) agt (ListLink a b))
(EvaluationLink (stv 1 1) agt (ListLink b c))

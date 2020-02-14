;; Rule for introducing
;;
;; IntensionalInheritanceLink
;;   A
;;   B
;;
;; based on direct evidence of patterns between A and B, where a
;; pattern of A is a super set of A with a description shorter than A.
;;
;; A
;; B
;; |-
;; IntensionalInheritance <TV>
;;   A
;;   B
;;
;; where TV is
;;
;; SubsetLink <TV>
;;   patterns-of(B)
;;   patterns-of(A)
;;
;; Note that A and B have been swapped, that is because if A inherits
;; B, then A is a specialization of B, thus B has less patterns than
;; A.
;;
;; patterns-of(A) is defined as the satifying set of pattern-of(X,A)
;; over X, where pattern-of(X,A) is calculated as follows
;;
;;   pattern-of(X,A) = s(X) × (P(X|A)-P(X|¬A))+
;;
;; where s(X) is the prior of X, reflecting it's simplicity. Thus the
;; simpler X is and the stronger the discriminating power of X over A
;; is, the more X is a pattern of A. Also, (x)+ is the posivitive part
;; of x, see
;; https://en.wikipedia.org/wiki/Positive_and_negative_parts.
;;
;; For the discriminating power of X over A, we also say that A is
;; attracted to pattern X, which can be represented with
;;
;; AttractionLink
;;   A
;;   X
;;
;; Although not present in the premises, this rule requires all
;; relevant attraction links to be present in the atomspace in order
;; to correctly calculate the TV.

;; TODO: in order to add the Attraction links in the premises maybe an
;; idea would be to introduce a has-closure predicate, such as
;;
;; Evaluation (stv 1 1)
;;   Predicate "has-closure"
;;   (Lambda X (Attraction A X))
;;
;; and
;;
;; Evaluation (stv 1 1)
;;   Predicate "has-closure"
;;   (Lambda X (Attraction B X))
;;
;; Or maybe even introduce a HasClosureLink, such as
;;
;; (HasClosureLink (stv 1 1)
;;   X
;;   (Attraction A X))

;; Rule
(define intensional-inheritance-direct-introduction-rule
  (define A (Variable "$A"))
  (define B (Variable "$B"))
  (define CT (Type "ConceptNode"))
  (Bind
    (VariableSet
      (TypedVariable A CT)
      (TypedVariable A CT))
    (Present
      A
      B)
    (ExecutionOutput
      (GroundedSchema "scm: intensional-inheritance-direct-introduction")
      (List
        ;; Conclusion
        (IntensionalInheritance A B)
        ;; Premises
        A
        B))))

;; Formula
(define (intensional-inheritance-direct-introduction conclusion . premises)
  ;; Given a concept return all attraction link
  ;;
  ;; Attraction <TV>
  ;;   A
  ;;   X
  (define (get-attractions A)
    (let* ((at-links (cog-filter 'AttractionLink (cog-incoming-set A)))
           (A-at? (lambda (x) (equal? A (gar x)))))
      (map gar (filter A-at? at-links))))
    
  ;; Given the attraction links of A and B calculate the fuzzy
  ;; intersection between the patterns of A and B, expressed as
  ;;
  ;; Sum_x min(pattern-of(X,A), pattern-of(X,B))
  ;;
  ;; where pattern-of(X,A) is the strength of the TV of
  ;;
  ;; Attraction <TV>
  ;;   A
  ;;   X
  (define (numerator A-ats B)
    (define (fuzzy-intersect A-at)
      (let* ((pat (gdr A-at))
             (B-at (cog-link 'AttractionLink B pat)))
        (if (null? B-at)
            0
            (min (cog-mean B-at) (cog-mean A-at)))))
    (fold + 0 (map fuzzy-intersect A-ats)))

  ;; Given the attraction links of A calculate the fuzzy sum of the
  ;; patterns of A expressed as
  ;;
  ;; Sum_x pattern-of(X,A)
  (define (denominator A-ats)
    (fold + 0 (map cog-mean A-ats)))

  ;; (cog-logger-debug "(intensional-inheritance-direct-introduction conclusion=~a . premises=~a)" conclusion premises)
  (if (= (length premises) 2)
      (let* ((IntInh conclusion)
             (A (car premises))
             (B (cadr premises))
             ;; Fetch all patterns of A (i.e. AttractionLink targets)
             (A-ats (get-attractions A))
             (TVs (numerator A-ats B) / (denominator A-ats))
             (TVc (count->confidence (length A-ats)))
             (TV (stv TVs TVc)))
        (cog-merge-hi-conf-tv! IntInh TV))))

; Name the rule
(define intensional-inheritance-direct-introduction-rule-name
  (DefinedSchemaNode "intensional-inheritance-direct-introduction-rule"))
(DefineLink intensional-inheritance-direct-introduction-rule-name
  intensional-inheritance-direct-introduction-rule)

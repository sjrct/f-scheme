(define (caar ls) (car (car ls)))
(define (cadr ls) (car (cdr ls)))
(define (cdar ls) (cdr (car ls)))
(define (cddr ls) (cdr (cdr ls)))

(define else #t)

(define (list &rest args) args)

(define do eval)

(define (map ls f)
  (cond ((null? ls) ())
        (#t (cons (f (car ls))
                  (map (cdr ls) f)))))

(define let
  (macro (lets body)
         (eval
           (cons
             (list
               (quote lambda)
               (map lets car)
               body)
             (map lets (lambda (pair) (eval (car (cdr pair)))))))))

(define (is-defined? name)
  (try (let ((_ (eval name))) #t)
       (lambda (err) #f)))

(define if
  (macro (check yes no)
    (let ((result (eval check)))
      (cond (result (eval yes))
            (else (eval no))))))

(define (abs x)
  (cond ((< x 0) (- x))
         (else x)))

;(define (accumulate combiner null-value term a next b)
;  (cond ((> a b) null-value)
;        (#t (combiner
;              (term a)
;              (accumulate combiner null-value term (next a) next b)))))

(define (accumulate combiner null-value term a next b)
  (accumulate-iter combiner null-value term a next b))

(define (accumulate-iter combiner value term a next b)
  (cond ((> a b) null-value)
        (else
          (accumulate-iter combiner (combiner (term a) value) term (next a) next b))))

(define (identity x) x)

(define (ncat ls)
  (cond ((null? ls) "")
        ((number? (car ls))
         (concat
           (number->string (car ls))
           (ncat (cdr ls))))
        (else
         (concat
           (car ls)
           (ncat (cdr ls))))))

(define current-test-name "<no-test>")

(define (expect-eq got expect)
  (cond ((= got expect) #t)
        (else
          (raise (ncat (list current-test-name ": got " got " but expected " expect))))))

(define (run-test name f)
  (try
    (do
      (set! current-test-name name)
      (f)
      (set! current-test-name "<no-test>"))
    (lambda (e) e)))

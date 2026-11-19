## Regressions that the install benchmarks found (xml2, vctrs).
opts <- list(suppressAll = FALSE)
same <- function(f) stopifnot(crbcc:::identical_strict(
  compiler::cmpfun(f), crbcc::cmpfun(f, options = opts)))
## The name of the first argument of a replacement call.
same(function(x, v) { f(x = x, a = 1) <- v; x })
## ..1 and a compiler note in a closure with srcrefs.
same(eval(parse(text = "function(...) { g(..1) }", keep.source = TRUE)))
same(eval(parse(text = "function(x) { g(..1) }", keep.source = TRUE)))

## The GC must not free the argument list that the inline of a simple
## .Internal call makes. The .Internal call of identical() has 9 arguments.
## Do not print the result: a bad result can be a cyclic list.
f <- function(x, y) identical(x, y)
expected <- compiler::cmpfun(f)
for (step in 2:10) {
  gctorture2(step)
  code <- crbcc::cmpfun(f)
  gctorture2(0)
  stopifnot(crbcc:::identical_strict(expected, code))
}

## The compile must force the same promises as GNU R. GNU R forces only the
## value of a function that it examines, and not in a local frame.
make <- function(cls) function(x) inherits(x, cls)
forced <- FALSE
code <- crbcc::cmpfun(make({ forced <- TRUE; "foo" }))
stopifnot(!forced)
delayedAssign("fun", { forced <- TRUE; identity })
code <- crbcc::cmpfun(function(x) fun(x))
stopifnot(forced)
## A missing argument and an active binding in the closure environment.
same((function(a) function() a)())
f <- function() ab
environment(f) <- new.env()
makeActiveBinding("ab", function() stop("the compiler called the binding"), environment(f))
same(f)

## The value of an active binding has no other reference. The GC must not
## free it while the compiler examines the call.
makeActiveBinding("ab", function() function(a) NULL, environment())
f <- function() ab(1, 2)
expected <- capture.output(invisible(compiler::cmpfun(f, opts)))
for (step in 2:10) {
  gctorture2(step)
  out <- capture.output(invisible(crbcc::cmpfun(f, opts)))
  gctorture2(0)
  stopifnot(identical(out, expected))
}

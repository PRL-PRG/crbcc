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

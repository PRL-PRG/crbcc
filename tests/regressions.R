## Regressions that the install benchmarks found (xml2, vctrs).
opts <- list(suppressAll = FALSE)
same <- function(f) stopifnot(crbcc:::identical_strict(
  compiler::cmpfun(f), crbcc::cmpfun(f, options = opts)))
## The name of the first argument of a replacement call.
same(function(x, v) { f(x = x, a = 1) <- v; x })
## ..1 and a compiler note in a closure with srcrefs.
same(eval(parse(text = "function(...) { g(..1) }", keep.source = TRUE)))
same(eval(parse(text = "function(x) { g(..1) }", keep.source = TRUE)))

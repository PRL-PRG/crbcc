## cmpfile(verbose = TRUE) prints the same output as compiler::cmpfile.
src <- tempfile(fileext = ".R")
dst <- tempfile(fileext = ".Rc")
writeLines(c(
  "f <- function(x) x + 1",
  "y <- f(1)",
  "if (y > 1) print(\"deparse cuts this long expression\")"
), src)
output <- function(cmpfile) capture.output(cmpfile(src, dst, verbose = TRUE))
expected <- output(compiler::cmpfile)
stopifnot(length(expected) == 4, identical(output(crbcc::cmpfile), expected))

## Compiles each closure of a package (`crbcc:::package_closures`) for the
## suites check and compile of crbcc_bench.py. The compiler options are the
## options of R CMD INSTALL, thus a compiler can write a note to stdout.
##
## Usage: Rscript closures.R check <package>
##        Rscript closures.R gnur|crbcc <package> <passes>
##
##   check        Compiles each closure with GNU R and with crbcc, and compares
##                the results (`crbcc:::identical_strict`). Writes
##                "RBC closures: <n>". If a result is different or a compiler
##                stops with an error, the script stops with an error.
##   gnur, crbcc  Compiles each closure with one compiler: one warm-up pass
##                without a measurement, then <passes> timed passes. Each
##                timed pass starts with gc() and compiles the closures in a
##                new random order. For each closure and timed pass, writes
##                "RBC closure<TAB><pass><TAB><s><TAB><size><TAB><name>". <s>
##                is the compile time and <size> is `crbcc:::function_size`.
##
## If the package has no closures, the script stops with an error.

args <- commandArgs(trailingOnly = TRUE)
mode <- args[1]
stopifnot(mode %in% c("check", "gnur", "crbcc"), length(args) == if (mode == "check") 2 else 3)
## R CMD INSTALL also compiles with the JIT off. With the JIT on, a JIT
## compile can occur in a measurement.
invisible(compiler::enableJIT(0))
opts <- list(suppressAll = FALSE, suppressUndefined = TRUE,
             suppressNoSuperAssignVar = TRUE)
funs <- crbcc:::package_closures(args[2])
if (length(funs) == 0) stop("the package has no closures", call. = FALSE)

if (mode == "check") {
  bad <- character()
  for (name in names(funs)) {
    same <- tryCatch(
      crbcc:::identical_strict(compiler::cmpfun(funs[[name]], opts),
                               crbcc::cmpfun(funs[[name]], opts)),
      error = function(e) {
        message(name, ": ", conditionMessage(e))
        FALSE
      })
    if (!same) bad <- c(bad, name)
  }
  cat(sprintf("RBC closures: %d\n", length(funs)))
  if (length(bad))
    stop(length(bad), " closures differ or do not compile: ",
         paste(head(bad, 10), collapse = ", "), call. = FALSE)
} else {
  cmpfun <- if (mode == "gnur") compiler::cmpfun else crbcc::cmpfun
  size <- vapply(funs, crbcc:::function_size, numeric(1))
  ## The first compile of a closure can load code of the compiler and force
  ## lazy-load promises. The warm-up pass does these first compiles, thus
  ## the timed passes are warm.
  for (f in funs) cmpfun(f, opts)
  for (pass in seq_len(as.integer(args[3]))) {
    invisible(gc())
    ## A new random order in each pass moves a garbage collection to a
    ## different closure in each pass. Thus the median of the passes removes
    ## its time from the closure values.
    for (name in sample(names(funs))) {
      f <- funs[[name]]
      ## .Internal(Sys.time()) is the R clock with the smallest overhead.
      t0 <- .Internal(Sys.time())
      cmpfun(f, opts)
      t <- .Internal(Sys.time()) - t0
      cat(sprintf("RBC closure\t%d\t%.9g\t%.0f\t%s\n", pass, t, size[[name]], name))
    }
  }
}

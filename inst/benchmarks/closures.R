## Compiles each closure of a package (`crbcc:::package_closures`) for the
## suites check and compile of crbcc_bench.py. The compiler options are the
## options of R CMD INSTALL, thus a compiler can write a note to stdout.
##
## Usage: Rscript closures.R check|gnur|crbcc <package>
##
##   check        Compiles each closure with GNU R and with crbcc, and compares
##                the results (`crbcc:::identical_strict`). Writes
##                "RBC closures: <n>". If a result is different or a compiler
##                stops with an error, the script stops with an error.
##   gnur, crbcc  Compiles each closure with one compiler. For each closure,
##                writes "RBC closure<TAB><s><TAB><size><TAB><name>". <s> is
##                the compile time and <size> is `crbcc:::function_size`.
##                Each run compiles the closures in a different random order.
##
## If the package has no closures, the script stops with an error.

args <- commandArgs(trailingOnly = TRUE)
mode <- args[1]
stopifnot(length(args) == 2, mode %in% c("check", "gnur", "crbcc"))
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
  ## The first compile loads code of the compiler. Thus do it before the
  ## measurements.
  invisible(cmpfun(funs[[1]], opts))
  ## A new R process makes its random seed from the time and the process ID.
  ## Thus a GC or the first use of a lazy-load promise occurs in a different
  ## closure in each run, and the median of the runs removes its time.
  for (name in sample(names(funs))) {
    f <- funs[[name]]
    ## .Internal(Sys.time()) is the R clock with the smallest overhead.
    t0 <- .Internal(Sys.time())
    cmpfun(f, opts)
    t <- .Internal(Sys.time()) - t0
    cat(sprintf("RBC closure\t%.9g\t%.0f\t%s\n", t, crbcc:::function_size(f), name))
  }
}

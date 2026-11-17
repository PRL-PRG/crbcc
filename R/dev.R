# Functions for the tests and the benchmarks. The package does not export
# them. Use them with `crbcc:::`, for example `crbcc:::package_closures()`.
#
# R CMD check does not accept `.Internal()`, thus this file uses
# `compiler::disassemble()`.

#' Returns the closures of the namespace of a package
#'
#' Each closure has its R code as the body, not the byte code of the
#' installed package.
#'
#' @param package The name of the package.
#' @return A named list of closures.
#' @noRd
package_closures <- function(package) {
  ns <- asNamespace(package)
  funs <- mget(ls(ns, all.names = TRUE), envir = ns)
  funs <- Filter(function(f) typeof(f) == "closure", funs)
  lapply(funs, function(f) {
    body(f) <- body(f)
    f
  })
}

#' Compares two objects with `identical()` and all its strict flags
#'
#' The result of `crbcc::cmpfun` must be the same as the result of
#' `compiler::cmpfun` in this sense.
#'
#' @return `TRUE` if `x` and `y` are the same.
#' @noRd
identical_strict <- function(x, y) {
  identical(x, y,
    num.eq             = FALSE,
    single.NA          = FALSE,
    attrib.as.set      = FALSE,
    ignore.bytecode    = FALSE,
    ignore.environment = FALSE,
    ignore.srcref      = FALSE,
    extptr.as.ref      = TRUE
  )
}

#' Returns the size of a closure in AST nodes
#'
#' The size is the sum of the sizes of the default values of the formals and
#' of the body. A call is one node plus the nodes of the function and of the
#' arguments. Each symbol, constant and empty argument is one node.
#'
#' @param f A closure.
#' @return The number of nodes.
#' @noRd
function_size <- function(f) {
  # The value of an empty argument (`x[, 1]`) or of a formal without a
  # default value is the empty symbol. Do not put it in a variable. Do not
  # give it to `size()`. R then stops with the error "argument is missing".
  size <- function(e) {
    if (!is.call(e) && !(is.pairlist(e) && length(e) > 0)) return(1)
    n <- as.numeric(is.call(e))
    for (i in seq_along(e))
      n <- n + if (identical(e[[i]], quote(expr = ))) 1 else size(e[[i]])
    n
  }
  (if (length(formals(f)) > 0) size(formals(f)) else 0) + size(body(f))
}

#' Returns the compile time of a closure
#'
#' `Sys.time()` has a resolution of approximately 1 us, but crbcc compiles a
#' small closure in less than 10 us. Thus the function compiles `f` in
#' batches, and each batch takes a minimum of `min_batch` seconds. The result
#' is the median of the batch times, divided by the number of compilations in
#' a batch.
#'
#' @param cmpfun The compile function, for example `compiler::cmpfun`.
#' @param f A closure.
#' @param iterations The number of batches.
#' @param min_batch The minimum time of a batch, in seconds.
#' @return The time of one compilation, in seconds.
#' @noRd
compile_time <- function(cmpfun, f, iterations = 10, min_batch = 1e-4) {
  now <- function() as.numeric(Sys.time())

  # The result does not include the first compilation. Its time gives the
  # number of compilations in a batch.
  t0 <- now()
  cmpfun(f)
  n <- max(1, ceiling(min_batch / max(now() - t0, 1e-7)))

  times <- vapply(seq_len(iterations), function(i) {
    t0 <- now()
    for (j in seq_len(n)) cmpfun(f)
    (now() - t0) / n
  }, 0)

  stats::median(times)
}

#' Returns the statistics of the byte code of closures
#'
#' The values are the sums for all code objects, for example closure bodies
#' and promises. A closure without byte code does not change the values.
#' `cmpfun` does not compile some closures, for example closures with
#' `browser()`.
#'
#' @param funs A closure or a list of closures.
#' @return A named numeric vector:
#'   - code_objects: the number of code objects
#'   - instructions: the number of instructions
#'   - pool_entries: the number of constant pool entries
#'   - pool_bytes: `object.size()` of the constants, without code objects
#'   - pool_max: the maximum number of entries in one constant pool
#' @noRd
bytecode_stats <- function(funs) {
  if (is.function(funs)) funs <- list(funs)

  stats <- c(code_objects = 0, instructions = 0, pool_entries = 0,
             pool_bytes = 0, pool_max = 0)

  # `compiler::disassemble()` gives `list(.Code, code, consts)`. It also
  # writes the result to stdout with `dput()`.
  disassemble <- function(x) {
    sink(nullfile())
    on.exit(sink())
    compiler::disassemble(x)
  }

  # A code object in `consts` is a list of the same type. MAKECLOSURE has a
  # code object as byte code in a list.
  walk <- function(d) {
    consts <- d[[3]]
    stats[["code_objects"]] <<- stats[["code_objects"]] + 1
    # In code, an opcode is a symbol and an operand is an integer.
    stats[["instructions"]] <<- stats[["instructions"]] + sum(vapply(d[[2]], is.symbol, NA))
    stats[["pool_entries"]] <<- stats[["pool_entries"]] + length(consts)
    stats[["pool_max"]] <<- max(stats[["pool_max"]], length(consts))
    for (k in consts) {
      if (is.list(k) && length(k) > 0 && identical(k[[1]], quote(.Code))) {
        walk(k)
        next
      }
      # `object.size()` includes the code objects. Thus remove them from `k`
      # and count them with `walk()`.
      if (is.list(k)) {
        code <- vapply(k, typeof, "") == "bytecode"
        for (e in k[code]) walk(disassemble(e))
        k <- k[!code]
      }
      stats[["pool_bytes"]] <<- stats[["pool_bytes"]] + as.numeric(utils::object.size(k))
    }
  }

  for (f in funs) {
    # If `f` has no byte code, `disassemble()` stops with an error.
    d <- tryCatch(disassemble(f), error = function(e) NULL)
    if (!is.null(d)) walk(d)
  }

  stats
}

# Benchmarks --------------------------------------------------------------

#' Measures the compile time of all closures of a package
#'
#' Writes these lines to stdout:
#'   RBC compile: <s>   the time to compile all closures
#'   RBC heap: <MB>     the maximum R heap during the compilation, minus the
#'                      heap before it (`gc()` "max used" of Ncells and Vcells)
#'
#' @param package The name of the package.
#' @param compiler "gnur" (`compiler::cmpfun`) or "crbcc" (`crbcc::cmpfun`).
#' @noRd
bench_package <- function(package, compiler = c("gnur", "crbcc")) {
  cmp <- if (match.arg(compiler) == "gnur") compiler::cmpfun else cmpfun
  funs <- package_closures(package)

  m0 <- gc(reset = TRUE)
  t0 <- Sys.time()
  for (f in funs) cmp(f)
  t1 <- Sys.time()
  m1 <- gc()

  cat(sprintf("RBC compile: %.6f\n", as.numeric(t1 - t0, units = "secs")))
  # Column 2 is "used (Mb)". The last column is "max used (Mb)".
  cat(sprintf("RBC heap: %.1f\n", sum(m1[, ncol(m1)]) - sum(m0[, 2])))
  invisible()
}

#' Compiles one closure with GNU R and crbcc
#'
#' Compares the two results and measures the compile time of each compiler
#' (`compile_time`). Writes one line to stdout, with tabs between the values:
#'   RBC fn <key> <size> <time GNU R (s)> <time crbcc (s)>
#' If the results are not the same (`identical_strict`), it also writes:
#'   RBC differs: <key>
#'
#' @param f A closure.
#' @param key The name of the closure in the output.
#' @param iterations The number of batches of `compile_time`.
#' @param options The options of `crbcc::cmpfun`.
#' @return The result of `crbcc::cmpfun`. If crbcc stops with an error, `f`.
#' @noRd
bench_closure <- function(f, key, iterations = 10, options = list()) {
  gnur  <- function(f) compiler::cmpfun(f)
  crbcc <- function(f) cmpfun(f, options = options)
  try_cmp <- function(cmp) tryCatch(cmp(f), error = function(e) f)

  res <- try_cmp(crbcc)
  if (!identical_strict(try_cmp(gnur), res))
    cat(sprintf("RBC differs: %s\n", key))

  # Do not measure the time of a closure that a compiler cannot compile.
  t <- tryCatch(c(compile_time(gnur, f, iterations), compile_time(crbcc, f, iterations)),
                error = function(e) c(NA, NA))
  cat(sprintf("RBC fn\t%s\t%.0f\t%.9g\t%.9g\n", key, function_size(f), t[1], t[2]))
  res
}

#' Compiles each closure of a package with GNU R and crbcc
#'
#' Uses `bench_closure` for each closure. The key is the name of the closure.
#'
#' @param package The name of the package.
#' @param iterations The number of batches of `compile_time`.
#' @noRd
bench_functions <- function(package, iterations = 10) {
  funs <- package_closures(package)
  for (name in names(funs)) bench_closure(funs[[name]], name, iterations)
  invisible()
}

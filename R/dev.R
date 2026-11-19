# Functions for the tests and the benchmarks. The package does not export
# them. Use them with `crbcc:::`, for example `crbcc:::package_closures()`.
#
# R CMD check does not accept `.Internal()`, thus this file uses
# `compiler::disassemble()`.

#' Returns the closures that a package defines
#'
#' The closures come from three sources: the functions of the namespace, the
#' S4 methods of its method tables, and the methods of its R6 class
#' generators. Each closure has its R code as the body, not the byte code of
#' the installed package.
#'
#' The name of a function is its name in the namespace. The name of an S4
#' method is "<generic>:<package of the generic>,<signature>", for example
#' "show:methods,dgCMatrix". The name of an R6 method is
#' "<generator>$<list>$<method>", for example "process$public_methods$kill".
#'
#' @param package The name of the package.
#' @return A named list of closures.
#' @noRd
package_closures <- function(package) {
  ns <- asNamespace(package)
  # The value of .Last.value in base changes after each top-level call.
  objs <- mget(setdiff(ls(ns, all.names = TRUE), ".Last.value"), envir = ns)
  funs <- Filter(function(f) typeof(f) == "closure", objs)
  # A method is a method of this package only if topenv() of its environment
  # is the namespace. For example, a derived default S4 method can wrap a
  # function of a different package.
  methods <- list()
  add <- function(name, m) {
    if (typeof(m) == "closure" && identical(topenv(environment(m)), ns))
      methods <<- c(methods, structure(list(m), names = name))
  }
  # A method table .__T__<generic>:<package> has one method for each
  # signature.
  for (t in grep("^\\.__T__", names(objs), value = TRUE))
    for (sig in ls(objs[[t]], all.names = TRUE))
      add(paste0(substring(t, 7), ",", sig), get(sig, envir = objs[[t]], inherits = FALSE))
  # An R6 class generator is an environment with three lists of methods.
  for (g in names(Filter(function(o) inherits(o, "R6ClassGenerator"), objs)))
    for (part in c("public_methods", "private_methods", "active"))
      for (m in names(objs[[g]][[part]]))
        add(paste(g, part, m, sep = "$"), objs[[g]][[part]][[m]])
  funs <- c(funs, methods)
  if (anyDuplicated(names(funs)))
    stop("a closure name is not unique: ", names(funs)[anyDuplicated(names(funs))])
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

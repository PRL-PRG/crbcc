#' Compile R Function to Bytecode
#'
#' Compiles an R function to bytecode using a C-based compiler.
#'
#' @param fun A closure to be compiled to bytecode. If fun is a builtin or
#'   special function, cmpfun returns it without a change.
#' @param options A list of compilation options. If options is NULL, cmpfun
#'   uses the default options.
#'
#' @return bytecode compiled function
#'
#' @export
cmpfun <- function(fun, options = list()) {
  # The same as compiler::cmpfun.
  if (typeof(fun) %in% c("builtin", "special")) {
    return(fun)
  }
  if (!is_closure(fun)) {
    stop("cannot compile a non-function")
  }

  if (!is.null(options) && !is.list(options)) {
    stop("Argument 'options' must be a list")
  }

  .Call(C_cmpfun, fun, options)
}

#' Check if Object is a Closure
#'
#' Internal helper function to check if an object is a closure
#' (user-defined function as opposed to built-in functions).
#'
#' @param x Object to test
#' @return Logical value indicating if x is a closure
#' @keywords internal
is_closure <- function(x) {
  is.function(x) && !is.primitive(x) && typeof(x) == "closure"
}

#' Compile a Generic Language Object
#'
#' Compiles a generic R language object (such as an expression or function) to bytecode using the C-based compiler backend.
#'
#' @param e An R language object to compile (expression, function, etc.).
#' @param env The environment in which to evaluate the object (default: .GlobalEnv).
#' @param options A list of compilation options (default: NULL).
#' @param srcref Optional source reference information (default: NULL).
#'
#' @return The compiled object, or invisible NULL if compilation is only for side effect.
#' @export
compile <- function(e, env = .GlobalEnv, options = NULL, srcref = NULL) {
  .Call(C_compile, e, env, options, srcref)
}

#' Compile an R Source File
#'
#' Compiles all top-level expressions in an R source file to bytecode and saves the result to an output file.
#'
#' @param infile Path to the input R source file.
#' @param outfile Path to the output file.
#' @param ascii Logical; whether to write an ASCII file.
#' @param env The top-level environment in which to evaluate expressions (default: .GlobalEnv).
#' @param verbose Logical; Not yet implemented, print compilation progress messages.
#' @param options List of compilation options.
#' @param version Optional file format version.
#'
#' @return Invisible NULL. Output is written to file as a side effect.
#' @export
cmpfile <- function(infile, outfile, ascii = FALSE, env = .GlobalEnv, verbose = FALSE, options = NULL, version = NULL) {

  if (verbose) {
    warning("verbose not yet implemented, defaulting to false")
    verbose <- FALSE
  }

  if (missing(outfile)) {
    basename <- sub("\\.[a-zA-Z0-9]$", "", infile)
    outfile <- paste0(basename, ".Rc")
    stop("an output file must be provided, eg. ", outfile)
  }

  if (infile == outfile)
    stop("input and output file names are the same")

  if (!is.environment(env) || !identical(env, topenv(env)))
    stop("'env' must be a top level environment")

  forms <- parse(infile)
  nforms <- length(forms)
  srefs <- attr(forms, "srcref")

  if (nforms > 0) {

    expr.needed <- 1000
    expr.old <- getOption("expressions")

    if (expr.old < expr.needed) {
      options(expressions = expr.needed)
      on.exit(options(expressions = expr.old))
    }

    cforms <- vector("list", nforms)
    cforms <- .Call(C_cmpfile, env, options, forms, nforms, cforms, srefs, verbose)

    message(gettextf("saving to file \"%s\" ... ", outfile))
    saveRDS(cforms, file = outfile, ascii = ascii, version = version)
    message(gettext("done"), "\n", sep = "")

  } else
    warning("empty input file; no output written")

  invisible(NULL)
}

#' Load a Compiled R File
#'
#' Loads bytecode compiled by \code{\link{cmpfile}} into an environment.
#'
#' @param file Path to the \code{.Rc} file compiled by 'crbcc' via 'cmpfile'.
#' @param envir The environment to load into, use .GlobalEnv to load the code into current global session
#'
#' @return Invisible NULL.
#' @export
loadcmp <- function(file, envir) {
  if (!is.environment(envir))
    stop("'envir' must be an environment")
  cforms <- readRDS(file)
  for (form in cforms)
    eval(form, envir = envir)
  invisible(NULL)
}

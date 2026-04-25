#' Compile R Function to Bytecode
#'
#' Compiles an R function to bytecode using a C-based compiler.
#'
#' @param fun A closure to be compiled to bytecode
#' @param options A list of compilation options
#'
#' @return bytecode compiled function
#'
#' @export
cmpfun <- function(fun, options = list()) {
  if (!is_closure(fun)) {
    stop("Argument 'fun' must be a closure (user-defined function)")
  }

  if (!is.list(options)) {
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

is_compiled <- function(fun) {
  if (!is_closure(fun)) {
    stop("Argument 'fun' must be a closure (user-defined function)")
  }
  .Call(C_is_compiled, fun)
}


compile <- function(e, env = .GlobalEnv, options = NULL, srcref = NULL) {
  .Call(C_compile, e, env, options, srcref)
}

# Boilerplate around .Call taken from the R compiler
cmpfile <- function(infile, outfile, ascii = FALSE, env = .GlobalEnv, verbose = FALSE, options = NULL, version = NULL) {
  
  if (missing(outfile)) {
    basename <- sub("\\.[a-zA-Z0-9]$", "", infile)
    outfile <- paste0(basename, ".Rc")
  }

  if (infile == outfile)
    stop("input and output file names are the same")

  if (! is.environment(env) || ! identical(env, topenv(env)))
    stop("’env’ must be a top level environment")

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

    cat(gettextf("saving to file \"%s\" ... ", outfile))
    .Internal(save.to.file(cforms, outfile, ascii, version))
    cat(gettext("done"), "\n", sep = "")

  } else
    warning("empty input file; no output written");

  invisible(NULL)

} 


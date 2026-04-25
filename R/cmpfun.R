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

# Use GNU compiler::cmpfile pipeline while injecting crbcc::cmpfun.
cmpfile <- function(infile, outfile, ascii = FALSE, env = .GlobalEnv,
                    verbose = FALSE, options = NULL, version = NULL) {
  if (!requireNamespace("compiler", quietly = TRUE)) {
    stop("Package 'compiler' is required")
  }

  compiler_ns <- asNamespace("compiler")
  original_cmpfun <- get("cmpfun", envir = compiler_ns, inherits = FALSE)

  shim_cmpfun <- function(fun, options = NULL) {
    if (is.null(options)) {
      options <- list()
    }
    cmpfun(fun, options)
  }

  unlockBinding("cmpfun", compiler_ns)
  assign("cmpfun", shim_cmpfun, envir = compiler_ns)
  lockBinding("cmpfun", compiler_ns)

  on.exit({
    unlockBinding("cmpfun", compiler_ns)
    assign("cmpfun", original_cmpfun, envir = compiler_ns)
    lockBinding("cmpfun", compiler_ns)
  }, add = TRUE)

  compiler::cmpfile(
    infile = infile,
    outfile = outfile,
    ascii = ascii,
    env = env,
    verbose = verbose,
    options = options,
    version = version
  )
}

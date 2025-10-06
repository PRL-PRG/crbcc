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

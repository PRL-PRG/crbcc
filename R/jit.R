# enable_jit() is not active. It replaces compiler:::tryCmpfun and
# compiler:::tryCompile, the two functions that the JIT of R uses (R_cmpfun1
# and R_compileExpr in src/main/eval.c of R). The CRAN policy permits a change
# to the namespace of a different package only with the agreement of the
# maintainer of that package. We do not have the agreement of Luke Tierney,
# the maintainer of the compiler package, yet.
#
# When we have the agreement, remove the comment characters, add
# export(enable_jit) to NAMESPACE and add man/enable_jit.Rd.
#
# jit_state <- new.env(parent = emptyenv())
#
# #' Makes the JIT of R use crbcc
# #'
# #' Works as `compiler::enableJIT()`. If level is more than 0, the JIT
# #' compiles closures and loops with crbcc. If level is 0, the JIT is off and
# #' the compiler package gets back its functions. If level is negative, the
# #' function only returns the current level.
# #'
# #' @param level The JIT level, as in `compiler::enableJIT()`.
# #' @return The previous JIT level.
# #' @export
# enable_jit <- function(level) {
#   old <- compiler::enableJIT(level)
#   if (level < 0) return(old)
#   ns <- asNamespace("compiler")
#   if (is.null(jit_state$tryCmpfun)) {
#     jit_state$tryCmpfun <- get("tryCmpfun", ns)
#     jit_state$tryCompile <- get("tryCompile", ns)
#   }
#   ## If crbcc cannot compile, use the function of the compiler package.
#   funs <- if (level > 0) list(
#     tryCmpfun = function(f)
#       tryCatch(cmpfun(f), error = function(err) jit_state$tryCmpfun(f)),
#     tryCompile = function(e, ...)
#       tryCatch(compile(e, ...), error = function(err) jit_state$tryCompile(e, ...)))
#   else list(tryCmpfun = jit_state$tryCmpfun, tryCompile = jit_state$tryCompile)
#   for (name in names(funs)) {
#     unlockBinding(name, ns)
#     assign(name, funs[[name]], envir = ns)
#     lockBinding(name, ns)
#   }
#   old
# }

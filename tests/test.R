library(crbcc)

## ---- helpers ---------------------------------------------------------------
.disas <- function(fun) {
  if (!crbcc:::is_compiled(fun)) {
    return(fun)
  }
  capture.output(code <- compiler::disassemble(fun)[1])
  code
}

eq <- function(name, a, b) {
  if (!identical(a, b)) {
    cat("---- MISMATCH in:", name, "----\n", file = stderr())
    cat("C cmpfun disassembly:\n", a, "\n\n", file = stderr())
    cat("R compiler disassembly:\n", b, "\n", file = stderr())
    stop(sprintf("Bytecode mismatch for %s", name), call. = FALSE)
  } else {
    cat(sprintf("[OK] %s\n", name))
  }
}

## ---- tests: constants ------------------------------------------------------
cases <- list(
  NULL_const    = function() NULL,
  TRUE_const    = function() TRUE,
  FALSE_const   = function() FALSE,
  int_const     = function() 42L,
  real_const    = function() 3.25,
  chr_const     = function() "hey",
  cplx_const    = function() 2i
)

for (nm in names(cases)) {
  f <- cases[[nm]]
  cat("Checking: ", deparse(f), "\n")
  ours <- cmpfun(f)
  theirs <- compiler::cmpfun(f)
  eq(nm, .disas(ours), .disas(theirs))
}

## ---- tests: non-constant fallback -----------------------------------------
nonconst <- function(x) x + 1
g <- cmpfun(nonconst, list())

if (!identical(g, nonconst)) {
  stop("Non-constant function should be returned unchanged", call. = FALSE)
}

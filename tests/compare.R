#  crbcc: C R Bytecode Compiler
#  tests/compare.R

#  Copyright (C) 2026 Josef Malý
#  Copyright (C) 2026 Faculty of Information Technology, CTU in Prague

#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or (at
#  your option) any later version.

#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.

if (!requireNamespace("compiler", quietly = TRUE)) stop("Package 'compiler' is required.")
if (!requireNamespace("crbcc", quietly = TRUE)) stop("Package 'crbcc' is required.")

PACKAGES_CRAN = c("base", "compiler")

PACKAGES_BASE = c(
  # "datasets" and "translations" omitted since they contains no closures 
  "base", "compiler", "graphics", "grDevices", "grid",
  "methods", "parallel", "splines", "stats", "stats4", "tcltk", "tools",
  "utils"
) 

PACKAGES_RECOMMENDED = c(
  # Recommended
  "KernSmooth", "MASS", "Matrix", "boot", "class", "cluster", "codetools",
  "foreign", "lattice", "mgcv", "nlme", "nnet", "rpart", "spatial", "survival"
)

# Define corpus here, using a
# lightweight test for CRAN
PACKAGES <- unique(c(PACKAGES_CRAN, PACKAGES_BASE, PACKAGES_RECOMMENDED))

OPTIONS = list(
  optimize=2
)

grand_total_funs   <- 0L
grand_total_lines  <- 0L
grand_total_instrs <- 0L

compare_compilers <- function(prog) {

  res_compiler <- compiler::cmpfun(prog, options=OPTIONS)
  res_crbcc    <- crbcc::cmpfun(prog, options=OPTIONS)
  grand_total_instrs <<- grand_total_instrs + crbcc:::bytecode_stats(res_crbcc)[["instructions"]]

  crbcc:::identical_strict(res_compiler, res_crbcc)

}

test_package <- function(package, torture=FALSE) {
  if (!requireNamespace(package, quietly = TRUE)) {
    message(sprintf("\n[SKIP] Package not available: %s\n", package))
    return(invisible(NULL))
  }

  uncompiled_funs <- crbcc:::package_closures(package)

  compiled_ok <- 0L
  total_lines <- 0L

  for (i in seq_along(uncompiled_funs)) {
    func_id <- names(uncompiled_funs)[i]

    tryCatch({
      res <- compare_compilers(uncompiled_funs[[i]])
      if (!res) {
        message(sprintf("[MISMATCH] Compilers are NOT identical!\n"))
        message(sprintf("Stopped at: %s\n", func_id))
        stop("Test suite halted due to fatal crash.")
      }
    }, error = function(e) {
      message(sprintf("[FATAL] Compiler crashed!\n"))
      message(sprintf("Stopped at: %s\n", func_id))
      message(sprintf("Error: %s\n", e$message))

      stop("Test suite halted due to fatal crash.")
    })

    total_lines <- total_lines + length(deparse(body(uncompiled_funs[[i]])))
    compiled_ok <- compiled_ok + 1L
  }

  grand_total_funs  <<- grand_total_funs  + compiled_ok
  grand_total_lines <<- grand_total_lines + total_lines

  message(sprintf("\n[OK] %d functions | %d lines compiled identically\n", compiled_ok, total_lines))
}

for (i in PACKAGES) {
  message(sprintf("\n[START] Compiling package: %s ", i))
  test_package(i)
}

message(sprintf("\n%s\n[TOTAL] %d functions | %d lines | %d opcodes compiled across %d packages\n%s\n",
  strrep("=", 60),
  grand_total_funs,
  grand_total_lines,
  grand_total_instrs,
  length(PACKAGES),
  strrep("=", 60)
))

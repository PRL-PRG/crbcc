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

OPTIONS = list(
  optimize=2
)

PACKAGES = c(
  # Base - "datasets" omitted since it has no closures 
  "base", "compiler", "graphics", "grDevices", "grid",
  "methods", "parallel", "splines", "stats", "stats4", "tcltk", "tools",
  "translations", "utils",
  # Recommended
  "KernSmooth", "MASS", "Matrix", "boot", "class", "cluster", "codetools",
  "foreign", "lattice", "mgcv", "nlme", "nnet", "rpart", "spatial", "survival"
)

grand_total_funs   <- 0L
grand_total_lines  <- 0L
grand_total_instrs <- 0L

bytecode_size <- function(xs) {
  if (is.list(xs)) {
    sum(vapply(as.list(xs), bytecode_size, integer(1L)))
  } else if (is.function(xs) || typeof(xs) == "bytecode") {
    tryCatch({
      capture.output(body <- compiler::disassemble(xs))
      bc     <- body[[2]]
      ops    <- vapply(as.list(bc), is.symbol, logical(1L))
      consts <- body[[3]]
      sum(ops) + bytecode_size(consts)
    }, error = function(e) 0L)
  } else {
    0L
  }
}

compare_compilers <- function(prog) {
  res_compiler <- compiler::cmpfun(prog, options=OPTIONS)
  res_crbcc    <- crbcc::cmpfun(prog, options=OPTIONS)

  grand_total_instrs <<- grand_total_instrs + bytecode_size(res_crbcc)

  identical(res_compiler, res_crbcc,
    num.eq             = FALSE,
    single.NA          = FALSE,
    attrib.as.set      = FALSE,
    ignore.bytecode    = FALSE,
    ignore.environment = FALSE,
    ignore.srcref      = FALSE,
    extptr.as.ref      = TRUE
  )
}

test_package <- function(package, torture=FALSE) {
  if (!requireNamespace(package, quietly = TRUE)) {
    cat(sprintf("\n[SKIP] Package not available: %s\n", package))
    return(invisible(NULL))
  }

  ns          <- getNamespace(package)
  symbols     <- ls(ns, all.names=T)
  objects     <- lapply(symbols, get, envir=ns)
  is_closure  <- sapply(objects, \(x) typeof(x) == "closure")
  funs        <- objects[is_closure]
  func_names  <- symbols[is_closure]

  uncompiled_funs <- lapply(funs, function(f) {
    res <- f
    body(res) <- body(f)
    return(res)
  })
  names(uncompiled_funs) <- func_names

  compiled_ok <- 0L
  total_lines <- 0L

  for (i in seq_along(uncompiled_funs)) {
    func_id <- names(uncompiled_funs)[i]

    tryCatch({
      res <- compare_compilers(uncompiled_funs[[i]])
      if (!res) {
        cat(sprintf("[MISMATCH] Compilers are NOT identical!\n"))
        cat(sprintf("Stopped at: %s\n", func_id))
        stop("Test suite halted due to fatal crash.")
      }
    }, error = function(e) {
      cat(sprintf("[FATAL] Compiler crashed!\n"))
      cat(sprintf("Stopped at: %s\n", func_id))
      cat(sprintf("Error: %s\n", e$message))
      readLines(con = "stdin", n = 1)
      stop("Test suite halted due to fatal crash.")
    })

    total_lines <- total_lines + length(deparse(body(uncompiled_funs[[i]])))
    compiled_ok <- compiled_ok + 1L
  }

  grand_total_funs  <<- grand_total_funs  + compiled_ok
  grand_total_lines <<- grand_total_lines + total_lines

  cat(sprintf("\n[OK] %d functions | %d lines compiled identically\n", compiled_ok, total_lines))
}

for (i in PACKAGES) {
  cat(sprintf("\n[START] Compiling package: %s ", i))
  test_package(i)
}

cat(sprintf("\n%s\n[TOTAL] %d functions | %d lines | %d opcodes compiled across %d packages\n%s\n",
  strrep("=", 60),
  grand_total_funs,
  grand_total_lines,
  grand_total_instrs,
  length(PACKAGES),
  strrep("=", 60)
))
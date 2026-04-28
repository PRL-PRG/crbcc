#  crbcc: C R Bytecode Compiler
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

PACKAGES = c(
  "base", "compiler", "graphics", "grDevices", "grid",
  "methods", "parallel", "splines", "stats", "stats4", "tcltk"
)

compare_compilers <- function(prog) {

  res_compiler <- compiler::cmpfun(prog, options=list(optimize=2)) 
  res_crbcc <- crbcc::cmpfun(prog)

  identical(res_compiler, res_crbcc,
    num.eq          = FALSE,  # compare numerics bitwise
    single.NA       = FALSE,  # treat each NA flavour as distinct bits
    attrib.as.set   = FALSE,  # attribute order must match exactly
    ignore.bytecode = FALSE,  # include compiled bytecode
    ignore.environment = FALSE, # closure environments must match
    ignore.srcref   = FALSE,  # include source references
    extptr.as.ref   = TRUE    # external pointers compared by address
  )

}

test_package <- function(package, torture=FALSE) {

  if (!requireNamespace(package, quietly = TRUE)) {
    cat(sprintf("\n[SKIP] Package not available: %s\n", package))
    return(invisible(NULL))
  }

  ns <- getNamespace(package)
  symbols <- ls(ns, all.names=T)
  objects <- lapply(symbols, get, envir=ns)

  is_closure <- sapply(objects, \(x) typeof(x) == "closure")
  funs <- objects[is_closure]
  func_names <- symbols[is_closure]

  uncompiled_funs <- lapply(funs, function(f) {
    res <- f
    body(res) <- body(f)
    return(res)
  })

  names(uncompiled_funs) <- func_names
  compiled_ok <- 0
  speedups <- numeric(0)

  for (i in seq_along(uncompiled_funs)) {
    
    func_id <- names(uncompiled_funs)[i]

    tryCatch({

      res <- compare_compilers(uncompiled_funs[[i]])      

      if (!res) {

        cat(sprintf("\n========================================\n"))
        cat(sprintf("[MISMATCH] Compilers are NOT identical!\n"))
        cat(sprintf("Stopped at: %s\n", func_id))
        cat(sprintf("========================================\n\n"))

        stop("Test suite halted due to fatal crash.")

      }
    
    }, error = function(e) {
      cat(sprintf("\n========================================\n"))
      cat(sprintf("[FATAL] Compiler crashed!\n"))
      cat(sprintf("Stopped at: %s\n", func_id))
      cat(sprintf("Error: %s\n", e$message))
      cat(sprintf("========================================\n\n"))
           
      readLines(con = "stdin", n = 1)
      
      stop("Test suite halted due to fatal crash.")
    })

    compiled_ok <- compiled_ok + 1
    
  }

  cat(sprintf("\n[OK] %d functions compiled identically\n", compiled_ok))

}

for (i in PACKAGES) {
  cat(sprintf("\n[START] Compiling package: %s ", i))
  test_package(i)
}

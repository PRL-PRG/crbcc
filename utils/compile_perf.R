##########################################
# COMPILER BENCHMARKING (FUNCTION-BY-FUNCTION)
# ALL PATHS RELATIVE TO GIT ROOT
##########################################

# ---- CONFIG ----
COMPILER_PKG <- "compiler"
COMPILER_FUN <- compiler::cmpfun
OPTIMIZE <- 2
ITERS_PER_FN <- 10

PACKAGES_TO_TEST = c(
  # "datasets" omitted since it contains no closures 
  "base", "compiler", "graphics", "grDevices", "grid",
  "methods", "parallel", "splines", "stats", "stats4", "tools",
  # Recommended
  "KernSmooth", "MASS", "Matrix", "boot", "class", "cluster", "codetools",
  "foreign", "lattice", "mgcv", "nlme", "nnet", "rpart", "spatial", "survival"
)

OUTPUT_FILE <- "test_results/new/compile_benchmark.csv"
# ----------------

if (!requireNamespace("bench", quietly = TRUE)) stop("Package 'bench' is required.")
if (!requireNamespace(COMPILER_PKG, quietly = TRUE)) stop(sprintf("Package '%s' is required.", COMPILER_PKG))

packages <- unique(c("bench", COMPILER_PKG, PACKAGES_TO_TEST))

for (i in packages) {
  library(i, character.only = TRUE)
}

output_dir <- dirname(OUTPUT_FILE)
if (!dir.exists(output_dir)) {
  dir.create(output_dir, recursive = TRUE)
}

headers <- data.frame(
  package = character(),
  name = character(),
  time = numeric()
)

write.table(headers, file = OUTPUT_FILE, sep = ",", row.names = FALSE, col.names = TRUE)

for (pkg in PACKAGES_TO_TEST) {
  cat(sprintf("\n[BENCH] Profiling package: %s\n", pkg))

  ns <- tryCatch(getNamespace(pkg), error = function(e) NULL)
  if (is.null(ns)) {
    warning(sprintf("Skipping '%s': namespace not available.", pkg))
    next
  }

  symbols <- ls(ns, all.names = TRUE)
  objects <- lapply(symbols, get, envir = ns)

  is_closure <- sapply(objects, function(x) typeof(x) == "closure")
  funs <- objects[is_closure]
  func_names <- symbols[is_closure]

  uncompiled_funs <- lapply(funs, function(f) {
    res <- f
    body(res) <- body(f)
    res
  })

  names(uncompiled_funs) <- func_names

  for (i in seq_along(uncompiled_funs)) {
    f_name <- func_names[i]
    f_uncomp <- uncompiled_funs[[i]]

    bm <- tryCatch({
      bench::mark(
        compiled = COMPILER_FUN(f_uncomp, options = list(optimize = OPTIMIZE)),
        iterations = ITERS_PER_FN,
        check = FALSE
      )
    }, error = function(e) NULL)

    if (!is.null(bm)) {
      time_median <- as.numeric(bm$median[1])
      res_row <- data.frame(package = pkg, name = f_name, time = time_median)
      write.table(res_row, file = OUTPUT_FILE, sep = ",", row.names = FALSE, col.names = FALSE, append = TRUE)
    }
  }
}

cat("[OK] Benchmarking complete. Results saved to ", OUTPUT_FILE, "\n")

PACKAGES = c(
  "base", "compiler", "graphics", "grDevices", "grid",
  "methods", "parallel", "splines", "stats", "stats4", "tcltk",
  "tools", "utils", "boot", "class", "cluster", "codetools",
  "foreign", "KernSmooth", "lattice", "MASS", "Matrix", "mgcv",
  "nlme", "nnet", "rpart", "spatial", "survival", "ggplot2"
)

# If TRUE, ignore PACKAGES and pull top N CRAN packages by downloads.
DO_CRAN_COMPARE <- FALSE
N_CRAN <- 50
CRAN_WHEN <- "last-week"
CRAN_FALLBACK_TO_STATIC <- FALSE

# If TRUE (and DO_CRAN_COMPARE=TRUE), remove tested CRAN packages after run.
# Base/recommended packages are never removed.
REMOVE_PACKAGES_AFTER_TEST <- FALSE

# If TRUE, install missing target packages before running checks.
INSTALL_MISSING_PACKAGES <- FALSE

# Print result reference and crbcc outputs
# Outputs latest compiled without errors,
# on mismatch look at diff
DUMP_BC <- FALSE
DUMP_BC_PATH <- "tests/bc"

# Old performance benchmarks, use ./tests/perf.R
# to measure performance
BENCHMARK <- FALSE
B_ITERS <- 1
###########################

resolve_packages <- function() {
  if (!isTRUE(DO_CRAN_COMPARE)) {
    return(list(packages = PACKAGES, source = "static"))
  }

  if (requireNamespace("cranlogs", quietly = TRUE)) {
    top <- cranlogs::cran_top_downloads(when = CRAN_WHEN, count = N_CRAN)
    pkgs <- unique(top$package)
    cat(sprintf("[INFO] Using cranlogs top %d packages (%s).\n", length(pkgs), CRAN_WHEN))
    return(list(packages = pkgs, source = "cranlogs"))
  }

  if (!isTRUE(CRAN_FALLBACK_TO_STATIC)) {
    stop("DO_CRAN_COMPARE=TRUE but neither 'cranlogs' nor 'pkgsearch' is installed. Install one of them or set CRAN_FALLBACK_TO_STATIC=TRUE.")
  }

  warning("DO_CRAN_COMPARE=TRUE but neither 'cranlogs' nor 'pkgsearch' is installed; falling back to PACKAGES.")
  list(packages = PACKAGES, source = "static-fallback")
}

install_missing_packages <- function(pkgs) {
  installed_before <- rownames(installed.packages())
  missing <- setdiff(pkgs, installed_before)

  if (length(missing) == 0) {
    cat("[INFO] All target packages already installed.\n")
    return(character(0))
  }

  if (!isTRUE(INSTALL_MISSING_PACKAGES)) {
    cat(sprintf("[INFO] Missing packages (%d) will not be installed (INSTALL_MISSING_PACKAGES=FALSE).\n", length(missing)))
    return(character(0))
  }

  cat(sprintf("[INFO] Installing %d missing package(s) before checks...\n", length(missing)))
  for (pkg in missing) {
    tryCatch({
      utils::install.packages(pkg)
      cat(sprintf("[INFO] Installed: %s\n", pkg))
    }, error = function(e) {
      cat(sprintf("[WARN] Failed to install %s: %s\n", pkg, conditionMessage(e)))
    })
  }

  installed_after <- rownames(installed.packages())
  newly_installed <- intersect(missing, installed_after)
  cat(sprintf("[INFO] Newly installed this run: %d\n", length(newly_installed)))
  newly_installed
}

remove_newly_installed_packages <- function(pkgs) {
  if (!isTRUE(REMOVE_PACKAGES_AFTER_TEST)) {
    return(invisible(NULL))
  }

  if (length(pkgs) == 0) {
    cat("[INFO] No newly installed packages to remove.\n")
    return(invisible(NULL))
  }

  installed_now <- rownames(installed.packages())
  removable <- intersect(pkgs, installed_now)

  if (length(removable) == 0) {
    cat("[INFO] Newly installed packages are already absent; nothing to remove.\n")
    return(invisible(NULL))
  }

  cat(sprintf("[CLEANUP] Removing %d newly installed package(s)...\n", length(removable)))
  for (pkg in removable) {
    tryCatch({
      utils::remove.packages(pkg)
      cat(sprintf("[CLEANUP] Removed: %s\n", pkg))
    }, error = function(e) {
      cat(sprintf("[CLEANUP] Failed to remove %s: %s\n", pkg, conditionMessage(e)))
    })
  }
}


compare_bytecode_strict <- function(bc1, bc2, path = "Root") {
  
  identical(bc1, bc2,
    num.eq          = FALSE,  # compare numerics bitwise
    single.NA       = FALSE,  # treat each NA flavour as distinct bits
    attrib.as.set   = FALSE,  # attribute order must match exactly
    ignore.bytecode = FALSE,  # include compiled bytecode
    ignore.environment = FALSE, # closure environments must match
    ignore.srcref   = FALSE,  # include source references
    extptr.as.ref   = TRUE    # external pointers compared by address
  )

}

dump_all_bytecode <- function(bc, filename) {
  
  # Open a file connection for writing
  dir.create(dirname(filename), recursive = TRUE, showWarnings = FALSE)
  fc <- file(filename, open = "wt")
  
  # Ensure the file connection is closed when the function exits
  on.exit(close(fc))
  
  dump_recursive <- function(obj, path) {
    # If it's a closure, unwrap it to get the raw bytecode
    if (typeof(obj) == "closure") {
      obj <- .Internal(bodyCode(obj))
    }
    
    if (typeof(obj) == "bytecode") {
      # Print a highly visible header for the diff
      cat(sprintf("\n======================================================\n"), file = fc)
      cat(sprintf(">>> PATH: %s \n", path), file = fc)
      cat(sprintf("======================================================\n"), file = fc)
      
      # Use R's built-in disassembler for the nice opcode formatting
      capture.output(compiler::disassemble(obj), file = fc)
      cat("\n", file = fc)
      
      # Extract the constant pool to search for nested bytecode
      d <- .Internal(disassemble(obj))
      cp <- d[[3]]
      
      for (i in seq_along(cp)) {
        item <- cp[[i]]
        
        # Direct nested bytecode
        if (typeof(item) == "bytecode") {
          dump_recursive(item, sprintf("%s -> CP[%d]", path, i))
        }
        # Bytecode inside a list
        else if (is.list(item)) {
          for (j in seq_along(item)) {
            if (typeof(item[[j]]) == "bytecode") {
              dump_recursive(item[[j]], sprintf("%s -> CP[%d] -> List[%d]", path, i, j))
            }
          }
        }
      }
    }
  }
  
  dump_recursive(bc, "Root")
}

benchmark_compilers <- function(prog, iters = 10, dump_bytecode = TRUE, torture = FALSE) {

  if (!requireNamespace("compiler", quietly = TRUE)) stop("Package 'compiler' is required.")
  if (!requireNamespace("crbcc", quietly = TRUE)) stop("Package 'crbcc' is required.")

  times_compiler <- numeric(10)
  times_crbcc <- numeric(10)
  
  # 1. Benchmark compiler::cmpfun 
  for (i in 1:iters) {
    start_time <- Sys.time()
    res_compiler <- compiler::cmpfun(prog, options=list(optimize=2)) 
    end_time <- Sys.time()
    times_compiler[i] <- as.numeric(difftime(end_time, start_time, units = "secs"))
  }

  # 2. Benchmark crbcc::cmpfun
  for (i in 1:iters) {
    start_time <- Sys.time()
    gctorture(on = torture)
    res_crbcc <- crbcc::cmpfun(prog)
    gctorture(on = FALSE)
    end_time <- Sys.time()
    times_crbcc[i] <- as.numeric(difftime(end_time, start_time, units = "secs"))
  }

  # 3. Dump recursively to files if requested
  if (dump_bytecode) {
    dump_all_bytecode(res_compiler, paste0(DUMP_BC_PATH, "/reference.txt"))
    dump_all_bytecode(res_crbcc, paste0(DUMP_BC_PATH, "/crbcc.txt"))
    
  }

  # 4. Strict Bytecode Comparison (using the strict function from earlier)
  is_identical <- compare_bytecode_strict(res_compiler, res_crbcc)

  return(list(
    mean_time_compiler_sec = mean(times_compiler),
    mean_time_crbcc_sec = mean(times_crbcc),
    speedup = mean(times_compiler) / mean(times_crbcc),
    bytecode_identical = is_identical
  ))
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
    #cat(sprintf("Compiling %s...\n", func_id))

    niters <- 1

    if (BENCHMARK)
     niters <- B_ITERS 

    tryCatch({
      res <- benchmark_compilers(uncompiled_funs[[i]], niters, DUMP_BC, torture)
      

      if (!isTRUE(res$bytecode_identical)) {
        cat(sprintf("\n========================================\n"))
        cat(sprintf("[MISMATCH] Compilers are NOT identical!\n"))
        cat(sprintf("Stopped at: %s\n", func_id))
        cat(sprintf("========================================\n\n"))
        
        print(uncompiled_funs[[i]])
        readLines(con = "stdin", n = 1)
        system("clear")
        
      } else {
        #cat("[OK]\n")
        compiled_ok <- compiled_ok + 1
        
        if (!is.null(res$speedup)) {
          speedups <- c(speedups, res$speedup)
        }

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
    
  }

  avg_speedup <- if (length(speedups) > 0) mean(speedups) else NA
  
  cat(sprintf("\n[OK] %d functions compiled identically\n", compiled_ok))

  if (BENCHMARK)
    cat(sprintf("[PERF] Average compiler speedup: %.2fx\n", avg_speedup))

}

run_compare <- function() {
  resolved <- resolve_packages()
  packages_to_test <- resolved$packages
  source <- resolved$source
  cat(sprintf("[INFO] Package source: %s\n", source))

  newly_installed <- install_missing_packages(packages_to_test)
  on.exit(remove_newly_installed_packages(newly_installed), add = TRUE)

  for (i in packages_to_test) {
    cat(sprintf("\n[START] Compiling package: %s ", i))
    test_package(i)
  }
}

run_compare()

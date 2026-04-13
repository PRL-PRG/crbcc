###########################
# CORRECTNESS TESTING
# All paths relative to root, comparing crbcc
# bytecode to GNU-R
###########################
PACKAGES = c("base", "compiler", "tools", "stats", "utils")

# Print result reference and crbcc outputs
# Outputs latest compiled without errors,
# on mismatch look at diff
DUMP_BC <- FALSE
DUMP_BC_PATH <- "tests/bc"

# Old performance benchmarks, use ./tests/perf.R
# to measure performance
BENCHMARK <- FALSE
B_ITERS <- 10
###########################


compare_bytecode_strict <- function(bc1, bc2, path = "Root") {
  
  # 1. Unpack closures to get to the raw bytecode
  if (typeof(bc1) == "closure") bc1 <- .Internal(bodyCode(bc1))
  if (typeof(bc2) == "closure") bc2 <- .Internal(bodyCode(bc2))
  
  if (typeof(bc1) != typeof(bc2)) {
    message(sprintf("[%s] Type mismatch: %s vs %s", path, typeof(bc1), typeof(bc2)))
    return(FALSE)
  }
  
  if (typeof(bc1) != "bytecode") {
    is_id <- identical(bc1, bc2)
    if (!is_id) message(sprintf("[%s] Values are not identical", path))
    return(is_id)
  }
  
  # 2. Extract using .Internal(disassemble) to suppress print output
  d1 <- .Internal(disassemble(bc1))
  d2 <- .Internal(disassemble(bc2))
  
  # d[[1]] = version, d[[2]] = instruction vector, d[[3]] = constant pool
  if (!identical(d1[[1]], d2[[1]])) {
    message(sprintf("[%s] Mismatch in Bytecode Version", path))
    return(FALSE)
  }
  
  if (!identical(d1[[2]], d2[[2]])) {
    message(sprintf("[%s] Mismatch in Instruction Vector", path))
    # Optional: Find the exact instruction index
    for (idx in seq_along(d1[[2]])) {
      if (!identical(d1[[2]][[idx]], d2[[2]][[idx]])) {
        message(sprintf("     -> First opcode/operand difference at instruction index %d", idx))
        break
      }
    }
    return(FALSE)
  }
  
  cp1 <- d1[[3]]
  cp2 <- d2[[3]]
  
  if (length(cp1) != length(cp2)) {
    message(sprintf("[%s] Mismatch in Constant Pool length: %d vs %d", path, length(cp1), length(cp2)))
    return(FALSE)
  }
  
  # Helper to recursively check elements (like lists containing bytecode)
  compare_elements <- function(v1, v2, current_path) {
    if (typeof(v1) == "bytecode" && typeof(v2) == "bytecode") {
      return(compare_bytecode_strict(v1, v2, current_path))
    } else if (is.list(v1) && is.list(v2)) {
      if (length(v1) != length(v2)) {
        message(sprintf("[%s] List length mismatch: %d vs %d", current_path, length(v1), length(v2)))
        return(FALSE)
      }
      for (j in seq_along(v1)) {
        nested_path <- sprintf("%s -> List[%d]", current_path, j)
        if (!compare_elements(v1[[j]], v2[[j]], nested_path)) return(FALSE)
      }
      return(TRUE)
    } else {
      if (!identical(v1, v2)) {
        message(sprintf("[%s] Mismatch at constant value", current_path))
        if (inherits(v1, "srcrefsIndex") || inherits(v1, "expressionsIndex")) {
          message("     -> The mismatch is in the source/expression tracking vectors!")
        }
        return(FALSE)
      }
      return(TRUE)
    }
  }
  
  # 3. Recursively compare the constant pool
  for (i in seq_along(cp1)) {
    if (!compare_elements(cp1[[i]], cp2[[i]], sprintf("%s -> CP[%d]", path, i))) {
      return(FALSE)
    }
  }
  
  return(TRUE)
}

dump_all_bytecode <- function(bc, filename) {
  # Open a file connection for writing
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
        
        # Case A: Direct nested bytecode (e.g., Promises)
        if (typeof(item) == "bytecode") {
          dump_recursive(item, sprintf("%s -> CP[%d]", path, i))
        } 
        # Case B: Bytecode inside a list (e.g., MAKECLOSURE arguments)
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
  
  # Start the recursive dump
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
    
    message("Recursive bytecode successfully dumped to 'bytecode_reference.txt' and 'bytecode_crbcc.txt'")
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
      
      print(uncompiled_funs[[i]])
      
      readLines(con = "stdin", n = 1)
      
      stop("Test suite halted due to fatal crash.")
    })
    
  }

  avg_speedup <- if (length(speedups) > 0) mean(speedups) else NA
  
  cat(sprintf("\n[OK] %d functions compiled identically\n", compiled_ok))

  if (BENCHMARK)
    cat(sprintf("[PERF] Average compiler speedup: %.2fx\n", avg_speedup))

}

for (i in PACKAGES) {
  cat(sprintf("\n[START] Compiling package: %s ", i))
  test_package(i)
}

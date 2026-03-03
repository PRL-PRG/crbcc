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

benchmark_compilers <- function(prog, dump_bytecode = TRUE) {

  if (!requireNamespace("compiler", quietly = TRUE)) stop("Package 'compiler' is required.")
  if (!requireNamespace("crbcc", quietly = TRUE)) stop("Package 'crbcc' is required.")

  times_compiler <- numeric(10)
  times_crbcc <- numeric(10)
  
  # 1. Benchmark compiler::cmpfun 
  for (i in 1:10) {
    start_time <- Sys.time()
    res_compiler <- compiler::cmpfun(prog, options=list(optimize=3)) 
    end_time <- Sys.time()
    times_compiler[i] <- as.numeric(difftime(end_time, start_time, units = "secs"))
  }

  # 2. Benchmark crbcc::cmpfun
  for (i in 1:10) {
    start_time <- Sys.time()
    res_crbcc <- crbcc::cmpfun(prog)
    end_time <- Sys.time()
    times_crbcc[i] <- as.numeric(difftime(end_time, start_time, units = "secs"))
  }

  # 3. Dump recursively to files if requested
  if (dump_bytecode) {
    dump_all_bytecode(res_compiler, "bytecode_reference.txt")
    dump_all_bytecode(res_crbcc, "bytecode_crbcc.txt")
    
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

benchmarked_fn <- function() { 

  test_arithmetic <- function(x, y, z) {
    # Grouping and precedence
    t1 <- (x + y) * z
    t2 <- x + (y * z)
    
    # Powers and roots
    val <- t1 ^ 2
    root <- sqrt(val)
    
    # Exponentials and Logs
    e_val <- exp(root)
    l_val <- log(e_val)
    
    # Complex nesting
    res <- (l_val - t2) / ((x * y) + 1)
    
    return(res)
  }

  test_logic <- function(a, b) {
    
    is_eq <- (a == b)
    is_ne <- (a != b)
    is_gt <- (a > b)
    is_lt <- (a < b)
    
    logic_res <- (is_gt & is_lt) | is_eq
    
    short_res <- (a > 0) && (b > 0)
    
    if (short_res || (a < 0)) {
      return(!logic_res) # Test negation
    }
    
    return(logic_res)
  }

  test_flow <- function(limit) {
    res <- 0
    i <- 0
    
    while (i < limit) {
      i <- i + 1
      
      if (i == 2) {
        next
      }

      if (i > 20) {
        break
      }
      
      res <- res + i
    }
    
    j <- 0
    repeat {
      j <- j + 1
      res <- res + 1
      if (j >= 5) {
        break
      }
    }
    
    return(res)
  }

  test_trig <- function(x) {

    # Standard Trig
    s <- sin(x)
    c <- cos(x)
    t <- tan(x)
    
    # Arc Trig
    as <- asin(s)
    ac <- acos(c)
    at <- atan(t)
    
    # Hyperbolic
    sh <- sinh(x)
    ch <- cosh(x)
    th <- tanh(x)
    
    # Arc Hyperbolic
    ash <- asinh(sh)
    ach <- acosh(ch)
    ath <- atanh(th)
    
    # Pi variants
    sp <- sinpi(x)
    cp <- cospi(x)
    tp <- tanpi(x)
    
    return(as + ac + at)
  }

  test_special_math <- function(x) {
    
    # Rounding and Sign
    f <- floor(x)
    c <- ceiling(x)
    s <- sign(x)
    
    # Precision Log/Exp
    e1 <- expm1(x)
    l1 <- log1p(x)
    
    # Gamma functions
    g <- gamma(x)
    lg <- lgamma(x)
    dg <- digamma(x)
    tg <- trigamma(x)
    
    return(g + lg)
  }

  # This test is currently limited
  # because crbcc does not yet support
  # inlining this thing -> ':'
  test_for_loop <- function(vec) {
    total <- 0
    
    for (item in vec) {
      total <- total + item
    }
    
    return(total)
  }


  test_dollar_setters <- function(x) {

    x$a <- 100
    x$"spaced name" <- 200
    x$config$threshold <- 0.05
    x$global_counter <<- 1
    
    return(x)

  }

  test_simple_internals <- function(x) {

    res1 <- dnorm(x, 0, 1, FALSE)
    
    res2 <- dnorm(x)
    
    res3 <- dnorm(sd = 2, mean = 1, x = x)
    
  }

  test_is_handlers <- function(x) {

      is_char = is.character(x)
      is_cplx = is.complex(x)
      is_dbl  = is.double(x)
      is_int  = is.integer(x)
      is_lgl  = is.logical(x)
      is_name = is.name(x)
      is_null = is.null(x)
      is_obj  = is.object(x)
      is_sym  = is.symbol(x)

  }

  test_builtin <- function() {
    
    combined <- list(x, y)
    length(combined)

  }

  test_generic_special <- function(x) {

    res <- substitute(x)
    quote(res)

  }  

  test_builtin <- function() {
    
    combined <- list(x, y)
    length(combined)

  }


  test_generic_special <- function(x) {

    res <- substitute(x)
    quote(res)

  }

  #TODO this compiler reallllyy slow even slower than Rs compiler
  f <- function(x, y) { y; x }
  `f<-` <- function(x, y, value) { y; x }
  g <- function(x, y) { y; x }
  `g<-` <- function(x, y, value) { y; x }

  hard_to_compile <- function(x, y, ...) {
    # 1. Nested complex assignment with side-effects
    f(g(x, print(y)), y) <- 3
    
    # 2. Non-local loop control inside a promise/closure
    repeat {
      lapply(1:5, function(i) {
        if (i == 3) break  
      })
    }
    
    # 3. Dots (...) passed to a math builtin
    return(sum(x, y, ...))
  }

  hard_to_compile_2 <- function(env, my_list) {
    
    while (TRUE) {
      cmd <- receive_command()
      eval(cmd, envir = env)
    }
    
    ast_tree <- bquote( my_list[[.(1 + 2)]] <- "Done" )
    
    total <- sum(1, , 3)
    
    names(my_list$metadata)[2] <<- "processed"
    
    return(ast_tree)
  }

  hard_to_compile_3 <- function(mat, threshold) {
    
    # TODO this
    #`*` <- function(a, b) base::`*`(a, b) + 1
    
    # 2. Named arguments in subsetting and sub-assignment
    mat[row = 1, col = 2] <- mat[row = 2, col = 1] * 10
    
    # 3. Non-local return hidden inside a promise
    lapply(1:5, function(i) {
      if (mat[1, 2] > threshold) {
        return(TRUE)
      }
    })
    
    return(FALSE)
  }

  hard_to_compile_4 <- function(data_vec) {
    
    # 1. The 'browser()' bailout (even if it's dead code)
    if (FALSE) {
      browser()
    }
    
    # 2. Assignment inside a function argument
    res <- median( clean_data <- na.omit(data_vec) )
    
    # 3. Computed namespace resolution
    target_func <- paste0("me", "an")
    final_val <- `::`("base", target_func)(res)
    
    return(final_val)
  }

}

test_package <- function(package) {

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
    cat(sprintf("Compiling %s...\n", func_id))

    # Skip the three ones which contain switches
    if ( func_id %in% c(".onLoad", "findLocals1", "setCompilerOptions") ) {
      next
    }
    
    tryCatch({
      res <- benchmark_compilers(uncompiled_funs[[i]])
      

      if (!isTRUE(res$bytecode_identical)) {
        cat(sprintf("\n========================================\n"))
        cat(sprintf("[MISMATCH] Compilers are NOT identical!\n"))
        cat(sprintf("Stopped at: %s\n", func_id))
        cat(sprintf("========================================\n\n"))
        
        # Print the function that broke your compiler
        print(uncompiled_funs[[i]])
        
        # Wait for you to press Enter
        readLines(con = "stdin", n = 1)
        
        # Clear the console (sends CTRL+L)
        system("clear")
        
      } else {
        cat("[OK]\n")
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
      
      # Print the function that crashed your compiler
      print(uncompiled_funs[[i]])
      
      # Wait for you to press Enter
      readline(prompt = "\nPress [Enter] to clear the console and exit...")
      
      # Clear the console
      system("clear")
      
      # Halt the script
      stop("Test suite halted due to fatal crash.")
    })
    
  }

  avg_speedup <- if (length(speedups) > 0) mean(speedups) else NA
  
  cat(sprintf("\n[SUCCESS] All %d functions compiled perfectly!\n", compiled_ok))
  cat(sprintf("[PERFORMANCE] Average compiler speedup: %.2fx\n", avg_speedup))

}

fails <- function (elist, shadowed, cntxt) 
{
    return (T)
}

#x <- benchmark_compilers(fails)
#print(x)

test_package("compiler")
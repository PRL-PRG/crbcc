compare_bytecode_recursive <- function(bc1, bc2) {

  if (typeof(bc1) != typeof(bc2)) return(FALSE)
  
  if (typeof(bc1) != "bytecode") return(identical(bc1, bc2))
  
  d1 <- compiler::disassemble(bc1)
  d2 <- compiler::disassemble(bc2)
  
  if (!identical(d1[[1]], d2[[1]])) return(FALSE) # Version mismatch
  if (!identical(d1[[2]], d2[[2]])) return(FALSE) # Opcode/Operand mismatch
  
  cp1 <- d1[[3]]
  cp2 <- d2[[3]]
  
  if (length(cp1) != length(cp2)) return(FALSE)
  
  # Recursively compare the constant pool,
  # which may include nested bytecode
  for (i in seq_along(cp1)) {
    val1 <- cp1[[i]]
    val2 <- cp2[[i]]
    
    if (typeof(val1) == "bytecode" && typeof(val2) == "bytecode") {
      if (!compare_bytecode_recursive(val1, val2)) return(FALSE)
    } else {
      if (!identical(val1, val2)) return(FALSE)
    }
  }
  
  return(TRUE)
}

benchmark_compilers <- function(prog) {

  # Ensure the compiler package is loaded
  if (!requireNamespace("compiler", quietly = TRUE)) {
    stop("Package 'compiler' is required.")
  }
  
  # Check if crbcc is available
  if (!requireNamespace("crbcc", quietly = TRUE)) {
    stop("Package 'crbcc' is required.")
  }

  # Initialize vectors to store timings
  times_compiler <- numeric(10)
  times_crbcc <- numeric(10)
  
  # Placeholder for compiled results to compare later
  res_compiler <- NULL
  res_crbcc <- NULL

  is_identical <- TRUE

  # 1. Benchmark compiler::cmpfun
  for (i in 1:10) {
    start_time <- Sys.time()
    res_compiler <- compiler::cmpfun(prog, options=list(optimize=3)) 
                                                    # NOTE currently comparing against level 3 because 
                                                    # crbcc agressively inlines without checking origin packages
                                                    # and does not emit any baseguards
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

  # 3. Compare bytecode
  if (!compare_bytecode_recursive(res_compiler, res_crbcc)) {
    is_identical <- FALSE  
  }

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

}

results_x <- benchmark_compilers(benchmarked_fn);
print(results_x);
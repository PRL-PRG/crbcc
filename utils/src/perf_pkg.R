test_package <- function(package) {

  if (!requireNamespace("compiler", quietly = TRUE)) stop("Package 'compiler' is required.")
  if (!requireNamespace("crbcc", quietly = TRUE)) stop("Package 'crbcc' is required.")

  ns <- getNamespace(package)
  symbols <- ls(ns, all.names = TRUE)
  objects <- lapply(symbols, get, envir = ns)

  is_closure <- sapply(objects, \(x) typeof(x) == "closure")
  funs <- objects[is_closure]
  func_names <- symbols[is_closure]

  uncompiled_funs <- lapply(funs, function(f) {
    res <- f
    body(res) <- body(f)
    return(res)
  })

  names(uncompiled_funs) <- func_names
 
  if (!dir.exists(OUTPUT_DIR)) {
    dir.create(OUTPUT_DIR, recursive = TRUE)
  }
  
  csv_file <- file.path(OUTPUT_DIR, paste0(package, "_benchmark_results.csv"))
  
  headers <- data.frame(
    function_name = character(),
    lines_of_code = integer(),
    cyclomatic_complexity = integer(),
    median_gnu_r = numeric(),
    median_crbcc = numeric(),
    mem_gnu_r = numeric(), 
    mem_crbcc = numeric()
  )

  write.table(headers, file = csv_file, sep = ",", row.names = FALSE, col.names = TRUE)
  
  for (i in seq_along(uncompiled_funs)) {
    f_name <- func_names[i]
    f_uncomp <- uncompiled_funs[[i]]
    
    loc <- length(deparse(body(f_uncomp)))
    complexity <- tryCatch(cyclocomp::cyclocomp(f_uncomp), error = function(e) { NA })
    
    # 1. Timing Benchmark (Memory set to FALSE to prevent error)
    bm <- tryCatch({
      bench::mark(
        gnur = compiler::cmpfun(f_uncomp, options = list(optimize = OPTIMIZE)), 
        crbcc = crbcc::cmpfun(f_uncomp, options = list(optimize = OPTIMIZE)),
        iterations = ITERS_PER_FN,
        check = FALSE,
        memory = FALSE 
      )
    }, error = function(e) NULL)

    if (!is.null(bm)) {
      
      # --- GNU R Memory Delta ---
      gc(reset = TRUE)
      invisible(compiler::cmpfun(f_uncomp, options = list(optimize = OPTIMIZE)))
      m_gnu <- gc()
      # (Ncells max * 28 bytes) + (Vcells max * 8 bytes)
      mem_gnu_r <- (m_gnu[1, 5] * 28) + (m_gnu[2, 5] * 8) 

      # --- crbcc Memory Delta ---
      gc(reset = TRUE)
      invisible(crbcc::cmpfun(f_uncomp, options = list(optimize = OPTIMIZE)))
      m_crb <- gc()
      mem_crbcc <- (m_crb[1, 5] * 28) + (m_crb[2, 5] * 8)

      median_gnu_r <- as.numeric(bm$median[1])
      median_crbcc <- as.numeric(bm$median[2])
      
      res_row <- data.frame(
        function_name = f_name,
        lines_of_code = loc,
        cyclomatic_complexity = complexity,
        median_gnu_r = median_gnu_r,
        median_crbcc = median_crbcc,
        mem_gnu_r = mem_gnu_r,
        mem_crbcc = mem_crbcc
      )
      
      write.table(res_row, file = csv_file, sep = ",", row.names = FALSE, col.names = FALSE, append = TRUE)
    }
  }
  
  cat("[OK] Benchmarking complete. Results saved to ", csv_file, "\n")
}
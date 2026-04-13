test_package <- function(package) {

  if (!requireNamespace("compiler", quietly = TRUE)) stop("Package 'compiler' is required.")
  if (!requireNamespace("crbcc", quietly = TRUE)) stop("Package 'crbcc' is required.")

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
 
  if (!dir.exists(OUTPUT_DIR)) {
    dir.create(OUTPUT_DIR, recursive = TRUE)
  }
  
  csv_file <- file.path(OUTPUT_DIR, paste0(package, "_benchmark_results.csv"))
  
  headers <- data.frame(
    function_name = character(),
    lines_of_code = integer(),
    cyclomatic_complexity = integer(),
    median_gnu_r = numeric(),
    median_crbcc = numeric()
  )

  write.table(headers, file = csv_file, sep = ",", row.names = FALSE, col.names = TRUE)
  
  for (i in seq_along(uncompiled_funs)) {
    f_name <- func_names[i]
    f_uncomp <- uncompiled_funs[[i]]
    
    # Code Metrics
    loc <- length(deparse(body(f_uncomp)))
    complexity <- tryCatch(cyclocomp::cyclocomp(f_uncomp), error = function(e) {print(e); NA})
    
    # Benchmark
    bm <- tryCatch({
      bench::mark(
        gnur = compiler::cmpfun(f_uncomp, options=list(optimize=OPTIMIZE)), 
        crbcc = crbcc::cmpfun(f_uncomp, options=list(optimize=OPTIMIZE)),
        iterations = ITERS_PER_FN,
        check = FALSE,
        memory = FALSE
      )
    }, error = function(e) NULL)
    
    # Process and Save Results
    if (!is.null(bm)) {

      median_gnu_r <- as.numeric(bm$median[1])
      median_crbcc <- as.numeric(bm$median[2])
      
      res_row <- data.frame(
        function_name = f_name,
        lines_of_code = loc,
        cyclomatic_complexity = complexity,
        median_gnu_r = median_gnu_r,
        median_crbcc = median_crbcc
      )
      
      write.table(res_row, file = csv_file, sep = ",", row.names = FALSE, col.names = FALSE, append = TRUE)
    }
  }
  
  cat("[OK] Benchmarking complete. Results saved to ", csv_file)
}
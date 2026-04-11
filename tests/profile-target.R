if (!requireNamespace("crbcc", quietly = TRUE)) stop("Package 'crbcc' is required.")

# The Top 20 Global Worst Performers
targets <- list(
  list(package = "tools", name = ".check_package_CRAN_incoming"),
  list(package = "utils", name = "install.packages"),
  list(package = "tools", name = ".install_packages"),
  list(package = "utils", name = "str.default"),
  list(package = "stats", name = "plot.lm"),
  list(package = "tools", name = ".check_packages"),
  list(package = "base",  name = "[<-.data.frame"),
  list(package = "tools", name = ".check_packages_used"),
  list(package = "stats", name = "predict.lm"),
  list(package = "tools", name = "httpd"),
  list(package = "tools", name = ".shlib_internal"),
  list(package = "stats", name = "arima"),
  list(package = "utils", name = "hsearch_db"),
  list(package = "utils", name = "read.DIF"),
  list(package = "stats", name = "arima0"),
  list(package = "tools", name = ".check_package_depends"),
  list(package = "base",  name = "loadNamespace"),
  list(package = "utils", name = "read.table"),
  list(package = "tools", name = "codoc"),
  list(package = "stats", name = "nls")
)

ITERATIONS <- 50
OPTIMIZE_LEVEL <- 2

cat("Starting C-level profiling run...\n")

for (t in targets) {
  pkg <- t$package
  fn_name <- t$name
  
  ns <- getNamespace(pkg)
  if (!exists(fn_name, envir = ns, inherits = FALSE)) {
    cat(sprintf("Skipping %s::%s (not found)\n", pkg, fn_name))
    next
  }
  
  f <- get(fn_name, envir = ns)
  
  if (typeof(f) == "closure") {
    f_uncomp <- f
    body(f_uncomp) <- body(f)
    
    cat(sprintf("Compiling %s::%s (%d iterations)...\n", pkg, fn_name, ITERATIONS))
    
    for (i in 1:ITERATIONS) {
      invisible(crbcc::cmpfun(f_uncomp, options=list(optimize=OPTIMIZE_LEVEL)))
    }
  }
}

cat("Profiling complete.\n")
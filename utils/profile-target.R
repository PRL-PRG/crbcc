######################
# PROFILING SCRIPT
# (No comparing or measuring, run with external profiler)
######################
ITERATIONS <- 500
OPTIMIZE_LEVEL <- 2

# Currently The Top 20 Global Worst Performers
TARGETS <- list(
  list(package = "compiler", name = ".onLoad")
)
######################

args <- commandArgs(trailingOnly = TRUE)
if (length(args) > 0) {
  TARGETS <- lapply(args, function(arg) {
    parts <- strsplit(arg, "::", fixed = TRUE)[[1]]
    if (length(parts) != 2) stop("Expected target as package::name")
    list(package = parts[1], name = parts[2])
  })
}

if (!requireNamespace("crbcc", quietly = TRUE)) stop("Package 'crbcc' is required.")

cat("Starting C-level profiling run...\n")

for (t in TARGETS) {
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

##########################################
# PERFORMANCE BENCHMARKING
# ALL PATHS RELATIVE TO GIT ROOT
##########################################
# FOR GENERATING RAW PROFILING DATA (CSV FILES)
PROFILE <- TRUE
OPTIMIZE <- 2
ITERS_PER_FN <- 10
PACKAGES_TO_TEST <- c("base", "compiler", "tools", "stats", "utils")
OUTPUT_DIR <- "test_results/benchmark_data"

# FOR INTERPRETING DATA
INTERPRET <- TRUE
INTERPRET_SOURCE <- OUTPUT_DIR
INTERPRET_OUTPUT <- "test_results/results"
##########################################

INTERPRET_SCRIPT <- "tests/src/interpret.R" 
PROFILER_SCRIPT <- "tests/src/perf_pkg.R"

packages <- c()

if (PROFILE)
  packages <- c(packages, "bench", "cyclocomp")

if (INTERPRET)
  packages <- c(packages, "ggplot2")

missing_packages <- packages[!(packages %in% rownames(installed.packages()))]

if (length(missing_packages) > 0) {
  install.packages(missing_packages)
}

for(i in packages)
  library(i, character.only = TRUE)

if (PROFILE) {
  source(PROFILER_SCRIPT)
  for (i in PACKAGES_TO_TEST) {
    cat(sprintf("\n[BENCH] Profiling package: %s\n", i))
    test_package(i)
  } 
}

if (INTERPRET) {
  source(INTERPRET_SCRIPT)
  interpret(INTERPRET_SOURCE, INTERPRET_OUTPUT)
}
## Installs the benchmark packages with GNU R from the CRAN snapshot: the
## packages in cran50.txt, the recommended packages and their dependencies.
## It keeps the source tarballs for the suite install of crbcc_bench.py.
##
## Usage: Rscript build-packages.R <cran50.txt>
##
## Output in $CRBCC_BENCHMARKS:
##   lib/  the installed packages
##   src/  the CRAN source tarballs

root     <- Sys.getenv("CRBCC_BENCHMARKS")
snapshot <- Sys.getenv("CRAN_SNAPSHOT")
stopifnot(nzchar(root), nzchar(snapshot))
lib <- file.path(root, "lib")
src <- file.path(root, "src")
dir.create(lib, recursive = TRUE)
dir.create(src)
.libPaths(lib)

cran <- trimws(grep("^#", readLines(commandArgs(trailingOnly = TRUE)[1]), invert = TRUE, value = TRUE))
cran <- cran[nzchar(cran)]
## Also the recommended packages, because the suite install uses their CRAN
## sources.
pkgs <- c(cran, rownames(installed.packages(priority = "recommended")))
db   <- available.packages(repos = snapshot, type = "source")
deps <- unlist(tools::package_dependencies(pkgs, db, recursive = TRUE))

## quiet = TRUE keeps the log short. BuildKit clips a step log at 2 MiB, and
## then the "not installed" error below does not show.
install.packages(pkgs, lib, repos = snapshot, type = "source", destdir = src,
                 Ncpus = parallel::detectCores(), quiet = TRUE)
## install.packages() only gives a warning if it cannot install a package.
if (length(miss <- setdiff(c(pkgs, deps), rownames(installed.packages()))))
  stop("not installed: ", paste(miss, collapse = ", "))

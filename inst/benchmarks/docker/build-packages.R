## Installs the benchmark packages one time with GNU R, and saves the GNU R
## byte code for the suite check. The installations are parallel, with one
## R CMD INSTALL for each CPU.
##
## The packages are the CRAN packages in cran100.txt, the recommended
## packages and all their dependencies. The script keeps the source tree of
## each package after the installation as a tarball, with the compiled code
## and the output of configure. R CMD INSTALL --no-configure of this tarball
## does not compile the native code again.
##
## Byte code: the script installs each package one more time from its
## tarball, as crbcc_bench.py does. The hook writes the GNU R byte code of
## each closure for the crbcc check. The first installation cannot write the
## byte code, because the R JIT compiles different closures when R installs
## from the source (for example 3 closures of withr in ggplot2).
##
## Usage: Rscript build-packages.R <directory with cran100.txt and the hook>
##
## Output in $CRBCC_BENCHMARKS:
##   lib/          the installed packages
##   prebuilt/     <package>.tar.gz, the source trees with the compiled code
##   bytecode/     <package>.rds, the GNU R byte code of each closure

here     <- commandArgs(trailingOnly = TRUE)[1]
root     <- Sys.getenv("CRBCC_BENCHMARKS")
snapshot <- Sys.getenv("CRAN_SNAPSHOT")
stopifnot(dir.exists(here), nzchar(root), nzchar(snapshot))

lib      <- file.path(root, "lib")
prebuilt <- file.path(root, "prebuilt")
src      <- file.path(tempdir(), "src")
build    <- file.path(tempdir(), "build")
for (d in c(lib, prebuilt, src, build)) dir.create(d, recursive = TRUE, showWarnings = FALSE)

db   <- available.packages(repos = snapshot, type = "source")
top  <- trimws(grep("^#", readLines(file.path(here, "cran100.txt")), invert = TRUE, value = TRUE))
top  <- top[nzchar(top)]
rec  <- rownames(installed.packages(priority = "recommended"))
base <- rownames(installed.packages(priority = "base"))
if (length(miss <- setdiff(c(top, rec), rownames(db))) > 0)
  stop("not in the CRAN snapshot: ", paste(miss, collapse = ", "))

## All packages. Each package comes after its dependencies. On CRAN, these
## dependencies have no cycles.
which <- c("Depends", "Imports", "LinkingTo")
all <- setdiff(unique(c(top, rec, unlist(tools::package_dependencies(
  c(top, rec), db, which = which, recursive = TRUE)))), base)
direct <- tools::package_dependencies(all, db, which = which)
order <- character()
visit <- function(p) {
  if (p %in% order) return()
  for (d in intersect(direct[[p]], all)) visit(d)
  order <<- c(order, p)
}
for (p in all) visit(p)
message(length(order), " packages: ", length(top), " CRAN, ", length(rec),
        " recommended, ", length(order) - length(union(top, rec)), " dependencies")

files <- download.packages(order, src, repos = snapshot, type = "source", quiet = TRUE)
tarball <- setNames(files[, 2], files[, 1])

hook <- file.path(here, "r-cmd-install-hook.R")
env  <- c(paste0("R_LIBS=", lib), "MAKEFLAGS=-j8",
          paste0("R_PROFILE_USER=", hook), "CRBCC_MODE=bench_gnur")

## Calls f(p) for each package p in parallel. Stops at the first error.
each_package <- function(pkgs, f) {
  res <- parallel::mclapply(pkgs, f, mc.cores = parallel::detectCores(),
                            mc.preschedule = FALSE)
  if (length(err <- Filter(function(r) inherits(r, "try-error"), res)) > 0)
    stop(err[[1]], call. = FALSE)
}

## The level of a package is 1 + the maximum level of its dependencies.
## The packages of one level do not depend on each other, thus they can
## install in parallel.
level <- setNames(integer(length(order)), order)
for (p in order) level[[p]] <- max(0L, level[intersect(direct[[p]], all)] + 1L)

install_source <- function(p) {
  untar(tarball[[p]], exdir = build)
  t0  <- Sys.time()
  out <- suppressWarnings(system2(file.path(R.home("bin"), "R"),
    c("CMD", "INSTALL", paste0("--library=", lib), file.path(build, p)),
    stdout = TRUE, stderr = TRUE, env = env))
  elapsed <- as.numeric(Sys.time() - t0, units = "secs")
  if (!is.null(status <- attr(out, "status")) && status != 0) {
    writeLines(tail(out, 40))
    stop("R CMD INSTALL cannot install ", p)
  }
  ## tar keeps the times of the files. Thus make finds the objects up to date.
  if (system2("tar", c("czf", file.path(prebuilt, paste0(p, ".tar.gz")), "-C", build, p)) != 0)
    stop("tar cannot pack ", p)
  unlink(file.path(build, p), recursive = TRUE)
  message(sprintf("[level %d] %s %.1f s", level[[p]], p, elapsed))
}
for (l in sort(unique(level))) each_package(order[level == l], install_source)

## Byte code ----------------------------------------------------------------

target   <- file.path(tempdir(), "install")
bytecode <- file.path(root, "bytecode")
dir.create(target)
dir.create(bytecode)

## The hook copies the shared objects from lib, thus make does not run. lib
## has all packages, thus all installations are parallel. Each package has
## its own target library, as in the suite check of crbcc_bench.py.
env_bytecode <- c(env, paste0("CRBCC_PREBUILT_LIB=", lib),
                  paste0("CRBCC_SAVE_BYTECODE=", bytecode))
save_bytecode <- function(p) {
  dir.create(file.path(target, p))
  out <- suppressWarnings(system2(file.path(R.home("bin"), "R"),
    c("CMD", "INSTALL", "--no-configure", paste0("--library=", file.path(target, p)),
      file.path(prebuilt, paste0(p, ".tar.gz"))),
    stdout = TRUE, stderr = TRUE, env = env_bytecode))
  if (!is.null(status <- attr(out, "status")) && status != 0) {
    writeLines(tail(out, 40))
    stop("R CMD INSTALL cannot install the prebuilt ", p)
  }
  message(sprintf("bytecode %s: %d closures", p, length(grep("^RBC fn\t", out))))
}
each_package(order, save_bytecode)

# crbcc

[![CRAN status](https://www.r-pkg.org/badges/version/crbcc)](https://CRAN.R-project.org/package=crbcc)
[![CRAN downloads](https://cranlogs.r-pkg.org/badges/grand-total/crbcc)](https://CRAN.R-project.org/package=crbcc)
[![CI](https://github.com/PRL-PRG/crbcc/actions/workflows/ci.yml/badge.svg)](https://github.com/PRL-PRG/crbcc/actions/workflows/ci.yml)
[![Compile speedup](https://img.shields.io/endpoint?url=https%3A%2F%2Fprl-prg.github.io%2Fcrbcc%2Fspeedup.json)](https://prl-prg.github.io/crbcc/)
[![License: GPL (>= 2)](https://img.shields.io/badge/license-GPL%20(%3E%3D%202)-blue)](LICENSE)

`crbcc` is an R package that compiles R code to bytecode using a C implementation.
It is intended as a fast, drop-in alternative to the base `compiler` package, for cases in which
a reduced compilation time could be beneficial, eg. in research.
However using `crbcc` for minor performance improvements inside R's JIT pipeline is also possible.

This project was developed as part of a bachelor's thesis at FIT CTU.

## Acknowledgement

When possible, `crbcc` mirrors the architecture and logic used in the original GNU-R compiler package by Luke Tierney. The original compiler Noweb document is available at https://homepage.cs.uiowa.edu/~luke/R/compiler/compiler.pdf, or distributed as an R base package, available under the GPL-2 license. Most documentation provided in the document is relevant to `crbcc` as well.

## Features

- The output is the same as the output of the GNU-R compiler. `tests/compare.R` compares the results of `cmpfun` for all 8,325 closures (approximately 231,000 lines of code) of the base and recommended R packages.
- During `R CMD INSTALL` of 65 R packages, crbcc compiles the closures approximately 49x faster than the GNU-R compiler (3.4 s, not 166.8 s). See [Performance](#performance).
- If the native code is prebuilt, `R CMD INSTALL` of the 15 recommended R packages takes 54% less time with crbcc.

## Performance

These results are for 65 R packages: the 15 recommended packages and the
50 CRAN packages with the most downloads (the first 50 packages of
`inst/benchmarks/docker/cran100.txt`). The two compilers ran on the same
machine, an Intel Xeon Gold 6140 with Linux and R 4.6.2. The results are
the median of 3 runs. The benchmark Docker image has the compiled native
code of the packages, thus the times do not include the compilation of C,
C++ and Fortran code.

| | GNU R | crbcc | |
|---|---:|---:|---|
| `R CMD INSTALL` of the 65 packages | 394.3 s | 230.5 s | 164 s (42%) less |
| Byte-code compilation in this `R CMD INSTALL` | 166.8 s | 3.4 s | 49x faster |
| `R CMD INSTALL` of the 15 recommended packages | 171.3 s | 78.0 s | 93 s (54%) less |
| `R CMD INSTALL` of Matrix | 50.7 s | 21.5 s | 58% less |
| `R CMD INSTALL` of mgcv | 41.6 s | 12.2 s | 71% less |

- With GNU R, the byte-code compilation is 42% of the time of
  `R CMD INSTALL` (55% for the recommended packages). With crbcc, it is
  1.5%. The other time, for example to make the lazy-load database and the
  help pages, is the same for the two compilers.
- The geometric mean of the speedups of the 23,918 closures is 41x.
- For all 46,129 closures of the 158 packages in the benchmark Docker
  image, the crbcc byte code is the same as the GNU R byte code. These are
  the closures that R compiles during `R CMD INSTALL`, also the S4 methods.

The benchmarks in `inst/benchmarks` made these results (see
[Tests and benchmarks](#tests-and-benchmarks)).

[![Compile time of each closure with GNU R and crbcc](https://prl-prg.github.io/crbcc/closures-time-by-size.png)](https://prl-prg.github.io/crbcc/)

The figure shows the time to compile each closure with GNU R and with
crbcc, by the size of the closure. The closures are from the 65 benchmark
packages. The CI makes the figure again for each push to `main`. The
[benchmark report](https://prl-prg.github.io/crbcc/) gives all the results
of the CI.

## Interface
- Compile closures with `crbcc::cmpfun()`
- Compile generic language objects with `crbcc::compile()`
- Compile full source files with `crbcc::cmpfile()`
- Load compiled files with `crbcc::loadcmp()`
- Install packages with crbcc with `crbcc::enable_r_cmd_install_hook()`
  
## Requirements

- R >= 4.5.0
- Build toolchain for compiling native C code

## Install

### From CRAN

```R
install.packages("crbcc")
```

### From source (local checkout)

```bash
R CMD INSTALL .
```

or with the provided Make target:

```bash
make install
```

## Quick Start

```r
library(crbcc)

f <- function(x) {
  y <- x + 1
  y * 2
}

cf <- cmpfun(f, options = list(optimize = 2))
cf(10)
```

## Compile an R file

```r
library(crbcc)

cmpfile("script.R", "script.Rc")
loadcmp("script.Rc", envir = .GlobalEnv)
```

## Install packages with crbcc

To make `install.packages()` compile the packages with crbcc:

```r
crbcc::enable_r_cmd_install_hook()
install.packages("MASS")
crbcc::enable_r_cmd_install_hook(FALSE)
```

To install a package from a shell:

```bash
R_PROFILE_USER=$(Rscript -e 'cat(crbcc::r_cmd_install_hook_path())') \
  R CMD INSTALL foo_1.0.tar.gz
```

For the details, refer to the vignette: `vignette("crbcc", package = "crbcc")`
or [online](https://prl-prg.github.io/crbcc/crbcc.html).

## Compiler options

The compiler accepts options as a named list:

- `optimize` (`integer`): optimization level (default `2`)
- `suppressAll` (`logical`): suppress warnings (default `TRUE`)
- `suppressNoSuperAssignVar` (`logical`): suppress missing super-assignment variable warnings
- `suppressUndefined` (`logical` or `character`):
  - `TRUE`: suppress all undefined variable/function warnings
  - `FALSE`: suppress none
  - `character()`: suppress specific names

Example:

```r
opts <- list(
  optimize = 2,
  suppressAll = FALSE,
  suppressUndefined = c("pi", "letters")
)

cf <- cmpfun(function(x) x + pi, options = opts)
```

## Contributing

Contributions are welcome.

- Open an issue first for bugs, regressions, or feature proposals, especially if the compiler crashed on some expression or returned different results from GNU-R
- Keep pull requests focused and small when possible.
- Do not modify tests. Compatibility with GNU R compiler behavior is expected to remain 1:1.
- The CI runs `R CMD check` and the benchmarks for each pull request.
- Run package checks before submitting (no new warnings or errors should rise):

```bash
R CMD check .
```

## Tests and benchmarks

`R CMD check .` runs `tests/compare.R`. The test compiles all closures of the
base and recommended packages with crbcc and with the GNU R compiler, and
compares the results.

`inst/benchmarks/crbcc_bench.py` compares crbcc with the GNU R compiler. It
installs each package with `R CMD INSTALL`, and the hook
`inst/r-cmd-install-hook.R` measures the compile time of each closure. In
the Docker image, the script also runs the suite `check`. This suite
installs each package one time with crbcc, and compares the byte code of
each closure with the GNU R byte code. The suite `install` measures the
time and does not compare, because the comparison adds time. The script
keeps the R libraries in the work directory, and uses the crbcc in
`pkg-lib/`. To run the script, install [uv](https://docs.astral.sh/uv/).
`inst/benchmarks/crbcc_bench.Rmd` makes the report from the results of the
suites `install` and `check`. The suite `check` runs only in the Docker
image, thus make the report in the image. The section "Reproduce" of the
report gives the commands. The docstring of `crbcc_bench.py` gives the
metrics of the report.

To run the benchmarks without the Docker image, run these commands in a work
directory:

```bash
mkdir -p pkg-lib
R-4.6.2/bin/R CMD INSTALL --library=pkg-lib crbcc
crbcc/inst/benchmarks/crbcc_bench.py --r R-4.6.2/bin/R --csv results.csv
```

By default, the benchmarks use the packages MASS, Matrix, lattice, mgcv and
survival (`--packages small`). For more packages, use
`--packages recommended`, `--packages cran100` (the packages in
`inst/benchmarks/docker/cran100.txt`) or `--packages top50` (the first 50
packages of cran100). If R does not have the recommended
packages, the script installs them from `src/library/Recommended`. Thus R
must then be a build in the R source tree. The script downloads the other
package sources from a CRAN snapshot. For the other options, use `--help`.

The Docker image `ghcr.io/prl-prg/crbcc-benchmarks` has R, the packages
cran100 and recommended, and their dependencies. The image build installs
each package one time with GNU R and keeps the compiled code. Then it
installs each package one more time with GNU R, and keeps the GNU R byte
code for the suite `check`. In the image, use
`--prebuilt /opt/crbcc-benchmarks`. Then `R CMD INSTALL` does not compile
the native code again. With `--prebuilt`, `--packages all` selects all
packages in the image. A run of the suite `check` is a failure if a closure
has a different byte code. `inst/benchmarks/docker/Dockerfile` tells how to
build the image. For each push and pull request, the CI workflow
`.github/workflows/ci.yml` runs the suite `check` with crbcc for all
packages in this image. It also runs the suite `install` with gnur and
crbcc on the same runner, for the packages top50 and recommended.

The comments in `inst/r-cmd-install-hook.R` give the hook modes
(`CRBCC_MODE`) that the benchmarks use.

To profile the compilation of one closure with an external profiler:

```bash
Rscript -e 'f <- crbcc:::package_closures("compiler")$.onLoad; for (i in 1:500) crbcc::cmpfun(f, options = list(optimize = 2))'
```

## Known limitations

- Warning output is not guaranteed to be 1:1 with GNU R's `compiler` package.
- `cmpfile()` currently does not support a functional `verbose` mode (the argument exists but is not implemented).
- Saving and loading functionality is not compatible with the GNU-R compiler, since it uses an .Internal call to manage the files.

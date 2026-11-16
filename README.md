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
- For the closures of the base and recommended R packages, crbcc is approximately 46x faster than the GNU-R compiler (0.83 s, not 38.3 s). See [Performance](#performance).
- With crbcc, `R CMD INSTALL` of the 15 recommended R packages takes 41% less time.

## Performance

These results are for the 12 base and the 15 recommended R packages, on an
Apple M4. They are the median of 5 runs.

| | GNU R | crbcc | |
|---|---:|---:|---|
| Compile all 8,323 closures of the 27 packages | 38.3 s | 0.83 s | 46x faster |
| `R CMD INSTALL` of the 15 recommended packages | 70.5 s | 41.6 s | 29 s (41%) less |
| Byte-code compilation in this `R CMD INSTALL` | 29.5 s | 0.61 s | 48x faster |
| `R CMD INSTALL` of mgcv | 14.6 s | 5.1 s | 65% less |

- With GNU R, the byte-code compilation is 42% of the time of
  `R CMD INSTALL`. With crbcc, it is 1.5%. The other time, for example to
  compile the C code (`make -j8`) and to make the help pages, is the same
  for the two compilers.
- The geometric mean of the speedups of the closures is 62x.
- For all 17,598 closures, the byte code of crbcc is the same as the byte
  code of GNU R. These are the closures of the package namespaces and the
  closures that R compiles during `R CMD INSTALL`, also the S4 methods.

To make these results, run
`crbcc_bench.py --packages base --packages recommended` (see
[Tests and benchmarks](#tests-and-benchmarks)).

[![Compile time of each closure with GNU R and crbcc](https://prl-prg.github.io/crbcc/closures-time-by-size.png)](https://prl-prg.github.io/crbcc/)

The figure shows the time to compile each closure with GNU R and with
crbcc, by the size of the closure. The closures are from the R base packages
and from MASS, Matrix, lattice, mgcv and survival. The CI makes the figure
again for each push to `main`. The
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
measures the compile time and the result of the compilation of package
closures, and the time of `R CMD INSTALL`. The script needs
[uv](https://docs.astral.sh/uv/). It keeps the R libraries in the work
directory, and uses the crbcc in `pkg-lib/`. The R code of the benchmarks is
in `R/dev.R` (`crbcc:::bench_package`, `crbcc:::bench_functions`,
`crbcc:::bench_closure`) and in `inst/r-cmd-install-hook.R`.
`inst/benchmarks/crbcc_bench.Rmd` makes the report. It needs the R packages
rmarkdown, dplyr, tidyr and ggplot2. The docstring of `crbcc_bench.py` gives
the suites and the metrics of the report.

To run the benchmarks and make the report, run these commands in a work
directory:

```bash
mkdir -p pkg-lib
R-4.6.2/bin/R CMD INSTALL --library=pkg-lib crbcc
crbcc/inst/benchmarks/crbcc_bench.py --r R-4.6.2/bin/R --csv results.csv
Rscript -e 'rmarkdown::render("crbcc/inst/benchmarks/crbcc_bench.Rmd", output_dir = ".", knit_root_dir = getwd())'
```

By default, the benchmarks use the R base packages. For more packages, use
`--packages small` (MASS, Matrix, lattice, mgcv and survival),
`--packages recommended` or `--packages cran50`. Only these packages have
the install suites. If R does not have the recommended packages, the script
installs them from `src/library/Recommended`. Thus R must then be a build in
the R source tree. The script downloads the other package sources from a
CRAN snapshot. For the other options, use `--help`. The CI workflow
`.github/workflows/ci.yml` runs the benchmarks with the packages base and
small, without the suite install-fn, for each push and pull request.

The install suites use the benchmark modes of the hook
`inst/r-cmd-install-hook.R` (`CRBCC_MODE`). The comments in the hook give
the modes.

To profile the compilation of one closure with an external profiler:

```bash
Rscript -e 'f <- crbcc:::package_closures("compiler")$.onLoad; for (i in 1:500) crbcc::cmpfun(f, options = list(optimize = 2))'
```

## Known limitations

- Warning output is not guaranteed to be 1:1 with GNU R's `compiler` package.
- `cmpfile()` currently does not support a functional `verbose` mode (the argument exists but is not implemented).
- Saving and loading functionality is not compatible with the GNU-R compiler, since it uses an .Internal call to manage the files.

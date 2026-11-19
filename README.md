# crbcc

[![CRAN status](https://www.r-pkg.org/badges/version/crbcc)](https://CRAN.R-project.org/package=crbcc)
[![CRAN downloads](https://cranlogs.r-pkg.org/badges/grand-total/crbcc)](https://CRAN.R-project.org/package=crbcc)
[![CI](https://github.com/PRL-PRG/crbcc/actions/workflows/ci.yml/badge.svg)](https://github.com/PRL-PRG/crbcc/actions/workflows/ci.yml)
[![Compile speedup](https://img.shields.io/endpoint?url=https%3A%2F%2Fprl-prg.github.io%2Fcrbcc%2Fspeedup.json)](https://prl-prg.github.io/crbcc/)
[![License: GPL (>= 2)](<https://img.shields.io/badge/license-GPL%20(%3E%3D%202)-blue>)](https://www.r-project.org/Licenses/GPL-2)

`crbcc` is an R package that compiles R code to bytecode using a C implementation.
It is intended as a fast, drop-in alternative to the base `compiler` package.

## Acknowledgement

When possible, `crbcc` mirrors the architecture and logic used in the original
GNU-R [compiler](https://homepage.cs.uiowa.edu/~luke/R/compiler/compiler.pdf)
package by Luke Tierney. We gain the performance by writing it in C instead of
R (plus a few tricks).

## Features

- The output is the same as the output of the GNU-R compiler.
- It compiles the closures faster than the GNU-R compiler - check the badge above!
- Package installation is often dominated by the native code compilation (C/C++/Fortran)
  but even with that crbcc can save approximately **25% of the time**.

[![Installation time of the 15 packages with the longest GNU R installation time](https://prl-prg.github.io/crbcc/install-time-top15.png)](https://prl-prg.github.io/crbcc/#install-results)
The figure shows the installation time of the 15 packages with the
longest GNU R installation time. `R CMD INSTALL` builds each package from the
CRAN source, also its native code. The left panel is the elapsed time of
`R CMD INSTALL`, and the middle panel is the time in the byte-code compiler.
The right panel is the part of the GNU R elapsed time that the GNU R
byte-code compiler uses.

For more details check the [benchmark report](https://prl-prg.github.io/crbcc/).

## Requirements

- R >= 4.5.0
- Build toolchain for compiling native C code

## Install

From CRAN:

```R
install.packages("crbcc")
```

or latest version from Github

```R
# install.packages("pak")
pak::pak("PRL-PRG/crbcc")
```

## Quick Start

For the details and more examples, refer to the vignette:
`vignette("crbcc", package = "crbcc")` or
[online](https://prl-prg.github.io/crbcc/crbcc.html).

### Compile a function

```r
library(crbcc)

f <- function(x) {
  y <- x + 1
  y * 2
}

cf <- cmpfun(f, options = list(optimize = 2))
cf(10)
```

### Compile an expression

```r
library(crbcc)

code <- compile(quote(x * 2 + 1))
x <- 20
eval(code)
```

### Compile an R file

```r
library(crbcc)

cmpfile("script.R", "script.Rc")
loadcmp("script.Rc", envir = .GlobalEnv)
```

### Install packages with crbcc

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

The [hook](https://github.com/PRL-PRG/crbcc/blob/main/inst/r-cmd-install-hook.R)
is an `R_PROFILE_USER` file. R calls `compiler:::tryCmpfun` to compile a
closure, during `R CMD INSTALL` and in the JIT. In each R process that reads
the hook, the hook replaces this function with crbcc. If crbcc cannot
compile a closure, the hook compiles it with GNU R.

## Compiler options

The compiler accepts options as a named list. The options and their
defaults are the same as in `compiler::cmpfun()`. crbcc does not use
`compiler::setCompilerOptions()`, thus an option that is not in the list
has its default value.

- `optimize` (`integer`): optimization level (default `2`)
- `suppressAll` (`logical`): suppress warnings (default `TRUE`)
- `suppressNoSuperAssignVar` (`logical`): suppress missing super-assignment variable warnings (default `FALSE`)
- `suppressUndefined` (`logical` or `character`), default
  `c(".Generic", ".Method", ".Random.seed", ".self")`:
  - `TRUE`: suppress all undefined variable/function warnings
  - `FALSE`: suppress none
  - `character()`: suppress specific names

## Tests

The tests in `tests/` compare crbcc with the GNU-R compiler:

- `tests/compare.R` compiles all closures of the base and recommended
  packages with the two compilers, and compares the byte code.
- `tests/regressions.R` has the differences that the benchmarks found.
- `tests/notes.R` compares the compiler notes, for example "no visible
  binding for global variable".

`R CMD check` and `make test` run these tests. The CI runs `R CMD check` on
Linux and macOS.

The integration test compiles each closure of each package in the benchmark
Docker image with the two compilers, and compares the byte code (see
[Benchmarks](#benchmarks)). These closures include the S4 methods and the R6
methods. The image build runs this test, and the CI job `compile` runs it
for the base packages and the 65 packages of the install benchmark. To run it
in the image:

```bash
docker run --rm -v "$PWD":/crbcc -w /crbcc \
  ghcr.io/prl-prg/crbcc-benchmarks:$(make -s image-tag) \
  make test-compile-all-packages
```

## Benchmarks

The benchmarks check the correctness of crbcc and compare its performance
with the GNU-R compiler. The install benchmark measures `R CMD INSTALL` of
the 50 CRAN packages with the most downloads and the 15 recommended packages,
from the source. It runs when the benchmark Docker image is built, one time
for each crbcc version. The compile benchmark measures the compile time of
each closure of the same 65 packages. The CI runs it for each pull request
and each push to `main`.

- [Benchmark report](https://prl-prg.github.io/crbcc/): the results of the
  last push to `main`. The source is `inst/benchmarks/crbcc_bench.Rmd`.
- [`inst/benchmarks/README.md`](https://github.com/PRL-PRG/crbcc/blob/main/inst/benchmarks/README.md): what the
  benchmarks measure, how the Docker image is built, and how to run the
  benchmarks and set their parameters.
- [bench](https://github.com/PRL-PRG/bench): the tool that runs the
  benchmarks.

## Known limitations

- The compiler notes are in English only. GNU R translates them to the
  language of the R session.
- Saving and loading functionality is not compatible with the GNU-R compiler, since it uses an .Internal call to manage the files.

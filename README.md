# crbcc

`crbcc` is an R package that compiles R code to bytecode using a C implementation.
It is intended as a fast, drop-in alternative to the base `compiler` package, for cases in which
a reduced compilation time could be beneficial, eg. in research.
However using `crbcc` for minor performance improvements inside R's JIT pipeline is also possible.

This project was developed as part of a bachelor's thesis at FIT CTU.

## Acknowledgement

When possible, `crbcc` mirrors the architecture and logic used in the original GNU-R compiler package by Luke Tierney. The original compiler Noweb document is available at https://homepage.cs.uiowa.edu/~luke/R/compiler/compiler.pdf, or distributed as an R base package, available under the GPL-2 license. Most documentation provided in the document is relevant to `crbcc` as well.

## Features

- 1:1 compatible output with current GNU-R compiler, `cmpfun` tested on a corpus consisting of around 500,000 lines of code
- 30x improvement on wall clock time when compiling base R packages, see Performance

## Interface
- Compile closures with `crbcc::cmpfun()`
- Compile generic language objects with `crbcc::compile()`
- Compile full source files with `crbcc::cmpfile()`
- Load compiled files with `crbcc::loadcmp()`
  
## Requirements

- R >= 4.5.0
- Build toolchain for compiling native C code

## Install

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

## JIT compilation

`crbcc` contains identical interface to the GNU-R compiler. Taking over JIT compilation is possible by updating the namespace of the compiler package. Keep in mind this changes the `compiler::cmpfun` definition in the entire R session. Intended for experimental use only.

```r
compiler_ns <- getNamespace("compiler")
unlockBinding("cmpfun", compiler_ns)
compiler_ns$cmpfun <- crbcc::cmpfun
lockBinding("cmpfun", compiler_ns)
```

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
- Run package checks before submitting (no new warnings or errors should rise):

```bash
R CMD check .
```

## Known limitations

- Warning output is not guaranteed to be 1:1 with GNU R's `compiler` package.
- `cmpfile()` currently does not support a functional `verbose` mode (the argument exists but is not implemented).
- Saving and loading functionality is not compatible with the GNU-R compiler, since it uses an .Internal call to manage the files.

## Citation

Citation details are pending and will be added once the associated thesis is publicly released.
If not available, reference the repository URL: <https://github.com/PRL-PRG/crbcc>.

## Performance

Benchmark results against GNU R's compiler are available at:

- `other/test_results/results/benchmark_report.md`

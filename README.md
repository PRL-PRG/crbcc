# crbcc

`crbcc` is an R package that compiles R code to bytecode using a C implementation.
It is intended as a fast, drop-in alternative to `compiler::cmpfun`.

This project was developed as part of a bachelor's thesis at FIT CTU.

## Features

- Compile closures with `crbcc::cmpfun()`
- Compile generic language objects with `crbcc::compile()`
- Compile full source files with `crbcc::cmpfile()`
- Load compiled files with `crbcc::loadcmp()`
- Configurable compiler options (`optimize`, warning suppression controls)

## Requirements

- R >= 4.5.2
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

- Open an issue first for bugs, regressions, or feature proposals.
- Keep pull requests focused and small when possible.
- Follow existing code style and project structure.
- Do not modify tests to make changes pass. Compatibility with GNU R compiler behavior is expected to remain 1:1.
- Run package checks before submitting:

```bash
R CMD check .
```

## Known limitations

- Warning output is not yet guaranteed to be 1:1 with GNU R's `compiler` package in all cases.
- `cmpfile()` currently does not support a functional `verbose` mode (the argument exists but is not implemented).

## Citation

Citation details are pending and will be added once the associated bachelor's thesis is publicly released.
For now, please reference the repository URL: <https://github.com/PRL-PRG/crbcc>.

## Performance

Benchmark results against GNU R's compiler are available at:

- `other/test_results/results/benchmark_report.md`

## License

GPL (>= 2)

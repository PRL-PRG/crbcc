# crbcc benchmarks

The benchmarks compare crbcc with the GNU R byte-code compiler:

- **Install benchmark**: the time of `R CMD INSTALL` with each compiler.
- **Closure check**: crbcc must give the same byte code as GNU R for each
  closure.
- **Compile benchmark**: the compile time of each closure with each
  compiler.

The [benchmark report](https://prl-prg.github.io/crbcc/) shows the results of
the last push to `main`.

## Files

| File                      | Contents                                                                                                                                                       |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `crbcc_bench.py`          | The benchmark tool, with the suites `install`, `check` and `compile`. It uses [bench](https://github.com/PRL-PRG/bench). The docstring gives the CSV columns.  |
| `closures.R`              | Compiles each closure of a package for the suites `check` and `compile`.                                                                                       |
| `timing-profile.R`        | Measures the native and bytecode times in the suite `install`.                                                                                                 |
| `crbcc_bench.Rmd`         | The benchmark report.                                                                                                                                          |
| `docker/`                 | The benchmark image. `cran50.txt` has the 50 CRAN packages with the most downloads.                                                                            |
| `../r-cmd-install-hook.R` | The hook that the crbcc runs of the suite `install` use.                                                                                                       |

## Install benchmark

The suite `install` installs the 50 CRAN packages of `docker/cran50.txt` and
the 15 recommended packages. Each install is `R CMD INSTALL` of the CRAN
source tarball. It runs `configure` and compiles the native code with
`MAKEFLAGS=-j8`. For each package, 3 runs with GNU R come first, then 3 runs
with crbcc. The installs run one at a time. The crbcc runs read the hook.
In all suites, the script sets `OPENBLAS_NUM_THREADS=1` for each R process,
because one OpenBLAS thread for each CPU adds CPU time that depends on the
machine.

The packages in `docker/cran50.txt` are the first 50 in the cranlogs download
list for 2026-10-16 to 2026-11-14
(https://cranlogs.r-pkg.org/top/last-month/100).

Each install goes into an empty library. Thus all dependencies come from the
image library, which GNU R compiled.

`timing-profile.R` is the `R_PROFILE_USER` file of each install. It only
reads the clock. Each run gives three numbers:

- **elapsed**: the wall time of `R CMD INSTALL`.
- **native**: the time in `tools:::.shlib_internal`, which compiles the C,
  C++ and Fortran code.
- **bytecode**: the time in `compiler:::tryCmpfun`, which compiles each
  closure to byte code.

This suite runs only in the image build, because it uses the CRAN sources in
the image. The results are in `$CRBCC_BENCHMARKS/install.csv` of the image.

## Closure check and compile benchmark

`closures.R` gets the closures of a package with `crbcc:::package_closures`:
the namespace functions, the S4 methods and the R6 methods that the package
defines.

- The suite `check` compiles each closure one time with each compiler, and
  compares the byte code (`crbcc:::identical_strict`). If a byte code
  differs or a compiler stops with an error, the run fails. If a package has
  no closures, the run also fails.
- The suite `compile` compiles each closure in 3 runs with each compiler. It
  records the compile time and the size of each closure. Each run compiles
  the closures in a different random order. Thus a garbage collection or a
  lazy load occurs in a different closure in each run, and the median of the
  runs removes its time.

The suite `compile` uses the 65 packages of the install benchmark. The suite
`check` uses the base packages without `datasets` and `tcltk`, and the same
65 packages. With `--installed`, the suite `check` also uses all other
installed packages.

The compile speedup in the report and in the README badge is the geometric
mean of the closure speedups. The speedup of a closure is its GNU R time
divided by its crbcc time.

## The image

The image `ghcr.io/prl-prg/crbcc-benchmarks` has R 4.6.2, the system
libraries, uv, pandoc, the benchmark packages with their dependencies, crbcc
and the install benchmark results. The packages come from a CRAN snapshot
(Posit Package Manager, 2026-11-12). The image is for linux/amd64.

The tag is `<crbcc version>-<revision>`, for example `0.2.0-1`. The CI uses
the tag in the `Makefile` (`IMAGE_TAG`). For each new crbcc version, set a
new tag, then build and push a new image. If only the image changes,
increase the revision.

The build:

1. Installs the packages with GNU R and keeps the CRAN sources
   (`docker/build-packages.R`).
2. Installs crbcc from the build context.
3. Runs the install benchmark.
4. Runs the closure check for all installed packages
   (`make test-compile-all-packages`). If a byte code differs, the build
   fails.

The build takes a long time, because it installs each benchmark package six
times. Commit your changes before the build, because the image records the
commit. Run these commands in the crbcc source directory:

```bash
make image        # build the image
make image-push   # push the image (docker login ghcr.io first)
```

Without git, give the commit: `make image CRBCC_COMMIT=<commit>`.

## CI

The workflow `.github/workflows/ci.yml` has these benchmark jobs:

- **compile**: runs the suites `check` and `compile` in the image, with the
  crbcc of the pull request. If a byte code differs, the job fails.
- **report**: renders the report from `install.csv` of the image and
  `compile.csv`. Thus the install results are for the crbcc of the image,
  not for the pull request. The image does not have `reactable`, thus the
  job installs it from the CRAN snapshot of the image into a job library.
- **pages**: for a push to `main`, puts the report on GitHub Pages.
  `README.md` shows its figure and its badge.

## Run in the image

Start a container in the crbcc source directory:

```bash
docker run --rm -it -v "$PWD":/crbcc -w /crbcc \
  ghcr.io/prl-prg/crbcc-benchmarks:$(make -s image-tag) bash
```

To run the closure check for all installed packages, use this command. The
results are in `bench/check.csv`.

```bash
make test-compile-all-packages
```

To run the compile benchmark as the CI does, use these commands:

```bash
mkdir -p bench/pkg-lib && cd bench
R CMD build --no-build-vignettes ..
R CMD INSTALL --library=pkg-lib crbcc_*.tar.gz
uv run --script ../inst/benchmarks/crbcc_bench.py \
  --include '^check/' --include '^compile/' --csv compile.csv
```

To render the report to `bench/site/index.html`, use these commands in
`bench/`:

```bash
mkdir -p rlib
Rscript -e 'install.packages("reactable", "rlib", repos = Sys.getenv("CRAN_SNAPSHOT"))'
cp "$CRBCC_BENCHMARKS/install.csv" .
R_LIBS="$PWD/rlib:$R_LIBS" Rscript -e 'rmarkdown::render("../inst/benchmarks/crbcc_bench.Rmd",
  output_dir = "site", output_file = "index.html", knit_root_dir = getwd(),
  params = list(install = "install.csv", compile = "compile.csv", assets = "site"))'
cp install.csv compile.csv site/
```

### Options of `crbcc_bench.py`

| Option                               | Use                                                                                                      |
| ------------------------------------ | -------------------------------------------------------------------------------------------------------- |
| `--include REGEX`, `--exclude REGEX` | Select the benchmarks, for example `--include '^check/'`.                                                |
| `--runs N`                           | The runs of each package and compiler in the suites `install` and `compile`. The default is 3.           |
| `--installed`                        | The suite `check` also uses all other installed packages.                                                |
| `--commit SHA`                       | The crbcc commit for the CSV header.                                                                     |
| `-j N`                               | Run N benchmarks in parallel. Use it only for the suite `check`, because parallel runs change the times. |
| `--csv FILE`                         | Write the CSV report.                                                                                    |
| `--list`                             | Show the benchmarks, and do not run them.                                                                |

For all options, use `--help`.

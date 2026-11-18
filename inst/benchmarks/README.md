# crbcc benchmarks

The benchmarks have two goals:

- **Correctness**: the crbcc byte code of each closure must be the same as
  the GNU R byte code.
- **Performance**: compare the compile time of crbcc with the compile time
  of the GNU R compiler.

The benchmarks use a real workload: `R CMD INSTALL` of R packages. During
the installation, R compiles all closures of the package, also the S4
methods. The CI runs the benchmarks for each push and pull request. The
[benchmark report](https://prl-prg.github.io/crbcc/) shows the results of
the last push to `main`.

## Files

| File | Contents |
|---|---|
| `crbcc_bench.py` | The benchmark script. It uses [bench](https://github.com/PRL-PRG/bench), which runs the suites and writes the CSV report. |
| `crbcc_bench.Rmd` | The benchmark report. The CI puts it on <https://prl-prg.github.io/crbcc/>. |
| `docker/` | The Docker image `ghcr.io/prl-prg/crbcc-benchmarks`. |
| `../r-cmd-install-hook.R` | The hook that compiles and measures each closure during `R CMD INSTALL`. |

## What the benchmarks measure

Each benchmark is one `R CMD INSTALL` of one package. The hook
`inst/r-cmd-install-hook.R` is the `R_PROFILE_USER` file of the
installation. It replaces `compiler:::tryCmpfun`, which R uses to compile
each closure. The replacement compiles the closure with GNU R (`gnur`) or
with crbcc (`crbcc`), and measures the compile time. The comments in the
hook give its modes (`CRBCC_MODE`).

The metrics:

- **elapsed**: the wall time of `R CMD INSTALL`.
- **bytecode**: the time in the byte-code compiler, for all closures.
- **native**: the time to compile and link the C, C++ and Fortran code.
- **time**: the compile time of one closure.

`crbcc_bench.py` has two suites:

- **install**: installs each package `--runs` times with each compiler, and
  measures the time. This suite does not compare the byte code, because
  this work adds time.
- **check**: only in the Docker image. Installs each package one time with
  crbcc, and compares the byte code of each closure with the GNU R byte code
  in the image. If a byte code is different, the run is a failure. The
  installations can be parallel (`-j`), because this suite does not measure
  the time.

The package sets (`--packages`, you can use the option more than one time):

| Set | Packages |
|---|---|
| `small` | MASS, Matrix, lattice, mgcv and survival (the default) |
| `recommended` | the 15 recommended packages |
| `cran100` | the 100 CRAN packages with the most downloads (`docker/cran100.txt`) |
| `top50` | the first 50 packages of cran100 |
| `all` | only in the Docker image: all 158 packages of the image, also the dependencies |

## The Docker image

The compilation of the native code (C, C++ and Fortran) takes a large part
of the installation time, for example 41% for the 65 benchmark packages. This
time is the same for the two compilers. Thus the image keeps the compiled
native code, and the benchmarks do not compile it again. The image also
gives the same environment to the CI and to a local run.

The image `ghcr.io/prl-prg/crbcc-benchmarks` has R 4.6.2, the system
libraries, uv, pandoc, and 158 packages: cran100, the recommended packages
and their dependencies. The packages come from a CRAN snapshot (Posit
Package Manager, 2026-11-12). `docker/build-packages.R` makes the image
content in `/opt/crbcc-benchmarks`:

1. It installs each package from the source with GNU R. Packages in the
   same dependency level install in parallel. After the installation, the
   script keeps the source tree of the package, with the compiled code and
   the output of `configure`, as `prebuilt/<package>.tar.gz`. `lib/` has the
   installed packages.
2. It installs each package one more time from its tarball, with the hook.
   The hook writes the GNU R byte code of each closure to
   `bytecode/<package>.rds`. The first installation cannot write the byte
   code, because the R JIT compiles different closures when R installs from
   the source.

With `--prebuilt /opt/crbcc-benchmarks`, `crbcc_bench.py` installs
`prebuilt/<package>.tar.gz` with `R CMD INSTALL --no-configure`. The hook
copies the shared object of the package from `lib/`, thus `make` does not
run. Thus the elapsed time is the time of the R part of the installation.

The image tag is a hash of `docker/` and of the hook. If you change these
files, build and push a new image, because the CI uses the tag of its
commit. Use these targets of the top-level `Makefile`:

```bash
make image-tag    # show the tag
make image        # build the image (commit the changes first)
make image-push   # push the image (docker login ghcr.io first)
```

## CI

The workflow `.github/workflows/ci.yml` has these jobs in the image:

- **check**: `make test-compile-all-packages`, the suite check for all 158
  packages.
- **benchmark**: the suite install for the packages top50 and recommended,
  3 runs with each compiler. The two compilers run on the same runner,
  because the report divides their times.
- **report**: makes the report from `check.csv` and `results.csv`. For a
  push to `main`, the job `pages` puts the report on GitHub Pages.

## Run the benchmarks

### In the Docker image

Run these commands in the crbcc source directory. `make -s image-tag` gives
the image of the current commit.

```bash
docker run --rm -it -v "$PWD":/crbcc -w /crbcc \
  ghcr.io/prl-prg/crbcc-benchmarks:$(make -s image-tag) bash
```

In the container, run the check. The results are in `bench/check.csv`, and
`bench/pkg-lib` has crbcc:

```bash
make test-compile-all-packages
```

Then run the suite install in `bench/`, and make the report:

```bash
cd bench
uv run --script ../inst/benchmarks/crbcc_bench.py --r "$(command -v R)" \
  --prebuilt "$CRBCC_BENCHMARKS" --include '^install/' \
  --packages top50 --packages recommended --csv results.csv
Rscript -e 'rmarkdown::render("../inst/benchmarks/crbcc_bench.Rmd",
  output_dir = ".", knit_root_dir = getwd(),
  params = list(csv = "check.csv,results.csv"))'
```

### Without the Docker image

Run these commands in a work directory that has the crbcc source in
`crbcc/` and an R build in `R-4.6.2/`:

```bash
mkdir -p pkg-lib
R-4.6.2/bin/R CMD INSTALL --library=pkg-lib crbcc
crbcc/inst/benchmarks/crbcc_bench.py --r R-4.6.2/bin/R --csv results.csv
```

If R does not have the recommended packages, the script installs them from
`src/library/Recommended`. Thus R must then be a build in the R source tree.
The script downloads the other package sources from the CRAN snapshot. The
installations compile the native code. Without the image, there is no suite
check, thus the report cannot use these results.

### Options

| Option | Use |
|---|---|
| `--r PATH` | The R executable. |
| `--packages SET` | The package set (see above). |
| `--compilers gnur\|crbcc` | The compilers of the suite install. The default is the two compilers. |
| `--runs N` | The runs of each package in the suite install. The default is 3. |
| `--prebuilt PATH` | The image content, `/opt/crbcc-benchmarks`. With this option, the script also runs the suite check. |
| `--include REGEX`, `--exclude REGEX` | Select the benchmarks, for example `--include '^check/'`. |
| `-j N` | Run N benchmarks in parallel. Use it only for the suite check, because parallel runs change the times. |
| `--csv FILE`, `--json FILE` | Write the report. `crbcc_bench.Rmd` reads the CSV. |
| `--list`, `--dry` | Show the benchmarks, and do not run them. |

For all options, use `--help`. The docstring of `crbcc_bench.py` gives the
CSV columns.

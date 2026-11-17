#!/usr/bin/env -S uv run --script --quiet
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "bench @ git+https://github.com/PRL-PRG/bench@4639a83a664901d83db3637bab863374878e96ac",
# ]
#
# [tool.uv]
# exclude-newer = "2026-11-12T00:00:00Z"
# ///
"""Benchmarks of crbcc and the byte-code compiler of GNU R

Run the script in a work directory. It uses the crbcc in pkg-lib/ of that
directory:

  mkdir -p pkg-lib
  R-4.6.2/bin/R CMD INSTALL --library=pkg-lib crbcc
  crbcc/inst/benchmarks/crbcc_bench.py --r R-4.6.2/bin/R --csv results.csv
  Rscript -e 'rmarkdown::render("crbcc/inst/benchmarks/crbcc_bench.Rmd",
                                output_dir = ".", knit_root_dir = getwd())'

The --csv report has one row for each value. These are its columns:
suite, benchmark (the package), run, compiler (gnur or crbcc, empty in the
-fn suites), fn (the closure, only in the -fn suites), metric and value.
crbcc_bench.Rmd reads the report. If the CSV does not have the suites of a
part of the report, the report does not show that part.

  suite        R code                        compiler  runs    metric      unit
  closures     crbcc:::bench_package         gnur      --runs  compile     s
                                             crbcc             heap        MB
                                                               max_rss     kB
  closures-fn  crbcc:::bench_functions       -         1       size        AST nodes
                                                               time_gnur   s
                                                               time_crbcc  s
  install      R CMD INSTALL with            gnur      --runs  elapsed     s
               inst/r-cmd-install-hook.R     crbcc             bytecode    s
                                                               native      s
  install-fn   R CMD INSTALL with the hook   -         1       size        AST nodes
               in mode bench_both                              time_gnur   s
                                                               time_crbcc  s

  compile     the time to compile all closures of the package namespace
  heap        the maximum R heap during the compilation, minus the heap
              before it
  max_rss     the maximum resident set size of the R process
  elapsed     the wall time of R CMD INSTALL
  bytecode    the time in the byte-code compiler, all closures of the package
  native      the time to compile and link src/ (no value without src/)
  size        the number of AST nodes of the closure (crbcc:::function_size)
  time_gnur   the time of one compilation with GNU R (crbcc:::compile_time)
  time_crbcc  the time of one compilation with crbcc (crbcc:::compile_time)

The -fn suites compile each closure with GNU R and crbcc in one R process.
closures-fn uses the closures of the namespace. install-fn uses all closures
that R compiles during the installation, also the S4 methods. If GNU R and
crbcc give different results for a closure, the -fn run is a failure.

Packages (--packages, repeatable, default: base):
  base         the base packages that have closures (no install suites,
               because R CMD INSTALL cannot install a base package)
  small        MASS, Matrix, lattice, mgcv and survival
  recommended  the recommended packages
  cran50       the 50 CRAN packages with the most downloads in the month
               before 2026-11-12, from the CRAN snapshot of that day

Directories in the work directory:
  pkg-lib/      crbcc, the packages and their dependencies
  pkg-src/      the sources from the CRAN snapshot
  pkg-install/  the target library of the install suites

If R does not have the recommended packages, the script installs them from
src/library/Recommended of the R home. Thus R must then be a build in the R
source tree. If a package source is not in src/library/Recommended, the
script downloads it from the CRAN snapshot to pkg-src/.
"""

import os
import re
import shutil
import subprocess
import sys
from collections.abc import Iterable
from dataclasses import field
from pathlib import Path

from bench import (
    BenchError,
    Benchmark,
    Context,
    FixedRuns,
    InvocationResult,
    Metric,
    NoDetection,
    RegexMetric,
    Sample,
    SharedBenchParams,
    SystemProbe,
    Time,
    bench,
    bench_app,
    max_rss,
    print_exception,
    stdout_metric_source,
    suite,
)

PKG_LIB = Path.cwd() / "pkg-lib"
PKG_SRC = Path.cwd() / "pkg-src"
PKG_INSTALL = Path.cwd() / "pkg-install"
HOOK = PKG_LIB / "crbcc" / "r-cmd-install-hook.R"
COMPILERS = ["gnur", "crbcc"]

# The base packages that have closures. The list does not include tcltk,
# because R-4.6.2 has no Tcl/Tk.
BASE = [
    "base", "compiler", "graphics", "grDevices", "grid", "methods",
    "parallel", "splines", "stats", "stats4", "tools", "utils",
]  # fmt: skip

# Each package comes after its dependencies, because R CMD INSTALL installs
# one package at a time.
RECOMMENDED = [
    "MASS", "lattice", "Matrix", "nlme", "survival", "boot", "class",
    "cluster", "codetools", "foreign", "KernSmooth", "mgcv", "nnet", "rpart",
    "spatial",
]  # fmt: skip

CRAN_SNAPSHOT = "https://packagemanager.posit.co/cran/2026-11-12"

# cranlogs.r-pkg.org/top/last-month/50, 2026-10-14 to 2026-11-12.
CRAN50 = [
    "rlang", "ggplot2", "cli", "vctrs", "lifecycle", "Rcpp", "R6", "dplyr",
    "glue", "withr", "fs", "magrittr", "tibble", "cpp11", "RcppEigen",
    "scales", "jsonlite", "pillar", "bslib", "rmarkdown", "S7", "gtable",
    "utf8", "pkgconfig", "curl", "isoband", "generics", "viridisLite", "sass",
    "farver", "xfun", "RColorBrewer", "purrr", "stringr", "labeling",
    "RcppParallel", "otel", "digest", "knitr", "tidyr", "lme4", "stringi",
    "processx", "tidyselect", "callr", "htmltools", "ps", "StanHeaders",
    "httr", "mime",
]  # fmt: skip

PACKAGES = {
    "small": ["MASS", "Matrix", "lattice", "mgcv", "survival"],
    "base": BASE,
    "recommended": RECOMMENDED,
    "cran50": CRAN50,
}


class Params(SharedBenchParams):
    r: Path  # the R executable, for example R-4.6.2/bin/R
    runs: int = 5
    iterations: int = field(
        default=10,
        metadata={"help": "The number of batches of compilations for each closure (the -fn suites)."},
    )
    packages: list[str] | None = field(
        default=None,
        metadata={"choices": list(PACKAGES), "help": "You can use the option more than one time. The default is base."},
    )


# ----------------------------------------------------------------------
# R
# ----------------------------------------------------------------------


def r_env() -> dict[str, str]:
    """Returns a fixed environment. R does not read ~/.Renviron or ~/.R/Makevars."""
    return {
        "PATH": os.environ["PATH"],
        "HOME": os.environ["HOME"],
        "LANG": os.environ.get("LANG", "en_US.UTF-8"),
        "TMPDIR": os.environ.get("TMPDIR", "/tmp"),
        "MAKEFLAGS": "-j8",
        "R_LIBS": str(PKG_LIB),
        "R_LIBS_USER": str(PKG_LIB),
        "R_ENVIRON_USER": "/dev/null",
        "R_MAKEVARS_USER": "/dev/null",
    }


def rscript(params: Params, code: str) -> list[str]:
    return [str(params.r.resolve()), "--vanilla", "--no-echo", "-e", code]


def r_run(params: Params, code: str, capture: bool = False) -> str:
    res = subprocess.run(
        rscript(params, code), env=r_env(), text=True,
        stdout=subprocess.PIPE if capture else None,
    )
    if res.returncode != 0:
        sys.exit(f"error: R stopped with an error: {code}")
    return res.stdout or ""


def r_vector(xs: Iterable[str]) -> str:
    return "c(" + ", ".join(f'"{x}"' for x in xs) + ")"


def missing(params: Params, packages: list[str]) -> list[str]:
    """Returns the packages that R cannot load from pkg-lib/."""
    code = f"cat(Filter(function(p) !requireNamespace(p, quietly = TRUE), {r_vector(packages)}))"
    return r_run(params, code, capture=True).split()


def tarball(params: Params, name: str) -> Path:
    """Returns the path of the source of a package. If there is no source,
    the path does not exist, because --list and --dry do not download the
    sources."""
    dirs = [params.r.resolve().parents[1] / "src" / "library" / "Recommended", PKG_SRC]
    for d in dirs:
        if found := sorted(d.glob(f"{name}_*.tar.gz")):
            return found[-1]
    return PKG_SRC / f"{name}.tar.gz"


def prepare(params: Params, packages: list[str]) -> None:
    code = 'quit(status = !requireNamespace("crbcc", quietly = TRUE) || !exists("bench_closure", asNamespace("crbcc")))'
    if subprocess.run(rscript(params, code), env=r_env()).returncode != 0:
        sys.exit(f"error: install crbcc into {PKG_LIB}: {params.r} CMD INSTALL --library={PKG_LIB} crbcc")

    cran = [p for p in CRAN50 if p in packages]
    # R must have all recommended packages, because some packages use them,
    # for example mgcv uses nlme and lme4 uses Matrix. If R does not have
    # them, install them from the sources in src/library/Recommended. If
    # install.packages installs them from CRAN_SNAPSHOT, their versions are
    # different from these sources.
    rec = RECOMMENDED if any(p not in BASE for p in packages) else []
    if todo := missing(params, rec + cran):
        PKG_LIB.mkdir(exist_ok=True)
        for p in (p for p in rec if p in todo):
            cmd = [str(params.r.resolve()), "CMD", "INSTALL", f"--library={PKG_LIB}", str(tarball(params, p))]
            if subprocess.run(cmd, env=r_env()).returncode != 0:
                sys.exit(f"error: R CMD INSTALL cannot install {p}")
        if todo_cran := [p for p in cran if p in todo]:
            r_run(params, f'install.packages({r_vector(todo_cran)}, lib = "{PKG_LIB}", '
                          f'repos = "{CRAN_SNAPSHOT}", type = "source", Ncpus = 8)')

    # A binary build of R does not have src/library/Recommended.
    if no_src := [p for p in packages if p not in BASE and not tarball(params, p).exists()]:
        PKG_SRC.mkdir(exist_ok=True)
        r_run(params, f'download.packages({r_vector(no_src)}, destdir = "{PKG_SRC}", '
                      f'repos = "{CRAN_SNAPSHOT}", type = "source")')

    if still := missing(params, packages):
        sys.exit(f"error: R cannot load {', '.join(still)} from {PKG_LIB}")
    if no_src := [p for p in packages if p not in BASE and not tarball(params, p).exists()]:
        sys.exit(f"error: no source of {', '.join(no_src)}")


# ----------------------------------------------------------------------
# Suites
# ----------------------------------------------------------------------


def metric(name: str, unit: str) -> RegexMetric:
    return RegexMetric(
        name, rf"^RBC {name}: ([0-9.]+)$", stdout_metric_source, unit=unit
    ).lower_is_better()


class FnMetric(Metric):
    """Reads the lines "RBC fn<TAB>closure<TAB>size<TAB>time_gnur<TAB>time_crbcc"
    that crbcc:::bench_closure writes. Each value gives one sample, with the
    closure in the column "fn"."""

    COLUMNS = [("size", ""), ("time_gnur", "s"), ("time_crbcc", "s")]

    def __init__(self) -> None:
        super().__init__("fn")

    def process(self, data: InvocationResult) -> Iterable[Sample]:
        for line in (data.stdout or "").splitlines():
            if not line.startswith("RBC fn\t"):
                continue
            _, fn, *values = line.split("\t")
            for (name, unit), value in zip(self.COLUMNS, values):
                if value != "NA":
                    direction = "lower better" if unit else "uncomparable"
                    yield Sample(metric=name, value=float(value), unit=unit,
                                 direction=direction, extra={"fn": fn})


def success(output: str):
    """Returns the check of a run. The run must exit with 0, write lines
    "RBC <output>" and write no lines "RBC differs"."""

    def check(result: InvocationResult) -> str | None:
        stdout = result.stdout or ""
        if result.returncode != 0:
            return f"exit code {result.returncode}"
        if not re.search(rf"^RBC {output}\b", stdout, re.MULTILINE):
            return f"no 'RBC {output}' in stdout"
        if differs := re.findall(r"^RBC differs: (.*)$", stdout, re.MULTILINE):
            return f"the results of {len(differs)} closures are not the same as GNU R: {', '.join(differs[:10])}"
        return None

    return check


def install_command(ctx: Context[Params]) -> list[str]:
    tar = tarball(ctx.params, ctx.benchmark)
    return [str(ctx.params.r.resolve()), "CMD", "INSTALL", f"--library={PKG_INSTALL}", str(tar)]


def install_env(mode: str | None):
    def env(ctx: Context[Params]) -> dict[str, str]:
        return r_env() | {
            "R_PROFILE_USER": str(HOOK),
            "CRBCC_MODE": mode or f"bench_{ctx.data.compiler}",
            "CRBCC_ITERATIONS": str(ctx.params.iterations),
        }

    return env


def remove_package(b: Benchmark) -> None:
    """Removes the package from pkg-install/. Thus each run is a new
    installation."""
    PKG_INSTALL.mkdir(exist_ok=True)
    for d in (b.name, f"00LOCK-{b.name}"):
        shutil.rmtree(PKG_INSTALL / d, ignore_errors=True)


def suites(params: Params):
    packages = list(dict.fromkeys(p for n in params.packages or ["base"] for p in PACKAGES[n]))
    installable = [p for p in packages if p not in BASE]

    # Do not prepare the packages for --list and --dry, because they do not
    # run a benchmark.
    if not (params.list or params.dry):
        prepare(params, packages)

    def new(name: str, pkgs: list[str]):
        return suite(name).add(*(bench(p) for p in pkgs)).with_env(lambda ctx: r_env()).with_timeout(3600)

    timed = lambda ctx: FixedRuns(ctx.params.runs)  # noqa: E731
    once = lambda ctx: FixedRuns(1)  # noqa: E731

    res = []

    res.append(new("closures", packages)
        .with_matrix(compiler=COMPILERS)
        .with_command(lambda ctx: rscript(
            ctx.params, f'crbcc:::bench_package("{ctx.benchmark}", "{ctx.data.compiler}")'))
        .with_metric(metric("compile", "s"), metric("heap", "MB"), max_rss())
        .with_success(success("compile"))
        .with_runs(timed))
                   
    res.append(new("closures-fn", packages)
        .with_command(lambda ctx: rscript(
            ctx.params, f'crbcc:::bench_functions("{ctx.benchmark}", {ctx.params.iterations})'))
        .with_metric(FnMetric())
        .with_outlier_detection(NoDetection())
        .with_success(success("fn"))
        .with_runs(once))


    if installable:
        res.append(
            new("install", installable)
            .with_matrix(compiler=COMPILERS)
            .with_command(install_command)
            .with_env(install_env(None), override=True)
            .with_setup(remove_package)
            .with_metric(Time(), metric("bytecode", "s"), metric("native", "s"))
            .with_success(success("bytecode"))
            .with_runs(timed))

        res.append(new("install-fn", installable)
            .with_command(install_command)
            .with_env(install_env("bench_both"), override=True)
            .with_setup(remove_package)
            .with_metric(FnMetric())
            .with_outlier_detection(NoDetection())
            .with_success(success("fn"))
            .with_runs(once))

    return res


if __name__ == "__main__":
    # The probe writes the machine data to the CSV as "# key: value" lines.
    # The report shows them in the section Environment.
    app = bench_app(
        "Benchmarks of crbcc and the byte-code compiler of GNU R", params=Params
    ).generator(suites).with_probe(SystemProbe())
    # app.main() exits with 0 also if a run is a failure. Thus use
    # app.run_cli(), which gives the report.
    try:
        report = app.run_cli()
    except BenchError as e:
        print_exception(e, with_traceback=False)
        sys.exit(e.exit_code)
    sys.exit(1 if report.failures else 0)

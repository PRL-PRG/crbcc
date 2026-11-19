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
"""Benchmarks of crbcc and the GNU R byte-code compiler

Suites:
  install   Installs the CRAN source of each package --runs times with each
            compiler: R CMD INSTALL with configure and the native code,
            MAKEFLAGS=-j8. timing-profile.R measures the time in the
            native-code compiler and in the byte-code compiler. This suite
            runs only in the benchmark image, which has the sources in
            $CRBCC_BENCHMARKS/src and the dependencies.
  check     Compiles each closure of each package with the two compilers one
            time, and compares the byte code (closures.R). A run is a failure
            if a byte code is different or a compiler stops with an error.
  compile   Compiles each closure of each package --runs times with each
            compiler, and measures the compile time of each closure. Each run
            compiles the closures in a different random order.

The suites install and compile use the 50 CRAN packages in docker/cran50.txt
and the 15 recommended packages. The suite check uses the base packages in
BASE and the same 65 packages. With --installed, the suite check also uses all
other installed packages.

To run one suite, use --include, for example --include '^check/'. Use -j only
for the suite check, because parallel runs change the times.

R and Rscript must be on the PATH. If the work directory has pkg-lib/, R uses
the crbcc in it:

  mkdir pkg-lib
  R CMD INSTALL --library=pkg-lib crbcc_0.2.0.tar.gz
  crbcc/inst/benchmarks/crbcc_bench.py --include '^check/' --csv check.csv

The --csv report starts with "# key: value" lines: the machine, the start
time (timestamp), r_version, crbcc_version and crbcc_commit (--commit). Then
it has one row for each value. The columns are: suite, benchmark (the
package), run, compiler (gnur or crbcc, empty in the suite check), failure,
iteration, warmup, metric, value, unit and lower_is_better. With the suite
compile, there are also the columns closure and size, in a random order.

  suite     metric    unit  value
  install   elapsed   s     the wall time of R CMD INSTALL
  install   native    s     the time in tools:::.shlib_internal (0 without
                            native code)
  install   bytecode  s     the time in compiler:::tryCmpfun
  check     closures        the number of closures
  compile   compile   s     the compile time of one closure. closure is its
                            name and size is crbcc:::function_size.

Each run of the suites check and compile also has the metric elapsed, the
wall time of the run. Thus the CSV has a row also for a failed run.

Directories in the work directory:
  pkg-lib/      crbcc, if it is not the crbcc of the R library
  pkg-install/  the target library of the suite install, empty at the
                start of each run
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
    FixedRuns,
    Fingerprint,
    InvocationResult,
    Metric,
    NoDetection,
    Probe,
    RegexMetric,
    Sample,
    SharedBenchParams,
    SystemProbe,
    Time,
    bench,
    bench_app,
    print_exception,
    stdout_metric_source,
    suite,
)

HERE = Path(__file__).resolve().parent
PKG_LIB = Path.cwd() / "pkg-lib"
PKG_INSTALL = Path.cwd() / "pkg-install"
COMPILERS = ["gnur", "crbcc"]

# The base packages without datasets, which has no closures, and without
# tcltk, which cannot load in the image, because the image has no Tcl/Tk.
BASE = [
    "base", "compiler", "graphics", "grDevices", "grid", "methods",
    "parallel", "splines", "stats", "stats4", "tools", "utils",
]  # fmt: skip

RECOMMENDED = [
    "MASS", "lattice", "Matrix", "nlme", "survival", "boot", "class",
    "cluster", "codetools", "foreign", "KernSmooth", "mgcv", "nnet", "rpart",
    "spatial",
]  # fmt: skip

# The Docker image installs the same packages.
CRAN = [
    line.strip()
    for line in (HERE / "docker" / "cran50.txt").read_text().splitlines()
    if line.strip()
]


class Params(SharedBenchParams):
    runs: int = field(
        default=3,
        metadata={"help": "The runs of each package and compiler in the suites install and compile."},
    )
    installed: bool = field(
        default=False,
        metadata={"action": "store_true", "help": "The suite check also uses all other installed packages."},
    )
    commit: str = field(
        default="unknown",
        metadata={"help": "The crbcc commit for the CSV header."},
    )


# ----------------------------------------------------------------------
# R
# ----------------------------------------------------------------------


def r_env() -> dict[str, str]:
    """Returns a fixed environment. R does not read ~/.Renviron or ~/.R/Makevars."""
    libs = [str(PKG_LIB)] + ([os.environ["R_LIBS"]] if os.environ.get("R_LIBS") else [])
    return {
        "PATH": os.environ["PATH"],
        "HOME": os.environ["HOME"],
        "LANG": os.environ.get("LANG", "en_US.UTF-8"),
        "TMPDIR": os.environ.get("TMPDIR", "/tmp"),
        "MAKEFLAGS": "-j8",
        "R_LIBS": ":".join(libs),
        "R_ENVIRON_USER": "/dev/null",
        "R_MAKEVARS_USER": "/dev/null",
        # Without this, each R process starts one OpenBLAS thread for each
        # CPU. These threads use CPU time that depends on the machine.
        "OPENBLAS_NUM_THREADS": "1",
    }


def rscript(code: str) -> str:
    return subprocess.run(
        ["Rscript", "--vanilla", "-e", code], env=r_env(), text=True,
        stdout=subprocess.PIPE, check=True,
    ).stdout


def installed() -> list[str]:
    """Returns the installed packages that are not base packages."""
    return rscript(
        'ip <- installed.packages(); '
        'cat(unique(rownames(ip)[!ip[, "Priority"] %in% "base"]), sep = "\\n")'
    ).split()


class RProbe(Probe):
    """Adds the R version, and the version and commit of crbcc."""

    def __init__(self, commit: str) -> None:
        self.commit = commit

    def collect(self) -> Fingerprint:
        r, crbcc = rscript('cat(R.version.string, format(packageVersion("crbcc")), sep = "\\n")').splitlines()
        return Fingerprint({"r_version": r, "crbcc_version": crbcc, "crbcc_commit": self.commit})


# ----------------------------------------------------------------------
# Metrics
# ----------------------------------------------------------------------


class Total(Metric):
    """The sum of the lines "RBC <name>: <s>". Each R process of an
    installation writes its own line. Without a line, the value is 0."""

    def __init__(self, name: str) -> None:
        super().__init__(name, "s", "lower better")
        self.regex = re.compile(rf"^RBC {name}: ([0-9.]+)$", re.MULTILINE)

    def process(self, data: InvocationResult) -> Iterable[Sample]:
        yield self.get_sample(sum(float(v) for v in self.regex.findall(data.stdout or "")))


class ClosureMetric(Metric):
    """Reads the lines "RBC closure<TAB><s><TAB><size><TAB><name>" of
    closures.R. Each line gives one sample."""

    def __init__(self) -> None:
        super().__init__("compile", "s", "lower better")
        self.regex = re.compile(r"^RBC closure\t([^\t\n]+)\t([^\t\n]+)\t(.*)$", re.MULTILINE)

    def process(self, data: InvocationResult) -> Iterable[Sample]:
        for time, size, name in self.regex.findall(data.stdout or ""):
            yield self.get_sample(float(time), extra={"closure": name, "size": int(size)})


def success(result: InvocationResult) -> str | None:
    """Returns the failure of a run. The run must exit with 0."""
    if result.returncode != 0:
        return f"exit code {result.returncode}: {(result.stderr or result.stdout or '')[-500:]}"
    return None


# ----------------------------------------------------------------------
# Suites
# ----------------------------------------------------------------------


def tarball(name: str) -> Path:
    """Returns the CRAN source of a package in the image. If there is no
    source, the path does not exist and R CMD INSTALL fails."""
    src = Path(os.environ.get("CRBCC_BENCHMARKS", "/opt/crbcc-benchmarks")) / "src"
    return next(src.glob(f"{name}_*.tar.gz"), src / f"{name}.tar.gz")


def empty_target(b: Benchmark) -> None:
    """Makes the target library empty. R CMD INSTALL puts the target library
    first in R_LIBS of its R processes. Without this step, an install can
    load its dependencies from a previous install with the other compiler."""
    shutil.rmtree(PKG_INSTALL, ignore_errors=True)
    PKG_INSTALL.mkdir()


def closures_r(mode: str, package: str) -> list[str]:
    return ["Rscript", "--vanilla", str(HERE / "closures.R"), mode, package]


def suites(params: Params):
    packages = list(dict.fromkeys(BASE + CRAN + RECOMMENDED + (installed() if params.installed else [])))

    def package_suite(name: str, packages: list[str]):
        return (
            suite(name)
            .add(*(bench(p) for p in packages))
            .with_env(r_env())
            .with_success(success)
            .with_outlier_detection(NoDetection())
            .with_timeout(3600)
        )

    return [
        package_suite("install", CRAN + RECOMMENDED)
        .with_matrix(compiler=COMPILERS)
        .with_command(lambda ctx: ["R", "CMD", "INSTALL", f"--library={PKG_INSTALL}", str(tarball(ctx.benchmark))])
        .with_env(lambda ctx: {
            "R_PROFILE_USER": str(HERE / "timing-profile.R"),
            "CRBCC_BENCH_COMPILER": ctx.data.compiler,
        })
        .with_setup(empty_target)
        .with_metric(Time(), Total("native"), Total("bytecode"))
        .with_runs(lambda ctx: FixedRuns(ctx.params.runs)),
        package_suite("check", packages)
        .with_command(lambda ctx: closures_r("check", ctx.benchmark))
        .with_metric(Time(), RegexMetric("closures", r"^RBC closures: (\d+)$", stdout_metric_source)),
        package_suite("compile", CRAN + RECOMMENDED)
        .with_matrix(compiler=COMPILERS)
        .with_command(lambda ctx: closures_r(ctx.data.compiler, ctx.benchmark))
        .with_metric(Time(), ClosureMetric())
        .with_runs(lambda ctx: FixedRuns(ctx.params.runs)),
    ]


if __name__ == "__main__":
    # The probes write the "# key: value" lines of the CSV.
    app = (
        bench_app("Benchmarks of crbcc and the GNU R byte-code compiler", params=Params)
        .generator(suites)
        .with_probe(lambda params: SystemProbe() & RProbe(params.commit))
    )
    # app.main() exits with 0 also if a run is a failure. Thus use
    # app.run_cli(), which gives the report.
    try:
        report = app.run_cli()
    except BenchError as e:
        print_exception(e, with_traceback=False)
        sys.exit(e.exit_code)
    sys.exit(1 if report.failures else 0)

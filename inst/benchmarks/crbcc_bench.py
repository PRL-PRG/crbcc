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

Each benchmark installs one package with R CMD INSTALL. The hook
inst/r-cmd-install-hook.R (R_PROFILE_USER) measures the compile time of each
closure that R compiles during the installation.

Run the script in a work directory. The script uses the crbcc in pkg-lib/:

  mkdir -p pkg-lib
  R-4.6.2/bin/R CMD INSTALL --library=pkg-lib crbcc
  crbcc/inst/benchmarks/crbcc_bench.py --r R-4.6.2/bin/R --csv results.csv

Suites:
  install  Installs each package --runs times with each compiler, and
           measures the time.
  check    Only with --prebuilt. Installs each package one time with crbcc,
           and compares the byte code of each closure with the GNU R byte
           code in the image. A run is a failure if a closure has a
           different byte code, or if the image has no byte code for a
           closure. The suite install does not compare, because the
           comparison adds time.
To run one suite, use --include '^install/' or --include '^check/'. To
install N packages in parallel in the suite check, use -j N. Do not use -j
for the suite install, because parallel runs change the times.

The --csv report has one row for each value. Its columns are: suite,
benchmark (the package), run, compiler (gnur or crbcc), metric, value, and
for the metric time: fn (the closure key), check (only in the suite check:
same, differs or unknown) and size (only in the suite check).
crbcc_bench.Rmd reads the report. It uses the suite check, thus only the
results from the Docker image (--prebuilt).

  metric    unit       value
  elapsed   s          the wall time of R CMD INSTALL
  bytecode  s          the time in the byte-code compiler, all closures
  native    s          the time to compile and link src/ (no value without src/)
  time      s          the compile time of one closure

Packages (--packages, repeatable, default: small):
  small        MASS, Matrix, lattice, mgcv and survival
  recommended  the recommended packages
  cran100      the 100 CRAN packages in docker/cran100.txt (the most
               downloads in the month to 2026-11-14), from the CRAN snapshot
  top50        the first 50 packages of cran100
  all          only with --prebuilt: all packages in the image, also the
               dependencies

Directories in the work directory:
  pkg-lib/      crbcc, the packages and their dependencies
  pkg-src/      the sources from the CRAN snapshot
  pkg-install/  the target library of R CMD INSTALL. The suite check uses
                pkg-install/check/<package>/.

If R does not have the recommended packages, the script installs them from
src/library/Recommended of the R home. Thus R must then be a build in the R
source tree. If a package source is not in src/library/Recommended, the
script downloads it from the CRAN snapshot to pkg-src/.

In the image ghcr.io/prl-prg/crbcc-benchmarks (docker/Dockerfile), use
--prebuilt /opt/crbcc-benchmarks. The script then uses the packages in
<prebuilt>/lib and installs <prebuilt>/prebuilt/<package>.tar.gz with
--no-configure. These tarballs have the compiled code of a previous
installation, thus R CMD INSTALL does not compile it again. The image also
has <prebuilt>/bytecode/<package>.rds, the GNU R byte code of each package
for the suite check.
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
    print_exception,
    stdout_metric_source,
    suite,
)

PKG_LIB = Path.cwd() / "pkg-lib"
PKG_SRC = Path.cwd() / "pkg-src"
PKG_INSTALL = Path.cwd() / "pkg-install"
HOOK = PKG_LIB / "crbcc" / "r-cmd-install-hook.R"
COMPILERS = ["gnur", "crbcc"]

# Each package comes after its dependencies, because R CMD INSTALL installs
# one package at a time.
RECOMMENDED = [
    "MASS", "lattice", "Matrix", "nlme", "survival", "boot", "class",
    "cluster", "codetools", "foreign", "KernSmooth", "mgcv", "nnet", "rpart",
    "spatial",
]  # fmt: skip

CRAN_SNAPSHOT = "https://packagemanager.posit.co/cran/2026-11-12"

# The Docker image uses the same file.
CRAN100 = [
    line.strip()
    for line in (Path(__file__).parent / "docker" / "cran100.txt").read_text().splitlines()
    if line.strip() and not line.startswith("#")
]

PACKAGES = {
    "small": ["MASS", "Matrix", "lattice", "mgcv", "survival"],
    "recommended": RECOMMENDED,
    "cran100": CRAN100,
    "top50": CRAN100[:50],
}


class Params(SharedBenchParams):
    r: Path  # the R executable, for example R-4.6.2/bin/R
    runs: int = 3
    packages: list[str] | None = field(
        default=None,
        metadata={"choices": [*PACKAGES, "all"], "help": "You can use the option more than one time. The default is small."},
    )
    compilers: list[str] | None = field(
        default=None,
        metadata={"choices": COMPILERS, "help": "You can use the option more than one time. The default is gnur and crbcc."},
    )
    prebuilt: Path | None = field(
        default=None,
        metadata={"help": "The directory of the prebuilt packages, /opt/crbcc-benchmarks in the Docker image."},
    )


# ----------------------------------------------------------------------
# R
# ----------------------------------------------------------------------


def r_env(params: Params) -> dict[str, str]:
    """Returns a fixed environment. R does not read ~/.Renviron or ~/.R/Makevars."""
    libs = [str(PKG_LIB)] + ([str(params.prebuilt / "lib")] if params.prebuilt else [])
    return {
        "PATH": os.environ["PATH"],
        "HOME": os.environ["HOME"],
        "LANG": os.environ.get("LANG", "en_US.UTF-8"),
        "TMPDIR": os.environ.get("TMPDIR", "/tmp"),
        "MAKEFLAGS": "-j8",
        "R_LIBS": ":".join(libs),
        "R_LIBS_USER": str(PKG_LIB),
        "R_ENVIRON_USER": "/dev/null",
        "R_MAKEVARS_USER": "/dev/null",
    }


def rscript(params: Params, code: str) -> list[str]:
    return [str(params.r.resolve()), "--vanilla", "--no-echo", "-e", code]


def r_run(params: Params, code: str, capture: bool = False) -> str:
    res = subprocess.run(
        rscript(params, code), env=r_env(params), text=True,
        stdout=subprocess.PIPE if capture else None,
    )
    if res.returncode != 0:
        sys.exit(f"error: R stopped with an error: {code}")
    return res.stdout or ""


def r_vector(xs: Iterable[str]) -> str:
    return "c(" + ", ".join(f'"{x}"' for x in xs) + ")"


def missing(params: Params, packages: list[str]) -> list[str]:
    """Returns the packages that R cannot load."""
    code = f"cat(Filter(function(p) !requireNamespace(p, quietly = TRUE), {r_vector(packages)}))"
    return r_run(params, code, capture=True).split()


def tarball(params: Params, name: str) -> Path:
    """Returns the path of the package source. If there is no source, the
    path does not exist, because --list and --dry do not download the
    sources."""
    if params.prebuilt:
        return params.prebuilt / "prebuilt" / f"{name}.tar.gz"
    dirs = [params.r.resolve().parents[1] / "src" / "library" / "Recommended", PKG_SRC]
    for d in dirs:
        if found := sorted(d.glob(f"{name}_*.tar.gz")):
            return found[-1]
    return PKG_SRC / f"{name}.tar.gz"


def prepare(params: Params, packages: list[str]) -> None:
    if missing(params, ["crbcc"]):
        sys.exit(f"error: install crbcc into {PKG_LIB}: {params.r} CMD INSTALL --library={PKG_LIB} crbcc")

    if not params.prebuilt:
        cran = [p for p in CRAN100 if p in packages]
        # R must have all recommended packages, because some packages use
        # them, for example mgcv uses nlme and lme4 uses Matrix. If R does
        # not have them, install them from the sources in
        # src/library/Recommended. If install.packages installs them from
        # CRAN_SNAPSHOT, their versions are different from these sources.
        if todo := missing(params, RECOMMENDED + cran):
            PKG_LIB.mkdir(exist_ok=True)
            for p in (p for p in RECOMMENDED if p in todo):
                cmd = [str(params.r.resolve()), "CMD", "INSTALL", f"--library={PKG_LIB}", str(tarball(params, p))]
                if subprocess.run(cmd, env=r_env(params)).returncode != 0:
                    sys.exit(f"error: R CMD INSTALL cannot install {p}")
            if todo_cran := [p for p in cran if p in todo]:
                r_run(params, f'install.packages({r_vector(todo_cran)}, lib = "{PKG_LIB}", '
                              f'repos = "{CRAN_SNAPSHOT}", type = "source", Ncpus = 8)')

        # A binary build of R does not have src/library/Recommended.
        if no_src := [p for p in packages if not tarball(params, p).exists()]:
            PKG_SRC.mkdir(exist_ok=True)
            r_run(params, f'download.packages({r_vector(no_src)}, destdir = "{PKG_SRC}", '
                          f'repos = "{CRAN_SNAPSHOT}", type = "source")')

    if still := missing(params, packages):
        sys.exit(f"error: R cannot load {', '.join(still)}")
    if no_src := [p for p in packages if not tarball(params, p).exists()]:
        sys.exit(f"error: no source of {', '.join(no_src)}")


# ----------------------------------------------------------------------
# Suite
# ----------------------------------------------------------------------


def metric(name: str, unit: str) -> RegexMetric:
    return RegexMetric(
        name, rf"^RBC {name}: ([0-9.]+)$", stdout_metric_source, unit=unit
    ).lower_is_better()


FN_LINE = re.compile(r"^RBC fn\t([^\t\n]+)\t([^\t\n]+)\t([^\t\n]+)\t([^\t\n]+)$", re.MULTILINE)


class FnMetric(Metric):
    """Reads the lines "RBC fn<TAB>key<TAB>time<TAB>check<TAB>size" that
    the hook writes. Each line gives one sample of the metric time."""

    def __init__(self) -> None:
        super().__init__("fn")

    def process(self, data: InvocationResult) -> Iterable[Sample]:
        for key, time, check, size in FN_LINE.findall(data.stdout or ""):
            yield Sample(metric="time", value=float(time), unit="s", direction="lower better",
                         extra={"fn": key, "check": "" if check == "-" else check,
                                "size": "" if size == "NA" else size})


def success(result: InvocationResult) -> str | None:
    """Returns the failure of a run. The run must exit with 0."""
    if result.returncode != 0:
        return f"exit code {result.returncode}: {(result.stderr or result.stdout or '')[-500:]}"
    return None


def check_success(result: InvocationResult) -> str | None:
    """Returns the failure of a run of the suite check. The byte code of each
    closure must be the same as the GNU R byte code in the baseline. A
    package without closures, for example BH, writes no "RBC" lines. This is
    not a failure."""
    if failure := success(result):
        return failure
    if differs := re.findall(r"^RBC differs: (.*)$", result.stdout or "", re.MULTILINE):
        return f"the byte code of {len(differs)} closures is not the same as GNU R: {'; '.join(differs[:10])}"
    if unknown := [key for key, _, check, _ in FN_LINE.findall(result.stdout or "") if check == "unknown"]:
        return f"the baseline has no byte code for {len(unknown)} closures: {'; '.join(unknown[:10])}"
    return None


def target(suite: str, name: str) -> Path:
    """Returns the target library of R CMD INSTALL. In the suite check, each
    package has its own library, because the runs can be parallel (-j). Thus
    R does not load a package that a different run installs at the same
    time."""
    return PKG_INSTALL / "check" / name if suite == "check" else PKG_INSTALL


def install_command(ctx: Context[Params]) -> list[str]:
    tar = tarball(ctx.params, ctx.benchmark)
    # A prebuilt tarball has the output of configure.
    configure = ["--no-configure"] if ctx.params.prebuilt else []
    lib = target(ctx.suite, ctx.benchmark)
    return [str(ctx.params.r.resolve()), "CMD", "INSTALL", *configure, f"--library={lib}", str(tar)]


def install_env(ctx: Context[Params], mode: str) -> dict[str, str]:
    # With --prebuilt, the hook copies the shared objects from the image
    # library, thus R CMD INSTALL does not compile src/ again. In the mode
    # check, the hook compares the crbcc byte code with the GNU R byte code
    # in CRBCC_BASELINE.
    prebuilt = {
        "CRBCC_PREBUILT_LIB": str(ctx.params.prebuilt / "lib"),
        "CRBCC_BASELINE": str(ctx.params.prebuilt / "bytecode"),
    } if ctx.params.prebuilt else {}
    return r_env(ctx.params) | prebuilt | {
        "R_PROFILE_USER": str(HOOK),
        "CRBCC_MODE": mode,
    }


def remove_package(b: Benchmark) -> None:
    """Removes the package from its target library. Thus each run is a new
    installation."""
    lib = target(b.suite, b.name)
    lib.mkdir(parents=True, exist_ok=True)
    for d in (b.name, f"00LOCK-{b.name}"):
        shutil.rmtree(lib / d, ignore_errors=True)


def package_set(params: Params, name: str) -> list[str]:
    """Returns the packages of one --packages value."""
    if name != "all":
        return PACKAGES[name]
    if not params.prebuilt:
        sys.exit("error: --packages all needs --prebuilt")
    return sorted(p.name.removesuffix(".tar.gz") for p in (params.prebuilt / "prebuilt").glob("*.tar.gz"))


def suites(params: Params):
    packages = list(dict.fromkeys(p for n in params.packages or ["small"] for p in package_set(params, n)))

    # Do not prepare the packages for --list and --dry, because they do not
    # run a benchmark.
    if not (params.list or params.dry):
        prepare(params, packages)

    def install_suite(name: str, compilers: list[str]):
        return (
            suite(name)
            .add(*(bench(p) for p in packages))
            .with_matrix(compiler=compilers)
            .with_command(install_command)
            .with_setup(remove_package)
            .with_outlier_detection(NoDetection())
            .with_timeout(3600)
        )

    # The suite check uses the GNU R byte code in the image.
    check = [
        install_suite("check", ["crbcc"])
        .with_env(lambda ctx: install_env(ctx, "check"))
        .with_metric(Time(), FnMetric())
        .with_success(check_success)
        .with_runs(lambda ctx: FixedRuns(1))
    ] if params.prebuilt else []

    return check + [
        install_suite("install", params.compilers or COMPILERS)
        .with_env(lambda ctx: install_env(ctx, f"bench_{ctx.data.compiler}"))
        .with_metric(Time(), metric("bytecode", "s"), metric("native", "s"), FnMetric())
        .with_success(success)
        .with_runs(lambda ctx: FixedRuns(ctx.params.runs))
    ]


if __name__ == "__main__":
    # The probe writes the machine data to the CSV as "# key: value" lines.
    # The report shows them in the section Environment.
    app = bench_app(
        "Benchmarks of crbcc and the GNU R byte-code compiler", params=Params
    ).generator(suites).with_probe(SystemProbe())
    # app.main() exits with 0 also if a run is a failure. Thus use
    # app.run_cli(), which gives the report.
    try:
        report = app.run_cli()
    except BenchError as e:
        print_exception(e, with_traceback=False)
        sys.exit(e.exit_code)
    sys.exit(1 if report.failures else 0)

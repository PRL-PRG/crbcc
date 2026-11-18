# The image ghcr.io/prl-prg/crbcc-benchmarks for the crbcc benchmarks. It has
# R, the system libraries, and the benchmark packages: cran100.txt, the
# recommended packages and their dependencies. build-packages.R installs the
# packages one time with GNU R and keeps the compiled code, thus the
# benchmarks do not compile the native code again. The script also saves the
# GNU R byte code of each package for the suite check.
#
# The image tag is a hash of this directory and of inst/r-cmd-install-hook.R,
# because the hook saves the GNU R byte code. If you change one of these files,
# build and push a new image. The CI uses the image tag of its commit. To
# build and push the image, commit the files. Then run make image and
# make image-push in the crbcc source directory (refer to the Makefile).

FROM rocker/r-ver:4.6.2

ARG CRAN_SNAPSHOT=https://packagemanager.posit.co/cran/2026-11-12
ENV CRAN_SNAPSHOT=${CRAN_SNAPSHOT} \
    CRBCC_BENCHMARKS=/opt/crbcc-benchmarks

LABEL org.opencontainers.image.source=https://github.com/PRL-PRG/crbcc \
      org.opencontainers.image.description="R and prebuilt packages for the crbcc benchmarks"

# The system libraries of the packages, from pak::pkg_sysreqs() for Ubuntu
# 26.04. crbcc_bench.py and uv use Python and git.
RUN apt-get update && apt-get install -y --no-install-recommends \
      cmake libuv1-dev pandoc libcurl4-openssl-dev libssl-dev libicu-dev \
      libx11-dev zlib1g-dev libfontconfig1-dev libfreetype6-dev \
      libfribidi-dev libharfbuzz-dev libxml2-dev libgdal-dev gdal-bin \
      libgeos-dev libproj-dev libsqlite3-dev libjpeg-dev libpng-dev \
      libtiff-dev libwebp-dev \
      python3 git \
    && rm -rf /var/lib/apt/lists/*

COPY --from=ghcr.io/astral-sh/uv:0.13.6 /uv /usr/local/bin/uv

COPY inst/benchmarks/docker/cran100.txt inst/benchmarks/docker/build-packages.R \
     inst/r-cmd-install-hook.R /tmp/build/
RUN Rscript /tmp/build/build-packages.R /tmp/build && rm -rf /tmp/build

ENV R_LIBS=${CRBCC_BENCHMARKS}/lib

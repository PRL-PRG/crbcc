# The image ghcr.io/prl-prg/crbcc-benchmarks for the crbcc benchmarks. It has
# R, the system libraries, the benchmark packages, crbcc, and the install
# benchmark results in $CRBCC_BENCHMARKS/install.csv. The build context is the
# crbcc source. The build also compares the crbcc byte code with the GNU R
# byte code for each closure of each installed package. If a step fails, the
# build fails. To build the image, refer to the target image of the Makefile.

FROM rocker/r-ver:4.6.2

ARG CRAN_SNAPSHOT=https://packagemanager.posit.co/cran/2026-11-12
ENV CRAN_SNAPSHOT=${CRAN_SNAPSHOT} \
    CRBCC_BENCHMARKS=/opt/crbcc-benchmarks \
    R_LIBS=/opt/crbcc-benchmarks/lib

LABEL org.opencontainers.image.source=https://github.com/PRL-PRG/crbcc \
      org.opencontainers.image.description="R, packages and install benchmark results for the crbcc benchmarks"

# The system libraries of the packages, from their SystemRequirements and
# from the libraries that their shared objects use. The packages and the
# report use pandoc. crbcc_bench.py and uv use Python and git.
RUN apt-get update && apt-get install -y --no-install-recommends \
      cmake libuv1-dev pandoc libcurl4-openssl-dev libssl-dev libicu-dev \
      zlib1g-dev python3 git \
    && rm -rf /var/lib/apt/lists/*

COPY --from=ghcr.io/astral-sh/uv:0.13.6 /uv /usr/local/bin/uv

COPY inst/benchmarks/docker/cran50.txt inst/benchmarks/docker/build-packages.R /tmp/build/
RUN Rscript /tmp/build/build-packages.R /tmp/build/cran50.txt && rm -rf /tmp/build

COPY . /tmp/crbcc
RUN cd /tmp/crbcc && R CMD build --no-build-vignettes . && R CMD INSTALL crbcc_*.tar.gz

# Run the install benchmark. The runs are serial, because parallel runs
# change the times.
ARG CRBCC_COMMIT=unknown
RUN mkdir /tmp/bench && cd /tmp/bench \
    && uv run --script /tmp/crbcc/inst/benchmarks/crbcc_bench.py --include '^install/' \
         --commit "$CRBCC_COMMIT" --no-progress --csv "$CRBCC_BENCHMARKS/install.csv" \
    && rm -rf /tmp/bench

RUN make -C /tmp/crbcc test-compile-all-packages && rm -rf /tmp/crbcc

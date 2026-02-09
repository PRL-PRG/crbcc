FROM rocker/r-base:latest

WORKDIR /project

    # Install wget
RUN apt-get update && apt-get install -y \
    wget \
    build-essential \
    bear \
    clang-format \
    && \
    R_VERSION=$(R --version | head -n1 | awk '{print $3}') && \
    wget -qO- "https://cran.r-project.org/src/base/R-4/R-${R_VERSION}.tar.gz" | \
    tar -xz -C /usr/local/src/ && \
    mv /usr/local/src/R-${R_VERSION} /usr/local/src/R

# Install dependencies
COPY . .
RUN R -e "install.packages(c('remotes', 'devtools')); \
          remotes::install_deps(dependencies = TRUE); \
          install.packages('.', repos = NULL, type = 'source')"

CMD ["R"]
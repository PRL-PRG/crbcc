PACKAGE_NAME := crbcc
PACKAGE_VERSION := $(shell grep "Version:" DESCRIPTION | cut -d' ' -f2)
PACKAGE_TAR := $(PACKAGE_NAME)_$(PACKAGE_VERSION).tar.gz

# R command
R := R

.PHONY: all
all: install

.PHONY: install
install: compile_flags.txt
		$(R) CMD INSTALL .

.PHONY: compile_flags.txt
compile_flags.txt:
	{ echo -xc; $(R) CMD config --cppflags | tr ' ' '\n'; } > $@

.PHONY: clean
clean:
	rm -f $(PACKAGE_NAME)_*.tar.gz
	rm -f src/*.o src/*.so
	rm -f compile_flags.txt

.PHONY: test
test: install
	@for f in tests/*.R; do \
	  echo "===> Running $$f"; \
	  $(R) -f $$f || exit 1; \
	done

# Installs crbcc from this source in bench/pkg-lib. Then compiles each closure
# of each installed package with crbcc and GNU R, and compares the byte code
# (the suite check of inst/benchmarks/crbcc_bench.py). The results are in
# bench/check.csv. The benchmark image build runs this target. It uses the R
# of the PATH, because crbcc_bench.py also uses it.
.PHONY: test-compile-all-packages
test-compile-all-packages:
	mkdir -p bench/pkg-lib
	cd bench && R CMD build --no-build-vignettes .. && \
	  R CMD INSTALL --library=pkg-lib $(PACKAGE_TAR) && \
	  uv run --script ../inst/benchmarks/crbcc_bench.py --include '^check/' \
	    --installed -j "$$(nproc)" --no-progress --csv check.csv

# The benchmark image (inst/benchmarks/docker). The tag is the crbcc version
# and an image revision. Build and push a new image for each crbcc version,
# because the image has the install benchmark results of crbcc. The CI uses
# this tag (make image-tag). The image records the source commit. Without
# git, give it: make image CRBCC_COMMIT=<commit>. Before make image-push, log
# in with docker login ghcr.io.
IMAGE := ghcr.io/prl-prg/crbcc-benchmarks
IMAGE_TAG := 0.2.0-1
CRBCC_COMMIT ?= $(or $(shell git describe --always --dirty --abbrev=40 --exclude='*' 2>/dev/null),unknown)

.PHONY: image
image:
	docker build --platform linux/amd64 --build-arg CRBCC_COMMIT=$(CRBCC_COMMIT) \
	  -f inst/benchmarks/docker/Dockerfile -t $(IMAGE):$(IMAGE_TAG) .

.PHONY: image-push
image-push:
	docker push $(IMAGE):$(IMAGE_TAG)

.PHONY: image-tag
image-tag:
	@echo $(IMAGE_TAG)

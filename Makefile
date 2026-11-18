PACKAGE_NAME := crbcc
PACKAGE_VERSION := $(shell grep "Version:" DESCRIPTION | cut -d' ' -f2)
PACKAGE_TAR := $(PACKAGE_NAME)_$(PACKAGE_VERSION).tar.gz

# R command
R := R

# Build directories
BUILD_DIR := build

C_SOURCES := $(wildcard src/*.c)
R_SOURCES := $(wildcard R/*.R)
TEST_SOURCES := $(wildcard tests/*.R)

# Formatting and tooling
CLANG_FORMAT := clang-format

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

# Installs each package of the benchmark image with crbcc, and compares the
# byte code of each closure with the GNU R byte code in the image (the suite
# check of inst/benchmarks/crbcc_bench.py). Run this target in the image. The
# results are in bench/check.csv.
.PHONY: test-compile-all-packages
test-compile-all-packages:
	@test -n "$$CRBCC_BENCHMARKS" || \
	  { echo "error: run this target in the benchmark image"; exit 1; }
	mkdir -p bench/pkg-lib
	cd bench && $(R) CMD build --no-build-vignettes .. && \
	  $(R) CMD INSTALL --library=pkg-lib $(PACKAGE_TAR) && \
	  uv run --script ../inst/benchmarks/crbcc_bench.py --r "$$(command -v $(R))" \
	    --prebuilt "$$CRBCC_BENCHMARKS" --include '^check/' -j "$$(nproc)" \
	    --packages all --no-progress --csv check.csv

# The benchmark image (inst/benchmarks/docker). The tag is a hash of
# inst/benchmarks/docker and of the hook in HEAD. The CI uses the same tag
# (make image-tag). Thus commit the changes to these files before the
# build. Without git, give the tag, for example make image IMAGE_TAG=dev.
# Before make image-push, log in with docker login ghcr.io.
IMAGE := ghcr.io/prl-prg/crbcc-benchmarks
IMAGE_FILES := inst/benchmarks/docker inst/r-cmd-install-hook.R
IMAGE_TAG ?= $(shell git rev-parse $(addprefix HEAD:,$(IMAGE_FILES)) | git hash-object --stdin | cut -c1-12)

.PHONY: image
image:
ifeq ($(origin IMAGE_TAG),file)
	@test -z "$$(git status --porcelain -- $(IMAGE_FILES))" || \
	  { echo "error: commit the changes to $(IMAGE_FILES) before the build"; exit 1; }
endif
	docker build --platform linux/amd64 -f inst/benchmarks/docker/Dockerfile \
	  -t $(IMAGE):$(IMAGE_TAG) .

.PHONY: image-push
image-push:
	docker push $(IMAGE):$(IMAGE_TAG)

.PHONY: image-tag
image-tag:
	@echo $(IMAGE_TAG)


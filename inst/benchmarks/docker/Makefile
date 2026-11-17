# Builds the benchmark image ghcr.io/prl-prg/crbcc-benchmarks.
#
#   make        build the image
#   make push   push the image (before, docker login ghcr.io)
#   make tag    show the image tag
#
# The tag is a hash of this directory and of inst/r-cmd-install-hook.R in
# HEAD, as in .github/workflows/ci.yml. Thus commit the changes to these
# files before the build. Without git, give the tag, for example
# make TAG=dev.

IMAGE := ghcr.io/prl-prg/crbcc-benchmarks
ROOT  := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../../..)
FILES := inst/benchmarks/docker inst/r-cmd-install-hook.R
TAG   ?= $(shell cd $(ROOT) && git rev-parse $(addprefix HEAD:,$(FILES)) | git hash-object --stdin | cut -c1-12)

.PHONY: build
build:
ifeq ($(origin TAG),file)
	@cd $(ROOT) && test -z "$$(git status --porcelain -- $(FILES))" || \
	  { echo "error: commit the changes to $(FILES) before the build"; exit 1; }
endif
	docker build --platform linux/amd64 -f $(ROOT)/inst/benchmarks/docker/Dockerfile \
	  -t $(IMAGE):$(TAG) $(ROOT)

.PHONY: push
push:
	docker push $(IMAGE):$(TAG)

.PHONY: tag
tag:
	@echo $(TAG)

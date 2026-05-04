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
BEAR := bear

.PHONY: all
all: install

.PHONY: install
install:
		bear -- env CFLAGS="-g -O2 -fno-omit-frame-pointer" $(R) CMD INSTALL .

.PHONY: clean
clean:
	rm -f $(PACKAGE_NAME)_*.tar.gz
	rm -f src/*.o src/*.so
	rm -f compile_commands.json
	rm -rf man

.PHONY: test
test: install
	@for f in tests/*.R; do \
	  echo "===> Running $$f"; \
	  $(R) -f $$f || exit 1; \
	done


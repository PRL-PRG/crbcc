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
	bear -- $(R) CMD INSTALL .

.PHONY: clean
clean:
	rm -f $(PACKAGE_NAME)_*.tar.gz
	rm -f src/*.o src/*.so
	rm -f compile_commands.json
	rm -rf man

.PHONY: format
format:
	@echo "Formatting C source code..."
	@if command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		$(CLANG_FORMAT) -i $(C_SOURCES) && \
		echo "C source files formatted successfully"; \
	else \
		echo "Warning: clang-format not found. Please install clang-format."; \
		exit 1; \
	fi

.PHONY: test
test: $(PACKAGE_TAR)
	@for f in tests/*.R; do \
	  echo "===> Running $$f"; \
	  $(R) -f $$f || exit 1; \
	done


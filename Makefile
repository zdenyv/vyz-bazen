SDK_DIR ?= sdk
VERSION ?= 1.1

CFLAGS += -D'VERSION="${VERSION}"'

-include sdk/Makefile.mk

.PHONY: all
all: debug

.PHONY: sdk
sdk: sdk/Makefile.mk

.PHONY: update
update:
	@git submodule update --remote --merge sdk

sdk/Makefile.mk:
	@git submodule update --init sdk
	@git submodule update --init .vscode

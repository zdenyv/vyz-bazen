SDK_DIR ?= sdk
FW_VERSION ?= 1.2

CFLAGS += -D'FW_VERSION="${FW_VERSION}"'

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

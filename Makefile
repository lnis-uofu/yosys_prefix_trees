# Build and test pptrees yosys plugin

SHELL := bash
BUILD_DIR := build
SRC_DIR := src
PLUGIN_NAME := pptrees

# Set Yosys path to path of yosys executable if it exists
# Otherwise it must be set by the user
YOSYS_PATH ?= $(realpath $(dir $(shell which yosys))/..)

YOSYS_CONFIG ?= $(YOSYS_PATH)/bin/yosys-config
ifeq (,$(wildcard $(YOSYS_CONFIG)))
	YOSYS_CONFIG = $(YOSYS_PATH)/yosys-config
endif
ifeq (,$(wildcard $(YOSYS_CONFIG)))
	$(error "Didn't find 'yosys-config' under '$(YOSYS_PATH)'")
endif

CFLAGS ?= -O0 -Wall -lstdc++ -I$(YOSYS_PATH)
LDFLAGS ?= -Wl,-rpath,$(YOSYS_PATH)

PLUGINS_DIR ?= $(shell $(YOSYS_CONFIG) --datdir)/plugins

.PHONY: default
default: install

.PHONY: install
install: $(PLUGINS_DIR)/$(PLUGIN_NAME).so

$(PLUGINS_DIR)/%.so: $(BUILD_DIR)/%.so
	install -D $< $(PLUGINS_DIR)/

$(BUILD_DIR)/%.so: $(BUILD_DIR)/%.o
	$(YOSYS_CONFIG) --build $@ $< -shared $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	$(YOSYS_CONFIG) --exec --cxx -c --cxxflags -o $@ $< $(CFLAGS)

.PHONY: clean
clean:
	@rm -rf build/*
	@rm -f $(PLUGINS_DIR)/$(PLUGIN_NAME).so

# Build and test pptrees yosys plugin

SHELL := bash
BUILD_DIR := build
SRC_DIR := src

# Set Yosys path to path of yosys executable if it exists
# Otherwise it must be set by the user
YOSYS_PATH ?= $(realpath $(dir $(shell which yosys))/..)

YOSYS_CONFIG ?= $(YOSYS_PATH)/bin/yosys-config

CFLAGS ?= -O0 -Wall -lstdc++ -I$(YOSYS_PATH)
LDFLAGS ?= -Wl,-rpath,$(YOSYS_PATH)

PLUGINS_DIR ?= $(shell $(YOSYS_CONFIG) --datdir)/plugins

.PHONY: default
default: build

.PHONY: build
build: $(BUILD_DIR)/version.o $(BUILD_DIR)/pptrees.so 

$(BUILD_DIR)/%.so: $(BUILD_DIR)/%.o
	$(YOSYS_CONFIG) --build $@ $< -shared $(LDFLAGS)

# Using .git/HEAD as a pre-req to force rebuild when HEAD changes
$(BUILD_DIR)/version.o: util/makeversion.sh .git/HEAD
	@mkdir -p build
	cd build; $(SHELL) ../util/makeversion.sh -o version.o

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	$(YOSYS_CONFIG) --exec --cxx -c --cxxflags -o $@ $< $(CFLAGS)

.PHONY: clean
clean:
	@rm -rf build/*

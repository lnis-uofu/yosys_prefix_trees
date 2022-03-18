# Build and test pptrees yosys plugin

SHELL := bash
BUILD_DIR := build
SRC_DIR := src

YOSYS_CONFIG := yosys-config
CFLAGS ?= -O0 -Wall -lstdc++
LDFLAGS :=  

PLUGINS_DIR ?= $(shell $(YOSYS_CONFIG) --datdir)/plugins

.PHONY: default
default: build

.PHONY: build
build: $(BUILD_DIR)/version.o $(BUILD_DIR)/pptrees.so $(BUILD_DIR)/optadders.so 

.PHONY: install
install: build
	install -D $(BUILD_DIR)/pptrees.so $(PLUGINS_DIR)

$(BUILD_DIR)/%.so: $(BUILD_DIR)/%.o
	$(YOSYS_CONFIG) --build $@ $< -shared $(LDFLAGS)

# Using .git/HEAD as a pre-req to force rebuild when HEAD changes
$(BUILD_DIR)/version.o: util/makeversion.sh .git/HEAD
	@mkdir -p build
	cd build; $(SHELL) ../util/makeversion.sh -o version.o

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	$(YOSYS_CONFIG) --exec --cxx -c --cxxflags -o $@ $< $(CFLAGS)

.PHONY: test
test: $(BUILD_DIR)/version.o $(BUILD_DIR)/pptrees.o
	$(YOSYS_CONFIG) --exec --cxx -o test build/pptrees.o build/version.o $(CFLAGS)
	./test --version

.PHONY: clean
clean:
	@rm -rf build/*

CXX := clang++
CPPFLAGS_RELEASE := -O3 -Wall -Wextra -Wpedantic -std=c++23 -Iinclude
CPPFLAGS_DEBUG := -g -Wall -Wextra -Wpedantic -std=c++23 -Iinclude
LDFLAGS := -Llib -lraylib

ifeq ($(OS),Windows_NT)
		EXCLUDE_PLATFORM := %platform_linux.cpp
else
		EXCLUDE_PLATFORM := %platform_win.cpp
		LDFLAGS += -ldl
endif

SRC := src
OUT := sulla

SRCS_ALL := $(shell find $(SRC) -name "*.cpp")
SRCS := $(filter-out $(EXCLUDE_PLATFORM), $(SRCS_ALL))
OBJS_DEBUG := $(SRCS:%=build/debug/%.o)

ENGINE_SRCS_ALL := $(SRC)/part.cpp $(SRC)/primitives.cpp $(SRC)/utils.cpp $(wildcard $(SRC)/compiler/*.cpp)
ENGINE_SRCS := $(filter-out $(EXCLUDE_PLATFORM), $(ENGINE_SRCS_ALL))
SUITE_CPPFLAGS := -O2 -Wall -Wextra -std=c++23 -Iinclude -I$(SRC)
PERF_CPPFLAGS := -O3 -Wall -Wextra -std=c++23 -Iinclude -I$(SRC)
SUITE_LDFLAGS :=
ifneq ($(OS),Windows_NT)
		SUITE_LDFLAGS += -ldl
endif

.PHONY: debug release check clean build_debug_impl test perf

debug:
	@mkdir -p build/debug
	@rm -f build/make.log
	@echo "Starting Parallel Debug Build..."
	@$(MAKE) --no-print-directory build_debug_impl

build_debug_impl: build/debug/$(OUT)
	@cp *.md build/debug/

build/debug/$(OUT): $(OBJS_DEBUG)
	@echo "Linking $@"
	@$(CXX) $(OBJS_DEBUG) -o $@ $(LDFLAGS) 2>> build/make.log
	@echo "Cleaning intermediate files..."
	@rm -rf build/debug/$(SRC)

build/debug/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@$(CXX) $(CPPFLAGS_DEBUG) -c $< -o $@ 2>> build/make.log

build/debug/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@$(CXX) $(CPPFLAGS_DEBUG) -c $< -o $@ 2>> build/make.log

release:
	@mkdir -p build/release
	@rm -f build/make.log
	@echo "Starting Unity Release Build..."
	@$(CXX) $(CPPFLAGS_RELEASE) $(SRCS) -o build/release/$(OUT) $(LDFLAGS) 2>> build/make.log
	@cp *.md build/release/

check:
	@mkdir -p build/check
	@rm -f build/make.log
	@echo "Starting Unity Debug Check..."
	@$(CXX) $(CPPFLAGS_DEBUG) $(SRCS) -o build/check/$(OUT) $(LDFLAGS) 2>> build/make.log
	@cp *.md build/check/

test:
	@echo "Building validation suite..."
	@$(CXX) $(SUITE_CPPFLAGS) $(ENGINE_SRCS) tests/validate.cpp -o tests/validate $(SUITE_LDFLAGS)
	@echo "Running validation suite (interpreted + native engines)..."
	@cd tests && ./validate

perf:
	@echo "Building performance suite..."
	@$(CXX) $(PERF_CPPFLAGS) $(ENGINE_SRCS) perf/bench.cpp -o perf/bench $(SUITE_LDFLAGS)
	@echo "Running performance suite (interpreted vs native, all modes)..."
	@cd perf && ./bench

clean:
	@rm -rf build
	@rm -f tests/validate perf/bench
	@rm -f tests/parts/*.so tests/parts/*.dll tests/parts/*.cpp
	@rm -f perf/parts/*.so perf/parts/*.dll perf/parts/*.cpp
	@echo "Cleaned build directory"

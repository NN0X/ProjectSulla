CXX := clang++
CPPFLAGS_RELEASE := -O3 -Wall -Wextra -Wpedantic -std=c++17 -Iinclude
CPPFLAGS_DEBUG := -g -Wall -Wextra -Wpedantic -std=c++17 -Iinclude
LDFLAGS := 
SRC := src
OUT := losim

SRCS := $(shell find $(SRC) -name "*.cpp")
OBJS_DEBUG := $(SRCS:%=build/debug/%.o)

.PHONY: debug release check clean build_debug_impl

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

clean:
	@rm -rf build
	@echo "Cleaned build directory"

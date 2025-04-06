DBG_DIR = out/debug
REL_DIR = out/release
BUILD_DIR = out
EXECUTABLE = Husky
SHADERS_DIR = ./shaders
SHADERS_OUT_DIR = out/shaders

.PHONY: all build run clean configure shaders

all: shaders build

configure:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
	cd $(BUILD_DIR) && mv compile_commands.json ../compile_commands.json

shaders:
	@mkdir -p $(SHADERS_OUT_DIR)
	$(foreach file, $(wildcard $(SHADERS_DIR)/*), \
		glslc $(file) -o $(SHADERS_OUT_DIR)/$(notdir $(file)).spv \
	)

build:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && $(MAKE)

run: build
	$(BUILD_DIR)/$(EXECUTABLE)

clean:
	rm compile_commands.json
	rm -rf $(BUILD_DIR)

release:
	@mkdir -p $(REL_DIR)
	cmake -S . -B $(REL_DIR) -DCMAKE_BUILD_TYPE=Release
	cd $(REL_DIR) && $(MAKE)
	$(REL_DIR)/$(EXECUTABLE)

debug:
	@mkdir -p $(DBG_DIR)
	cmake -S . -B $(DBG_DIR) -DCMAKE_BUILD_TYPE=Debug
	cd $(DBG_DIR) && $(MAKE)
	$(DBG_DIR)/$(EXECUTABLE)

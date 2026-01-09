.PHONY: all run clean

GLSLC := glslc

PROJECT_NAME ?= civ-game
ROOT_DIR := $(shell pwd)
PROJECT_BUILD_PATH ?= out
SHADER_SRC_DIR = shaders
SHADER_OUT_DIR = $(PROJECT_BUILD_PATH)/shaders

SHADER_EXTS := vert frag comp

SHADERS := $(foreach ext,$(SHADER_EXTS), \
    $(shell find $(SHADER_SRC_DIR) -type f -name "*.$(ext)"))

SPV := $(patsubst $(SHADER_SRC_DIR)/%,$(SHADER_OUT_DIR)/%.spv,$(SHADERS))

all: shaders
	cmake -S . -B $(PROJECT_BUILD_PATH) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	ln -sf $(PROJECT_BUILD_PATH)/compile_commands.json .
	ln -sf $(ROOT_DIR)/assets $(ROOT_DIR)/$(PROJECT_BUILD_PATH)
	cmake --build $(PROJECT_BUILD_PATH)

shaders: $(SPV)

$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/%
	@mkdir -p $(dir $@)
	$(GLSLC) $< -o $@

run: all
	cd $(PROJECT_BUILD_PATH) && ./$(PROJECT_NAME)$(EXT)

clean:
	rm -rf $(PROJECT_BUILD_PATH)

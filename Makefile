BUILD_DIR = out
DBG_DIR = out/debug
REL_DIR = out/release

ifeq ($(OS),Windows_NT)
	BUILD_CMD = msbuild Husky.sln
	EXECUTABLE = Husky.exe
else
	BUILD_CMD = make
	EXECUTABLE = Husky
endif

.PHONY: all build run clean configure shaders

all: shaders build

configure:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
	cd $(BUILD_DIR) && mv compile_commands.json ../compile_commands.json

clean:
	rm compile_commands.json
	rm -rf $(BUILD_DIR)

release:
	@mkdir -p $(REL_DIR)
	cmake -S . -B $(REL_DIR) -DCMAKE_BUILD_TYPE=Release
	cd $(REL_DIR) && $(BUILD_CMD)
	$(REL_DIR)/$(EXECUTABLE)

debug:
	@mkdir -p $(DBG_DIR)
	cmake -S . -B $(DBG_DIR) -DCMAKE_BUILD_TYPE=Debug
	cd $(DBG_DIR) && $(BUILD_CMD)
	$(DBG_DIR)/$(EXECUTABLE)

windows:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release 
	cd $(BUILD_DIR) && $(BUILD_CMD)
	cp SDL3.dll $(BUILD_DIR)\Debug
	cd $(BUILD_DIR)\Debug && $(EXECUTABLE)

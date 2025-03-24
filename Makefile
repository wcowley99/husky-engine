BUILD_DIR = out
EXECUTABLE = Husky

.PHONY: all build run clean configure

all: build

configure:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
	cd $(BUILD_DIR) && mv compile_commands.json ../compile_commands.json

build:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && $(MAKE)

run: build
	$(BUILD_DIR)/$(EXECUTABLE)

clean:
	rm compile_commands.json
	rm -rf $(BUILD_DIR)

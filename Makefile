.PHONY: all run clean

PROJECT_NAME ?= civ-game

PROJECT_BUILD_PATH ?= out

all:
	cmake -S . -B $(PROJECT_BUILD_PATH) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	ln -sf $(PROJECT_BUILD_PATH)/compile_commands.json .
	cmake --build $(PROJECT_BUILD_PATH)

run: all
	./$(PROJECT_BUILD_PATH)/$(PROJECT_NAME)$(EXT)

clean:
	rm -rf $(PROJECT_BUILD_PATH)

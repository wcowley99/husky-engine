OUT = out
TST_NAME = test
APP_NAME = civ-game

ifeq ($(OS), Windows_NT)
	MDOUT := if not exist $(OUT) mkdir $(OUT)
	TEST := $(TST_NAME).exe
	EXE := $(APP_NAME).exe
else 
	MDOUT := mkdir -p $(OUT)
	TEST := $(TST_NAME)
	EXE := $(APP_NAME)
endif

run: build
	cd $(OUT)/Debug && $(EXE)

test: build
	cd $(OUT)/Debug && $(TEST)

build: configure
	cd $(OUT) && cmake --build .
	cd $(OUT) && cp SDL/Release/* Debug

configure:
	$(MDOUT)
	cd $(OUT) && cmake ..


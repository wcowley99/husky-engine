OUT = out
SRC = src
EXE = mordor

CC = gcc
INCLUDE = -Iexternal/SDL/include -Iexternal/volk
CFLAGS := $(INCLUDE) -Wall

HEADERS := $(wildcard *.h) $(wildcard $(SRC)/*.h)
SOURCES := $(wildcard *.c) $(wildcard $(SRC)/*.c)

OBJS := $(patsubst src/%.c, out/%.o, $(SOURCES))
LIBS = -Lout -lSDL3 -Wl,-rpath=. -lvulkan

.PHONY: default deps build clean run

default: run

deps:
	# Build SDL
	cmake -S external/SDL -B out/SDL
	cmake --build out/SDL
	cp out/SDL/libSDL3.so out/

build: $(OUT)/$(EXE)

run: $(OUT)/$(EXE)
	cd $(OUT) && ./$(EXE)

valgrind: $(OUT)/$(EXE)
	cd $(OUT) && valgrind --leak-check=yes ./$(EXE)

clean:
	rm -rf out

$(OUT)/%.o: $(SRC)/%.c $(HEADERS)
	@mkdir -p $(@D)
	$(CC) -g $(CFLAGS) -c $< -o $@

$(OUT)/$(EXE): $(OBJS) deps
	$(CC) $(OBJS) -g $(CFLAGS) $(LIBS) -o $@

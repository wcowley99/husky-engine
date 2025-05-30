run: build
	cd out/Debug && civ-game.exe

test: build
	cd out/Debug && test.exe

build: configure
	cd out && cmake --build .
	cd out && cp SDL/Release/* Debug

configure:
	if not exist out mkdir out
	cd out && cmake ..


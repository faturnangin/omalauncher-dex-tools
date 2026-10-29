CC ?= gcc
CFLAGS ?= -O2 -Wall

all: build/libheidisql-fix.so build/libfirefox-fix.so build/omarchy-dex-menu

build/libheidisql-fix.so: src/libheidisql-fix.c
	mkdir -p build
	$(CC) $(CFLAGS) -shared -fPIC src/libheidisql-fix.c -o build/libheidisql-fix.so -lwayland-client -ldl -lpthread

build/libfirefox-fix.so: src/libfirefox-fix.c
	mkdir -p build
	$(CC) $(CFLAGS) -shared -fPIC src/libfirefox-fix.c -o build/libfirefox-fix.so -lwayland-client -ldl

build/omarchy-dex-menu: src/omarchy-dex-menu.c
	mkdir -p build
	$(CC) $(CFLAGS) src/omarchy-dex-menu.c -o build/omarchy-dex-menu $$(pkg-config --cflags --libs gtk+-3.0)

clean:
	rm -rf build

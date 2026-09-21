# MWCheats - 32-bit ASI plugin
#
#   mingw32-make                     build build/MWCheats.asi
#   mingw32-make clean
#   mingw32-make install GAME="C:/path/to/NFSMW"
#
# Requires a 32-bit MinGW-w64 g++ (i686-w64-mingw32) with C++20 support.
# Structured exception handling (__try/__except) is MSVC-only; the MinGW
# build falls back to VirtualQuery-based pointer validation.

CXX      ?= g++
WINDRES  ?= windres
TARGET   := build/MWCheats.asi
RESOURCE := build/UI.res.o
SOURCES  := src/dllmain.cpp $(wildcard src/NFSMW/*.cpp) $(wildcard src/UI/Animation/*.cpp)
HEADERS  := src/dllmain.hpp $(wildcard src/NFSMW/*.hpp) $(wildcard src/UI/Animation/*.hpp)

CXXFLAGS := -m32 -std=c++20 -O2 -Wall -Wextra -fno-strict-aliasing \
            -fno-exceptions -fno-rtti -DNOMINMAX -DWIN32_LEAN_AND_MEAN
LDFLAGS  := -m32 -shared -static -static-libgcc -static-libstdc++ \
            -Wl,--exclude-all-symbols -Wl,-u,___mingw_SEH_error_handler -ladvapi32 -lgdi32 -lwinmm

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS) $(RESOURCE)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(SOURCES) $(RESOURCE) $(LDFLAGS) -o $@

$(RESOURCE): src/UI/UI.rc src/UI/Audio/achievement.wav
	@mkdir -p build
	$(WINDRES) -F pe-i386 -O coff -o $@ src/UI/UI.rc

clean:
	@rm -rf build

install: $(TARGET)
ifndef GAME
	$(error Set GAME to your Most Wanted folder, e.g. mingw32-make install GAME="C:/Games/NFSMW")
endif
	@mkdir -p "$(GAME)/scripts"
	cp $(TARGET) "$(GAME)/scripts/"
	cp MWCheats.ini "$(GAME)/scripts/"
	@echo "Installed to $(GAME)/scripts"

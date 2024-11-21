# A simple Makefile for compiling small SDL projects

# set the compiler
#CC := g++
CC := llvm-g++

## macOS
SDL_CONF := `/opt/homebrew/bin/sdl2-config --cflags --libs`

# Linux
#SDL_CONF := `sdl2-config --cflags --libs`
# set the compiler flags
CFLAGS := $(SDL_CONF) -ggdb2 -O0 -lSDL2_image -lSDL2_mixer -lm -std=c++17
# -fmax-errors=3
# -ferror-limit=2

# add header files here
HDRS := global.h state.h types.h mouse.h driver.h scene.h game.h bitmap.h car.h part.h carbitmap.h cardata.h driving.h racing.h spritesheet.h SFont.h assets.h ui/button.h ui/zone.h parsers/libp.h \
		parsers/carcolor.h ui/bubble.h include/toml.hpp roster.h

# add source files here
SRCS := main.cpp state.cpp types.cpp SFont.c bitmap.cpp spritesheet.cpp scene.cpp car.cpp part.cpp cardata.cpp carbitmap.cpp diner.cpp driving.cpp racing.cpp garage.cpp gas_station.cpp assets.cpp ui/button.cpp \
		ui/bubble.cpp parsers/libp.cpp parsers/carcolor.cpp roster.cpp

# generate names of object files
OBJS := $(SRCS:.c=.o)

# name of executable
EXEC := main

# default recipe
all: $(EXEC)

# recipe for building the final executable
$(EXEC): $(OBJS) $(HDRS) Makefile
	$(CC) -o $@ $(OBJS) $(CFLAGS)

# new target for generating dependency tree
tree: CFLAGS += -H
tree: $(EXEC)

# recipe to clean the workspace
clean:
	find . -name '*.o' -exec rm -rf {} \;
	find . -name '*.dSYM' -exec rm -rf {} \;

.PHONY: all clean tree

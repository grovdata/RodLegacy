# A simple Makefile for compiling small SDL projects

# set the compiler
#CC := g++
CC := llvm-g++

# set the compiler flags
CFLAGS := `sdl2-config --cflags --libs` -ggdb3 -O0 -Wall -lSDL2_image -lSDL2_mixer -lm -std=c++17

# add header files here
HDRS := global.h state.h mouse.h driver.h scene.h garage.h game.h car.h spritesheet.h SFont.h ui/button.h ui/zone.h parsers/libp.h \
		include/libamivideo/amivideotypes.h include/libamivideo/viewportmode.h include/libamivideo/palette.h include/libamivideo/screen.h

# add source files here
SRCS := main.cpp state.cpp SFont.c spritesheet.cpp scene.cpp car.cpp diner.cpp ui/button.cpp \
		include/libamivideo/viewportmode.c include/libamivideo/palette.c include/libamivideo/screen.c

# generate names of object files
OBJS := $(SRCS:.c=.o)

# name of executable
EXEC := main

# default recipe
all: $(EXEC)

# recipe for building the final executable
$(EXEC): $(OBJS) $(HDRS) Makefile
	$(CC) -o $@ $(OBJS) $(CFLAGS)

# recipe to clean the workspace
clean:
	find . -name '*.o' -exec rm -rf {} \;
	find . -name '*.dSYM' -exec rm -rf {} \;

.PHONY: all clean

# A simple Makefile for compiling small SDL projects

# set the compiler
CC := clang++

# set the compiler flags
CFLAGS := `sdl2-config --cflags --libs` -ggdb3 -O0 -Wall -lSDL2_image -lSDL2_mixer -lm -std=c++17

# add header files here
HDRS := global.hpp mouse.hpp driver.hpp SFont.h ui/button.hpp

# add source files here
SRCS := main.cpp SFont.c ui/button.cpp

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
	rm -rf *.o *.dSYM

.PHONY: all clean

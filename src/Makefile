# A simple Makefile for compiling small SDL projects

# set the compiler
CC := clang++

# set the compiler flags
CFLAGS := `sdl2-config --cflags --libs` -ggdb3 -O0 -Wall -lSDL2_image -lSDL2_mixer -lm

# add header files here
HDRS := global.hpp mouse.hpp driver.hpp SFont.h

# add source files here
SRCS := main.cpp SFont.c

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

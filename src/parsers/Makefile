CC := g++
SDL_CONF := `sdl2-config --cflags --libs`
CFLAGS := $(SDL_CONF) -ggdb3 -O0 -Wall -lSDL2_image -lSDL2_mixer -lm -std=c++11


HDRS := libp.h
SRCS := libp.cpp

# generate names of object files
OBJS := $(SRCS:.c=.o)

# name of executable
EXEC := libp

# default recipe
all: $(EXEC)

# recipe for building the final executable
$(EXEC): $(OBJS) $(HDRS) Makefile
	$(CC) -o $@ $(OBJS) $(CFLAGS)

# recipe to clean the workspace
clean:
	find . -name '*.o' -exec rm -rf {} \;
	find . -name '*.dSYM' -exec rm -rf {} \;
	rm $(EXEC)

.PHONY: all clean

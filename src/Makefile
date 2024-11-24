# A simple Makefile for compiling small SDL projects

# set the compiler
CC := llvm-g++

## macOS
SDL_CONF := `/opt/homebrew/bin/sdl2-config --cflags --libs`

# ImGui configuration - use absolute paths
IMGUI_DIR = include/imgui
IMGUI_SRCS = $(IMGUI_DIR)/imgui.cpp \
             $(IMGUI_DIR)/imgui_demo.cpp \
             $(IMGUI_DIR)/imgui_draw.cpp \
             $(IMGUI_DIR)/imgui_tables.cpp \
             $(IMGUI_DIR)/imgui_widgets.cpp \
             $(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp \
             $(IMGUI_DIR)/backends/imgui_impl_sdlrenderer2.cpp

# Generate ImGui object files with explicit paths
IMGUI_OBJS = $(IMGUI_SRCS:.cpp=.o)

# Game source files (excluding ImGui)
SRCS := main.cpp state.cpp types.cpp SFont.c bitmap.cpp spritesheet.cpp scene.cpp car.cpp \
        part.cpp cardata.cpp carbitmap.cpp diner.cpp driving.cpp racing.cpp garage.cpp \
        gas_station.cpp assets.cpp ui/button.cpp ui/bubble.cpp parsers/libp.cpp \
        parsers/carcolor.cpp roster.cpp debug_overlay.cpp

# Generate object files for game sources
OBJS = $(SRCS:.c=.o)

# compiler flags
CXXFLAGS = -std=c++17 $(SDL_CONF) -ggdb2 -O0 -lSDL2_image -lSDL2_mixer -lm \
           -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -DIMGUI_IMPL_API=

# name of executable
EXEC := main

# default recipe
all: $(EXEC)

# recipe for building the final executable
$(EXEC): $(OBJS) $(IMGUI_OBJS)
	$(CC) -o $@ $^ $(CXXFLAGS)

# Compile game cpp files
%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -o $@ $<

# Compile game c files
%.o: %.c
	$(CC) $(CXXFLAGS) -c -o $@ $<

# recipe to clean the workspace
clean:
	find . -name '*.o' -exec rm -rf {} \;
	find . -name '*.dSYM' -exec rm -rf {} \;
	rm -f $(EXEC)

.PHONY: all clean



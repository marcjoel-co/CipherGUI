# CipherGUI/Makefile

# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++17 -g -Wall # Use c++17 or newer

# OpenSSL paths (adjust if your Homebrew installed a different version, e.g., openssl@1.1)
OPENSSL_INCLUDE_DIR = /usr/local/opt/openssl@3/include
OPENSSL_LIB_DIR = /usr/local/opt/openssl@3/lib

# Include paths for your project's headers, ImGui headers, GLFW, and OpenSSL
INCLUDES = -I./src \
           -I./lib/imgui \
           -I./lib/imgui/backends \
           -I/usr/local/include \
           -I$(OPENSSL_INCLUDE_DIR) # Added OpenSSL include path
      
# Libraries to link
# GLFW (from Homebrew), OpenSSL, and system frameworks for OpenGL, Cocoa, etc. on macOS
LIBS = -L/usr/local/lib \
       -L$(OPENSSL_LIB_DIR) \
       -lglfw \
       -lcrypto \
       -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

# Source files
# Your application sources
APP_SOURCES = src/main.cpp src/cipher_utils.cpp src/jay_gui.cpp
# ImGui sources
IMGUI_SOURCES = lib/imgui/imgui.cpp \
                lib/imgui/imgui_draw.cpp \
                lib/imgui/imgui_tables.cpp \
                lib/imgui/imgui_widgets.cpp \
                lib/imgui/imgui_stdlib.cpp \
                lib/imgui/backends/imgui_impl_glfw.cpp \
                lib/imgui/backends/imgui_impl_opengl3.cpp\
                # lib/imgui/imgui_demo.cpp # Usually not needed for final app

# Combine all C++ sources
SOURCES = $(APP_SOURCES) $(IMGUI_SOURCES)

# Object files: derive from source files by replacing .cpp with .o
OBJS = $(SOURCES:.cpp=.o)

# Executable name
TARGET = cipher_gui

# Default rule: build the executable
all: $(TARGET)

# Rule to link the executable
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

# Rule to compile .cpp files into .o files
# This uses automatic variables: $< (the prerequisite, i.e., the .cpp file) and $@ (the target, i.e., the .o file)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean rule: remove object files and the executable
clean:
	rm -f $(OBJS) $(TARGET) imgui.ini # Also remove imgui.ini if desired

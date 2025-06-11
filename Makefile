#
# Makefile for DiaryManager on Windows with MinGW/g++
#

# --- Configuration ---
# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -g -Wall
                     #g for additional Debug statement

# Directories
SRC_DIR = src
IMGUI_DIR = lib/imgui
GLFW_DIR = lib/glfw

# --- Paths and Libraries ---
# Include paths for the compiler (-I)
INCLUDES = -I$(SRC_DIR) \
           -I$(IMGUI_DIR) \
           -I$(IMGUI_DIR)/backends \
           -I$(GLFW_DIR)/include

# Library search paths for the linker (-L)
LDFLAGS = -L$(GLFW_DIR)/lib-mingw-w64

# Libraries to link (-l)
# We removed -lssl and -lcrypto as they are not needed for the Diary app.
LIBS = -lglfw3 -lgdi32 -lopengl32 -limm32 -lshell32

# --- Source Files ---

SRCS =V# --- Source Files ---

SRCS = src/main.cpp \
       src/ui_manager.cpp \
       src/dairy_manager.cpp \
       src/external_editor.cpp\
       lib/imgui/imgui.cpp \
       lib/imgui/imgui_demo.cpp \
       lib/imgui/imgui_draw.cpp \
       lib/imgui/imgui_stdlib.cpp \
       lib/imgui/imgui_tables.cpp \
       lib/imgui/imgui_widgets.cpp \
       lib/imgui/backends/imgui_impl_glfw.cpp \
       lib/imgui/backends/imgui_impl_opengl3.cpp

# This line automatically creates the .o list from the .cpp list. 
OBJS = $(SRCS:.cpp=.o)

# The final executable name is updated to match the project theme.
TARGET = DiaryManager.exe

# --- Build Rules ---
all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking executable: $(TARGET)"
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS) $(LIBS)
	@echo "Build complete. Run with: ./$(TARGET)"

# This generic rule compiles any .cpp file into a .o file. It's perfect.
%.o: %.cpp
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "Cleaning project..."
	rm -f $(OBJS) $(TARGET)
	@echo "Clean complete."

run:

# Tells 'make' that 'all' and 'clean' are not actual files.
.PHONY: all clean
/**
 *  @brief: THe main.cpp, sets up the 
 */
// ImGui Dependecies
#include "imgui.h"                  // The main ImGui library for drawing widgets.
#include "imgui_impl_glfw.h"        // Bridge GLFW.
#include "imgui_impl_opengl3.h"     // Bridge OpenGL (the graphics API).

// This is the windowing library.
#include <GLFW/glfw3.h>
 
#include <iostream> //for basic i/o operatiobs


//Srcs.
#include "diary_manager.h" // Manages the actual diary data
// ^ here is where we're gonna focus on the croods 

#include "ui_manager.h"  
#include "external_editor.h"// Opens Notepad, and logic for creating temp.txt

// sets up errors
void glfw_error_callback(int error, const char* description) {
   
    std::cerr << "GLFW Error " << error << ": " << description << '\n';
}

// Main
int main()
{  
    
    // see a clean error message using our erros set up
    glfwSetErrorCallback(glfw_error_callback);

    // Initialize the GLFW library. we can't create a window.
    if (!glfwInit()) {
        return 1; // Return 1  failed to start.
    }

  
    //  version of OpenGL (3.3).
    const char* glsl_version = "#version 130";
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); 
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); 
   
    // Parameters: width, height, "Title in the title bar", and two nullptrs for advanced stuff.
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Diary Manager", nullptr, nullptr);
    if (window == nullptr) {
        // If the window wasn't created, something is wrong. We must exit.
        return 1;
    }
    
    // Focus on the window
    glfwMakeContextCurrent(window);
    // Enable VSync. This caps the framerate to your monitor's refresh rate,
    // which prevents screen tearing and saves CPU/GPU power.
    glfwSwapInterval(1); 

    
   

    // Standard ImGui setup boilerplate. This gets ImGui ready to be used.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io; // Get a reference to the ImGui Input/Output object.
    
    // Set the visual theme. 
    ImGui::StyleColorsDark();

    //This connects ImGui to our window (GLFW) and our renderer (OpenGL).
    // Now ImGui knows how to receive mouse clicks from GLFW and how to draw using OpenGL.
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
   
    DiaryManager myDiary;

    // THE MAIN LOOP (The "Game Loop") ---
    while (!glfwWindowShouldClose(window)) {
        // Handle Input
        glfwPollEvents();

        // Start a New Drawing: 
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // We pass it `myDiary` so the UI can access the list of entries and other data.
        ui_manager::draw_ui(myDiary);

        
        ImGui::Render(); 
        
        // Prepare the window for drawing
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Tell the OpenGL backend to execute the drawing commands and put them on the screen.
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
       
        // Swap the back buffer (where we were drawing) with the front buffer (what the user sees).
        
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0; 
}
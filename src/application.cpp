#include "application.h"

#include <iostream>  // For std::cerr
#include <algorithm> // For std::max
#include <cmath>     // For std::abs

// Include the necessary implementation headers for backend libraries
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Platform-specific OpenGL header for Apple
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#endif

// Anonymous namespace for file-local helper functions
namespace {
    // A C++-style error callback for GLFW
    void glfw_error_callback(int error, const char* description) {
        std::cerr << "GLFW Error " << error << ": " << description << '\n';
    }
}

// --- Constructor / Destructor ---

// Definitions match the 'noexcept' in the header
Application::Application() noexcept
    : window(nullptr),
      glsl_version(nullptr),
      last_calculated_os_window_size{0.0f, 0.0f}
{}

Application::~Application() noexcept {
    // The shutdown() method is called at the end of run(), so the destructor can be empty.
    // This design ensures cleanup happens deterministically when run() finishes.
}

// --- Public Methods ---

int Application::run() {
    if (!initialize()) {
        std::cerr << "Fatal: Failed to initialize application.\n";
        // Ensure cleanup is attempted even if initialization fails partway through
        shutdown();
        return 1;
    }

    main_loop();
    shutdown();
    return 0;
}

// --- Private Methods ---

bool Application::initialize() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return false;
    }

    // --- Set OpenGL/GLSL versions based on the platform ---
#if defined(__APPLE__)
    glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    
    // Window is programmatically resized, so user resizing is disabled.
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // --- Create Window and OpenGL Context ---
    window = glfwCreateWindow(800, 600, APP_TITLE, nullptr, nullptr);
    if (!window) {
        return false;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable V-Sync

    // --- Initialize ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return true;
}

void Application::main_loop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Delegate all UI drawing to the UIManager.
        // It returns the content size and chrome height for dynamic window resizing.
        auto [content_size, chrome_height] = ui_manager.draw_ui(window);

        // Dynamically resize the OS window based on the UI's needs, but not if a modal is open.
        if (!ui_manager.is_modal_active()) {
            ImVec2 desired_os_window_size = {
                std::max(content_size.x + ImGui::GetStyle().WindowPadding.x * 2.0f, 350.0f),
                std::max(content_size.y + chrome_height, 250.0f)
            };
            
            // Check if the desired size has changed significantly to prevent jitter
            if (std::abs(desired_os_window_size.x - last_calculated_os_window_size.x) > 2.0f ||
                std::abs(desired_os_window_size.y - last_calculated_os_window_size.y) > 2.0f) {
                
                if (desired_os_window_size.x > 0 && desired_os_window_size.y > 0) {
                    glfwSetWindowSize(window, static_cast<int>(desired_os_window_size.x), static_cast<int>(desired_os_window_size.y));
                    last_calculated_os_window_size = desired_os_window_size;
                }
            }
        }

        // Render the frame
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(CLEAR_COLOR.x, CLEAR_COLOR.y, CLEAR_COLOR.z, CLEAR_COLOR.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
}

void Application::shutdown() {
    // Clean up ImGui, GLFW, and the window context in reverse order of initialization
    if (window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(window);
        window = nullptr; // Set to null after destruction
    }
    glfwTerminate();
}
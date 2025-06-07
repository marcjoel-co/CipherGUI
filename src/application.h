#pragma once

#include "ui_manager.h" // The Application class owns and manages the UI

// Forward-declare GLFWwindow to avoid including the heavyweight GLFW header
struct GLFWwindow;

class Application {
public:
    // Mark constructor/destructor as noexcept as they don't throw exceptions
    Application() noexcept;
    ~Application() noexcept;

    // The main entry point for the application loop. Returns an exit code.
    int run();

    // Disable copy and move operations to ensure single ownership
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

private:
    // --- Private Methods for Application Lifecycle ---
    bool initialize();
    void main_loop();
    void shutdown();

    // --- Member Variables ---
    GLFWwindow* window;
    UIManager ui_manager;
    const char* glsl_version;
    ImVec2 last_calculated_os_window_size;

    // --- Application Constants (using modern inline constexpr) ---
    inline static constexpr const char* APP_TITLE = "Cipher GUI";
    inline static constexpr ImVec4 CLEAR_COLOR = {0.1f, 0.1f, 0.12f, 1.0f};
};
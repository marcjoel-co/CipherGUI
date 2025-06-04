/**
 * @file jay_gui.cpp
 * @brief Implements the main GUI application logic using Dear ImGui and GLFW.
 */

#define GL_SILENCE_DEPRECATION // To silence OpenGL deprecation warnings on macOS

#include "jay_gui.h"      // Assuming this declares int run_gui_application();
#include "cipher_utils.h" // For cipher functions, constants, and validation

// Dear ImGui Headers
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Standard Headers
#include <cstdio>   
#include <string>   // For std::string
#include <vector>   // Potentially for storing history lines
#include <sstream>  // For std::ostringstream to capture validation errors
#include <fstream>  // For std::ifstream to load history
#include <algorithm> // For std::clamp, std::min, std::max

// GLFW (Graphics Library Framework) - for windowing and input
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
// For other platforms, GLFW will include the necessary OpenGL headers
#endif
#include <GLFW/glfw3.h> // Must be after OpenGL headers if not using glad/glew

// --- Global GUI State ---
namespace { // Anonymous namespace for file-local statics

void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

enum class AppScreen {
    MainMenu,
    EncryptFiles,
    DecryptFiles,
    GetItem,        // Placeholder
    CompareFiles,   // Placeholder
    History
};
AppScreen current_screen = AppScreen::MainMenu;

// Buffers for ImGui inputs.
char input_file_path_buf[cipher_utils::MAX_FILENAME_BUFFER_SIZE] = "";
char output_file_path_buf[cipher_utils::MAX_FILENAME_BUFFER_SIZE] = "";
int pegs_value = 7; // Default peg value

// Buffer for GUI messages
std::string gui_message = "";
ImVec4 gui_message_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white

// Buffer for history content
std::string history_content_buf = "";

void set_gui_message(const std::string& message, bool is_error) {
    gui_message = message;
    if (is_error) {
        gui_message_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
    } else {
        gui_message_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
    }
}

class CerrRedirect {
public:
    CerrRedirect(std::streambuf* new_buffer) : old_cerr_buf(std::cerr.rdbuf(new_buffer)) {}
    ~CerrRedirect() { std::cerr.rdbuf(old_cerr_buf); }
private:
    std::streambuf* old_cerr_buf;
};

void load_history_content() {
    std::ifstream history_file_stream(cipher_utils::HISTORY_FILE);
    if (history_file_stream) {
        std::stringstream ss;
        ss << history_file_stream.rdbuf();
        history_content_buf = ss.str();
    } else {
        history_content_buf = "Could not open history file: " + cipher_utils::HISTORY_FILE;
    }
}

} // end anonymous namespace


int run_gui_application() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Initial OS window size - this will be adjusted dynamically
    GLFWwindow* window = glfwCreateWindow(300, 400, "Cipher GUI", nullptr, nullptr);
    if (window == nullptr) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImVec2 last_os_window_size = ImVec2(0,0); // Store the last size OS window was set to

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiWindowFlags panel_window_flags = ImGuiWindowFlags_MenuBar |
                                              ImGuiWindowFlags_AlwaysAutoResize |
                                              ImGuiWindowFlags_NoSavedSettings;

        // Ensure ImGui panel is at (0,0) within the OS window client area
        ImGui::SetNextWindowPos(ImVec2(0, 0));

        // Set minimum size constraints for the ImGui panel
        // These values should be enough to display your main menu comfortably.
        float min_panel_width = 280.0f;  // Adjust based on your widest main menu button
        float min_panel_height = 380.0f; // Adjust based on total height of main menu elements
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(min_panel_width, min_panel_height), // Minimum size
            ImVec2(FLT_MAX, FLT_MAX)                   // Maximum size (effectively no upper limit)
        );

        ImGui::Begin("Cipher Application Panel", nullptr, panel_window_flags);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) { glfwSetWindowShouldClose(window, GLFW_TRUE); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Navigation")) {
                if (ImGui::MenuItem("Main Menu")) { current_screen = AppScreen::MainMenu; gui_message.clear(); }
                if (ImGui::MenuItem("Encrypt Files")) { current_screen = AppScreen::EncryptFiles; gui_message.clear(); }
                if (ImGui::MenuItem("Decrypt Files")) { current_screen = AppScreen::DecryptFiles; gui_message.clear(); }
                if (ImGui::MenuItem("View History")) {
                    current_screen = AppScreen::History;
                    gui_message.clear();
                    load_history_content();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::Spacing();

        if (!gui_message.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, gui_message_color);
            ImGui::TextWrapped("%s", gui_message.c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
        }

        float available_width = ImGui::GetContentRegionAvail().x;
        // For main menu, buttons might need to be a bit wider if panel is narrow
        float main_menu_button_width = std::max(min_panel_width * 0.8f, available_width * 0.9f); // Try to use more space
        main_menu_button_width = std::clamp(main_menu_button_width, 150.0f, 400.0f);


        switch (current_screen) {
            case AppScreen::MainMenu:
                ImGui::Text("Main Menu");
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 10.0f));
                if (ImGui::Button("Encrypt Files", ImVec2(main_menu_button_width, 40))) { current_screen = AppScreen::EncryptFiles; gui_message.clear(); }
                ImGui::Dummy(ImVec2(0, 5.0f));
                if (ImGui::Button("Decrypt Files", ImVec2(main_menu_button_width, 40))) { current_screen = AppScreen::DecryptFiles; gui_message.clear(); }
                ImGui::Dummy(ImVec2(0, 5.0f));
                if (ImGui::Button("Get Item (placeholder)", ImVec2(main_menu_button_width, 40))) { current_screen = AppScreen::GetItem; gui_message.clear(); }
                ImGui::Dummy(ImVec2(0, 5.0f));
                if (ImGui::Button("Compare Files (placeholder)", ImVec2(main_menu_button_width, 40))) { current_screen = AppScreen::CompareFiles; gui_message.clear(); }
                ImGui::Dummy(ImVec2(0, 5.0f));
                if (ImGui::Button("View History", ImVec2(main_menu_button_width, 40))) {
                    current_screen = AppScreen::History;
                    gui_message.clear();
                    load_history_content();
                }
                break;

            case AppScreen::EncryptFiles:
            case AppScreen::DecryptFiles: {
                ImGui::Text(current_screen == AppScreen::EncryptFiles ? "Encrypt Files" : "Decrypt Files");
                ImGui::Separator();

                ImGui::PushItemWidth(std::max(min_panel_width - 80.0f, available_width - 60.0f));
                ImGui::InputText("Input File Path", input_file_path_buf, cipher_utils::MAX_FILENAME_BUFFER_SIZE);
                ImGui::InputText("Output File Path", output_file_path_buf, cipher_utils::MAX_FILENAME_BUFFER_SIZE);
                ImGui::InputInt("Pegs", &pegs_value);
                ImGui::PopItemWidth();
                pegs_value = std::clamp(pegs_value, cipher_utils::MIN_PEG, cipher_utils::MAX_PEG);

                float action_button_width = std::max(80.0f, (available_width - ImGui::GetStyle().ItemSpacing.x - 20.0f) / 2.0f); // Ensure some padding
                if (ImGui::Button(current_screen == AppScreen::EncryptFiles ? "Encrypt" : "Decrypt", ImVec2(action_button_width, 0))) {
                    gui_message.clear();
                    std::ostringstream validation_errors_stream;
                    CerrRedirect redirect_cerr_to_string(validation_errors_stream.rdbuf());

                    cipher_utils::OperationParams params;
                    params.input_file = input_file_path_buf;
                    params.output_file = output_file_path_buf;
                    params.pegs = pegs_value;

                    bool valid = false;
                    bool success = false;

                    if (current_screen == AppScreen::EncryptFiles) {
                        if (cipher_utils::validate_encryption_params(params)) {
                            valid = true;
                            success = cipher_utils::encrypt_file(params.input_file, params.output_file, params.pegs);
                        }
                    } else {
                        if (cipher_utils::validate_decryption_params(params)) {
                            valid = true;
                            success = cipher_utils::decrypt_file(params.input_file, params.output_file, params.pegs);
                        }
                    }

                    if (valid) {
                        if (success) {
                            set_gui_message(std::string(current_screen == AppScreen::EncryptFiles ? "Encryption" : "Decryption") + " Successful!", false);
                        } else {
                            set_gui_message(std::string(current_screen == AppScreen::EncryptFiles ? "Encryption" : "Decryption") + " Failed during processing.", true);
                        }
                    } else {
                        std::string captured_errors = validation_errors_stream.str();
                        if (!captured_errors.empty()) {
                            set_gui_message("Validation Failed:\n" + captured_errors, true);
                        } else {
                            set_gui_message("Validation Failed. Please check input fields and ensure files are valid.", true);
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Back to Main Menu", ImVec2(action_button_width, 0))) { current_screen = AppScreen::MainMenu; gui_message.clear(); }
                break;
            }
             case AppScreen::GetItem:
                 ImGui::Text("Get Item Screen (Placeholder)"); ImGui::Separator();
                 if (ImGui::Button("Back to Main Menu", ImVec2(main_menu_button_width, 0))) { current_screen = AppScreen::MainMenu; gui_message.clear(); }
                 break;
            case AppScreen::CompareFiles:
                 ImGui::Text("Compare Files Screen (Placeholder)"); ImGui::Separator();
                 if (ImGui::Button("Back to Main Menu", ImVec2(main_menu_button_width, 0))) { current_screen = AppScreen::MainMenu; gui_message.clear(); }
                 break;
            case AppScreen::History:
                ImGui::Text("Encryption History"); ImGui::Separator();
                float history_height = std::min(300.0f, ImGui::GetTextLineHeightWithSpacing() * 20); // Max 20 lines or 300px
                ImGui::BeginChild("HistoryScrollRegion", ImVec2(0, history_height), true, ImGuiWindowFlags_HorizontalScrollbar);
                ImGui::TextUnformatted(history_content_buf.c_str());
                ImGui::EndChild();

                float history_button_width = std::max(80.0f, (available_width - ImGui::GetStyle().ItemSpacing.x - 20.0f) / 2.0f);
                if (ImGui::Button("Refresh History", ImVec2(history_button_width, 0))) {
                    load_history_content();
                }
                ImGui::SameLine();
                if (ImGui::Button("Back to Main Menu", ImVec2(history_button_width, 0))) { current_screen = AppScreen::MainMenu; gui_message.clear(); }
                break;
        }

        ImVec2 current_imgui_panel_size = ImGui::GetWindowSize();
        ImGui::End(); // End "Cipher Application Panel"

        // Dynamically resize GLFW OS window to match ImGui panel's determined size
        // The ImGui panel's size is already constrained by SetNextWindowSizeConstraints
        float target_os_width = current_imgui_panel_size.x;
        float target_os_height = current_imgui_panel_size.y;

        // Only resize if the target size is different from the last OS window size
        if (std::abs(target_os_width - last_os_window_size.x) > 1e-3f || std::abs(target_os_height - last_os_window_size.y) > 1e-3f) {
            glfwSetWindowSize(window, static_cast<int>(target_os_width), static_cast<int>(target_os_height));
            last_os_window_size = ImVec2(target_os_width, target_os_height);
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h); // This should now match target_os_width/height
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
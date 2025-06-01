/**
 * @file jay_gui.cpp
 * @brief Implements the main GUI application logic using Dear ImGui and GLFW.
 *
 * This file is responsible for initializing the graphics window, setting up ImGui,
 * running the main application loop (event handling, UI drawing, rendering),
 * and cleaning up resources on exit. It manages different application "screens"
 * like the main menu, encryption/decryption views, etc.
 */

#define GL_SILENCE_DEPRECATION // To silence OpenGL deprecation warnings on macOS

#include "jay_gui.h"      // For the declaration of run_gui_application
#include "cipher_utils.h" // For MAX_FILENAME, cipher functions, and constants (MIN_PEG, MAX_PEG)

// Dear ImGui Headers
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Standard Library
#include <cstdio>   // For printf, fprintf (used for error handling and placeholder messages)
#include <cstring>  // For strlen (used for basic input validation)

// GLFW (Graphics Library Framework) - for windowing and input
#if defined(__APPLE__)
#include <OpenGL/gl3.h> // For macOS specific OpenGL headers
#else
// On other platforms, GLFW typically drags in necessary GL headers,
// or ImGui's backend handles GL loading if you were using something like GLAD/GLEW.
#endif
#include <GLFW/glfw3.h>

/**
 * @brief GLFW error callback function.
 *        Prints GLFW errors to stderr.
 * @param error An error code.
 * @param description A human-readable description of the error.
 */
static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

/**
 * @enum AppScreen
 * @brief Defines the different views or "screens" available in the application.
 *        Used to control which UI elements are currently displayed.
 */
enum class AppScreen {
    MainMenu,       ///< The main menu screen with primary navigation options.
    EncryptFiles,   ///< Screen for encrypting files.
    DecryptFiles,   ///< Screen for decrypting files.
    GetItem,        ///< Placeholder screen for a "Get Item" feature.
    CompareFiles,   ///< Placeholder screen for a "Compare Files" feature.
    History         ///< Screen for viewing encryption/decryption history.
};
/// @brief Static variable holding the currently active application screen.
static AppScreen current_screen = AppScreen::MainMenu;

// Static buffers to hold user input from ImGui widgets.
// Note: For more complex applications, using std::string or more robust data
// handling mechanisms would be preferable over fixed-size C-style char arrays.
static char username_buf[128] = "";             ///< Buffer for username input.
static char password_buf[128] = "";             ///< Buffer for password input.
static char input_file_path[MAX_FILENAME] = ""; ///< Buffer for input file path. Uses MAX_FILENAME from cipher_utils.h.
static char output_file_path[MAX_FILENAME] = "";///< Buffer for output file path. Uses MAX_FILENAME from cipher_utils.h.
static int pegs_value = 7;                      ///< Buffer for the Caesar cipher peg/shift value.

/**
 * @brief Initializes and runs the main GUI application.
 *
 * This function performs the following steps:
 * 1. Initializes GLFW and creates a window with an OpenGL context.
 * 2. Initializes Dear ImGui, including context, style, and platform/renderer backends.
 * 3. Enters the main application loop, which handles:
 *    - Event polling (input from mouse/keyboard).
 *    - Starting new ImGui frames.
 *    - Drawing the application's UI based on the current_screen state.
 *    - Rendering the ImGui draw data.
 *    - Swapping frame buffers.
 * 4. Cleans up ImGui and GLFW resources upon exiting the loop.
 *
 * @return int Returns 0 on successful execution, 1 on failure (e.g., GLFW or window creation failed).
 */
int run_gui_application() {
    // Setup GLFW window and error callback
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    // Set GLFW window hints for OpenGL version and profile.
    // This ensures we get a compatible OpenGL context for ImGui's OpenGL3 backend.
#if defined(__APPLE__) // macOS requires specific hints for core profile OpenGL
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // Use core profile
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else // General case for other platforms (Windows, Linux)
    const char* glsl_version = "#version 130"; // A common baseline
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0); // Or higher depending on support
#endif

    // Create the main application window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Cipher GUI - Dear ImGui", nullptr, nullptr);
    if (window == nullptr) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window); // Make the window's OpenGL context current
    glfwSwapInterval(1); // Enable vsync to cap frame rate to monitor refresh rate

    // Initialize Dear ImGui context
    IMGUI_CHECKVERSION(); // Check ImGui version compatibility
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io; // Get ImGui IO structure
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard navigation

    // Set ImGui style (Dark is default, Light or Classic also available)
    ImGui::StyleColorsDark();

    // Initialize ImGui Platform and Renderer backends for GLFW and OpenGL3
    ImGui_ImplGlfw_InitForOpenGL(window, true); // true = install callbacks
    ImGui_ImplOpenGL3_Init(glsl_version);       // Initialize with the chosen GLSL version string

    // Background color for clearing the screen
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // --- Main Application Loop ---
    while (!glfwWindowShouldClose(window)) { // Loop until the user closes the window
        // Poll for and process events (keyboard, mouse, window events)
        glfwPollEvents();

        // Start a new Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame(); // Backend new frame
        ImGui_ImplGlfw_NewFrame();    // Platform new frame
        ImGui::NewFrame();            // ImGui core new frame

        // --- UI Drawing Code ---
        // Create a main application panel that fills the entire viewport
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);   // Position at viewport's top-left work area
        ImGui::SetNextWindowSize(viewport->WorkSize); // Size to fill viewport's work area

        // Flags for the main panel: no title bar, not movable/resizable, but with a menu bar
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
        window_flags |= ImGuiWindowFlags_MenuBar;

        // Begin the main ImGui window (our application panel)
        ImGui::Begin("Cipher Application Panel", nullptr, window_flags);

        // Application Menu Bar at the top of the panel
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE); // Signal GLFW to close the window
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Navigation")) {
                // Menu items to switch between different application screens
                if (ImGui::MenuItem("Main Menu", nullptr, current_screen == AppScreen::MainMenu)) { current_screen = AppScreen::MainMenu; }
                if (ImGui::MenuItem("Encrypt Files", nullptr, current_screen == AppScreen::EncryptFiles)) { current_screen = AppScreen::EncryptFiles; }
                if (ImGui::MenuItem("Decrypt Files", nullptr, current_screen == AppScreen::DecryptFiles)) { current_screen = AppScreen::DecryptFiles; }
                if (ImGui::MenuItem("View History", nullptr, current_screen == AppScreen::History)) { current_screen = AppScreen::History; }
                // Add more navigation items here for other screens
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::Spacing(); // Add a little vertical space after the menu bar

        // --- Main Content Area: Render UI based on the current_screen ---
        switch (current_screen) {
            case AppScreen::MainMenu:
                ImGui::Text("Main Menu");
                ImGui::Separator(); // Horizontal line
                // Buttons to navigate to other screens. ImVec2(width, height) for button size.
                if (ImGui::Button("Encrypt Files", ImVec2(200, 50))) { current_screen = AppScreen::EncryptFiles; }
                ImGui::Dummy(ImVec2(0,5)); // Small vertical spacer
                if (ImGui::Button("Decrypt Files", ImVec2(200, 50))) { current_screen = AppScreen::DecryptFiles; }
                ImGui::Dummy(ImVec2(0,5));
                if (ImGui::Button("Get Item (placeholder)", ImVec2(200, 50))) { current_screen = AppScreen::GetItem; }
                ImGui::Dummy(ImVec2(0,5));
                if (ImGui::Button("Compare Files (placeholder)", ImVec2(200, 50))) { current_screen = AppScreen::CompareFiles; }
                ImGui::Dummy(ImVec2(0,5));
                if (ImGui::Button("View History", ImVec2(200, 50))) { current_screen = AppScreen::History; }
                break;

            case AppScreen::EncryptFiles:
                ImGui::Text("Encrypt Files");
                ImGui::Separator();
                ImGui::InputText("Input File Path", input_file_path, MAX_FILENAME);
                ImGui::InputText("Output File Path", output_file_path, MAX_FILENAME);
                // InputInt for integer input, or SliderInt for a slider
                ImGui::InputInt("Pegs", &pegs_value);
                // Clamp pegs_value to be within MIN_PEG and MAX_PEG (defined in cipher_utils.h)
                pegs_value = (pegs_value < MIN_PEG) ? MIN_PEG : (pegs_value > MAX_PEG) ? MAX_PEG : pegs_value;

                if (ImGui::Button("Encrypt")) {
                    // Basic validation and call to your cipher logic
                    if (strlen(input_file_path) > 0 && strlen(output_file_path) > 0) {
                        if (validate_file(input_file_path)) { // Calls function from cipher_utils
                            if (encrypt_file(input_file_path, output_file_path, pegs_value)) { // Calls function from cipher_utils
                                log_encryption(input_file_path, output_file_path, pegs_value); // Calls function from cipher_utils
                                printf("GUI Message: Encryption Successful!\n"); // Placeholder: replace with ImGui message
                            } else {
                                printf("GUI Message: Encryption Failed.\n"); // Placeholder
                            }
                        } else {
                            // validate_file should print its own error. This is an additional GUI notification.
                            printf("GUI Message: Input file validation failed.\n"); // Placeholder
                        }
                    } else {
                        printf("GUI Message: File paths cannot be empty.\n"); // Placeholder
                    }
                }
                ImGui::SameLine(); // Place next widget on the same line
                if (ImGui::Button("Back to Main Menu")) { current_screen = AppScreen::MainMenu; }
                break;

            case AppScreen::DecryptFiles:
                ImGui::Text("Decrypt Files");
                ImGui::Separator();
                ImGui::InputText("Username", username_buf, sizeof(username_buf));
                ImGui::InputText("Password", password_buf, sizeof(password_buf), ImGuiInputTextFlags_Password); // Flag hides password text
                ImGui::Dummy(ImVec2(0.0f, 10.0f)); // Larger vertical spacer
                ImGui::InputText("Input File Path", input_file_path, MAX_FILENAME);
                ImGui::InputText("Output File Path", output_file_path, MAX_FILENAME);
                ImGui::InputInt("Pegs", &pegs_value);
                pegs_value = (pegs_value < MIN_PEG) ? MIN_PEG : (pegs_value > MAX_PEG) ? MAX_PEG : pegs_value;

                if (ImGui::Button("Decrypt")) {
                    if (strlen(username_buf) > 0 && strlen(password_buf) > 0) {
                        // --- TODO: Implement actual user authentication using your User/Admin structs ---
                        // This is a placeholder for where you'd check credentials.
                        printf("User: %s, Pass: *** (placeholder validation)\n", username_buf);
                        // For now, we'll assume authentication passes to test decryption.
                        // --- End TODO ---
                        if (validate_file(input_file_path)) {
                            if (decrypt_file(input_file_path, output_file_path, pegs_value)) {
                                printf("GUI Message: Decryption Successful!\n"); // Placeholder
                            } else {
                                printf("GUI Message: Decryption Failed.\n"); // Placeholder
                            }
                        } else {
                            printf("GUI Message: Input file validation failed.\n"); // Placeholder
                        }
                    } else {
                        printf("GUI Message: Username and password required.\n"); // Placeholder
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Back to Main Menu")) { current_screen = AppScreen::MainMenu; }
                break;

            case AppScreen::GetItem:
                 ImGui::Text("Get Item Screen (Placeholder)"); ImGui::Separator();
                 // TODO: Implement UI for "Get Item" functionality
                 if (ImGui::Button("Back to Main Menu")) { current_screen = AppScreen::MainMenu; }
                 break;
            case AppScreen::CompareFiles:
                 ImGui::Text("Compare Files Screen (Placeholder)"); ImGui::Separator();
                 // TODO: Implement UI for "Compare Files" functionality
                 if (ImGui::Button("Back to Main Menu")) { current_screen = AppScreen::MainMenu; }
                 break;
            case AppScreen::History:
                ImGui::Text("Encryption History"); ImGui::Separator();
                if (ImGui::Button("Show History in Console (for now)")) {
                    view_history(); // This will print to your terminal via cipher_utils
                }
                // TODO: For a proper GUI history view:
                // 1. Create a function in cipher_utils.cpp to read history.md into a std::string or char buffer.
                // 2. In this case, call that function.
                // 3. Use ImGui::TextUnformatted() or ImGui::TextWrapped() within a scrolling child window
                //    (ImGui::BeginChild() / ImGui::EndChild()) to display the history content.
                ImGui::Dummy(ImVec2(0, 20));
                if (ImGui::Button("Back to Main Menu")) { current_screen = AppScreen::MainMenu; }
                break;
        }
        ImGui::End(); // End of "Cipher Application Panel"

        // --- End UI Drawing Code ---

        // Rendering Phase
        ImGui::Render(); // Gather all ImGui draw data
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h); // Get actual framebuffer size
        glViewport(0, 0, display_w, display_h);                 // Set OpenGL viewport to cover the whole window
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w); // Set screen clear color
        glClear(GL_COLOR_BUFFER_BIT);                           // Clear the screen
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // Render ImGui draw data using OpenGL3 backend

        // Note: Multi-viewport (platform windows) rendering block has been removed for simplicity.
        // If re-enabled, io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable must be set,
        // and the corresponding ImGui::UpdatePlatformWindows() and ImGui::RenderPlatformWindowsDefault() calls.

        glfwSwapBuffers(window); // Swap the front and back buffers to display the rendered frame
    }

    // --- Cleanup ---
    // Shutdown ImGui platform and renderer backends
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(); // Destroy ImGui context

    // Destroy GLFW window and terminate GLFW
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0; // Indicate successful execution
}
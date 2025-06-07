#pragma once

#include <string>
#include <utility> // For std::pair

#include "imgui.h"
#include "cipher_utils.h" // For constants like MAX_FILENAME_BUFFER_SIZE

// Forward-declare GLFWwindow to avoid including the GLFW header here
struct GLFWwindow;

class UIManager {
public:
    UIManager();

    // Draws the entire UI and returns the required content size and a scaling factor
    std::pair<ImVec2, float> draw_ui(GLFWwindow* window);
    
    // Returns true if any modal dialog is currently active
    bool is_modal_active() const noexcept;

private:
    // Scoped enums (enum class) are more type-safe and prevent naming conflicts
    enum class Screen { MainMenu, Encrypt, Decrypt, GetItem, Compare, History };
    enum class Modal { None, AdminPasswordPrompt, CompareFilesPrompt };

    // --- Private Helper Methods ---
    void go_to_screen(Screen new_screen);
    void set_main_gui_message(const std::string& message, const ImVec4& color);
    void load_history_content();
    void request_admin_access_for_screen(Screen target_screen);
    void clear_all_persistent_state();

    // --- UI Drawing Methods (one for each major component) ---
    ImVec2 draw_main_menu_screen();
    ImVec2 draw_encrypt_decrypt_screen(bool is_encrypt_mode);
    ImVec2 draw_get_item_screen();
    ImVec2 draw_history_screen();
    ImVec2 draw_compare_files_screen();
    void draw_admin_password_prompt_modal(const std::string& prompt_message);
    void draw_compare_files_modal();

    // --- Member Variables ---

    // UI State
    Screen current_screen;
    Screen screen_requiring_password;
    Modal current_modal;
    bool admin_access_granted;

    // GUI Message
    std::string gui_message;
    ImVec4 gui_message_color;

    // Input Buffers for ImGui Widgets
    static constexpr size_t MAX_PATH_LEN = MAX_FILENAME_BUFFER_SIZE;
    char input_file_path_buf[MAX_PATH_LEN];
    char output_file_path_buf[MAX_PATH_LEN];
    char get_item_filename_buf[MAX_PATH_LEN];
    char get_item_destination_buf[MAX_PATH_LEN];
    char compare_modal_vault_filename_buf[MAX_PATH_LEN];
    char compare_modal_external_enc_filepath_buf[MAX_PATH_LEN];
    char admin_password_buf[128];
    int pegs_value;
    int compare_modal_pegs_value;
    std::string history_content_buf;

    // --- UI Configuration Constants (C++17 inline lets us define them here) ---
    inline static constexpr ImVec4 MSG_COLOR_INFO    = {0.6f, 0.8f, 1.0f, 1.0f}; // Light Blue
    inline static constexpr ImVec4 MSG_COLOR_SUCCESS = {0.6f, 1.0f, 0.6f, 1.0f}; // Light Green
    inline static constexpr ImVec4 MSG_COLOR_ERROR   = {1.0f, 0.6f, 0.6f, 1.0f}; // Light Red
    inline static constexpr ImVec4 MSG_COLOR_WARNING = {1.0f, 1.0f, 0.6f, 1.0f}; // Yellow

    inline static constexpr float MAIN_MENU_MIN_CONTENT_WIDTH       = 400.0f;
    inline static constexpr float MAIN_MENU_MIN_CONTENT_HEIGHT      = 250.0f;
    inline static constexpr float ENCRYPT_DECRYPT_MIN_CONTENT_WIDTH = 450.0f;
    inline static constexpr float ENCRYPT_DECRYPT_MIN_CONTENT_HEIGHT= 200.0f;
    inline static constexpr float HISTORY_MIN_CONTENT_WIDTH         = 500.0f;
    inline static constexpr float HISTORY_MIN_CONTENT_HEIGHT        = 400.0f;
    inline static constexpr float GET_ITEM_MIN_CONTENT_WIDTH        = 450.0f;
    inline static constexpr float GET_ITEM_MIN_CONTENT_HEIGHT       = 180.0f;
    
    inline static constexpr size_t MAX_TEXT_COMPARE_DISPLAY_CHARS = 5000;
};
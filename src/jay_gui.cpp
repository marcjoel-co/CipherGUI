// src/jay_gui.cpp

#define GL_SILENCE_DEPRECATION // For macOS OpenGL deprecation warnings

// Own headers
#include "jay_gui.h"
#include "cipher_utils.h" // Assumes this declares all necessary functions and constants

// ImGui and backends
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_stdlib.h" // For using std::string with ImGui::InputTextXXX functions

// Standard Library
#include <iostream>
#include <cstdio>       // For fprintf
#include <string>
#include <sstream>      // For std::ostringstream, std::stringstream
#include <fstream>      // For std::ifstream
#include <algorithm>    // For std::clamp, std::min, std::max
#include <vector>
#include <cstring>      // For memset
#include <filesystem>   // For std::filesystem (C++17)
#include <cctype>       // For ::tolower

// OpenGL / GLFW
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
// For Windows/Linux, you need an OpenGL loader like GLAD or GLEW
// Example: #include <glad/glad.h> 
#endif
#include <GLFW/glfw3.h>

// Anonymous namespace for file-local state and helper functions
namespace {

// --- Application Configuration ---
const char* const APP_TITLE = "Cipher GUI by MJ";
const ImVec4 CLEAR_COLOR = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
const ImVec4 MSG_COLOR_INFO = ImVec4(0.7f, 0.7f, 1.0f, 1.0f);
const ImVec4 MSG_COLOR_SUCCESS = ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
const ImVec4 MSG_COLOR_ERROR = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
const ImVec4 MSG_COLOR_WARNING = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);

// --- Application State ---
enum class AppScreen { MainMenu, EncryptFiles, DecryptFiles, GetItem, CompareFiles, History }; // CompareFiles kept for structure, but is now modal
AppScreen current_screen = AppScreen::MainMenu;
AppScreen screen_requiring_password = AppScreen::MainMenu;

enum class ModalState { None, AdminPasswordPrompt, CompareFilesPrompt }; // Added CompareFilesPrompt
ModalState current_modal = ModalState::None;
bool admin_access_granted = false;

// GUI Messaging
std::string gui_message = "Welcome to Cipher GUI!";
ImVec4 gui_message_color = MSG_COLOR_INFO;

// Input Buffers
constexpr size_t MAX_PATH_LEN = cipher_utils::MAX_FILENAME_BUFFER_SIZE;
char input_file_path_buf[MAX_PATH_LEN] = "";
char output_file_path_buf[MAX_PATH_LEN] = "";
int pegs_value = cipher_utils::MIN_PEG; // General pegs for Encrypt/Decrypt screens
char admin_password_buf[128] = "";
char get_item_filename_buf[MAX_PATH_LEN] = "";
char get_item_destination_buf[MAX_PATH_LEN] = "";

// Buffers for the new Compare/Verify Modal
char compare_modal_vault_filename_buf[MAX_PATH_LEN] = "";
char compare_modal_external_enc_filepath_buf[MAX_PATH_LEN] = "";
int  compare_modal_pegs_value = cipher_utils::MIN_PEG;

// Data Holders for Screens
std::string history_content_buf = ""; // For History screen

// --- Sizing Constants (Content Area Minimums) ---
const float MAIN_MENU_MIN_CONTENT_WIDTH = 300.0f;
const float MAIN_MENU_MIN_CONTENT_HEIGHT = 260.0f;
const float ENCRYPT_DECRYPT_MIN_CONTENT_WIDTH = 550.0f;
const float ENCRYPT_DECRYPT_MIN_CONTENT_HEIGHT = 150.0f;
const float HISTORY_MIN_CONTENT_WIDTH = 600.0f;
const float HISTORY_MIN_CONTENT_HEIGHT = 300.0f;
const float GET_ITEM_MIN_CONTENT_WIDTH = 550.0f;
const float GET_ITEM_MIN_CONTENT_HEIGHT = 120.0f;
// const float COMPARE_FILES_MIN_CONTENT_WIDTH = 750.0f; // No longer a dedicated screen
// const float COMPARE_FILES_MIN_CONTENT_HEIGHT = 500.0f; // No longer a dedicated screen
const size_t MAX_TEXT_COMPARE_DISPLAY_CHARS = 50000; // For loading content in modal


// --- Utility Functions ---
void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void set_main_gui_message(const std::string& message, const ImVec4& color) {
    gui_message = message;
    gui_message_color = color;
}

// RAII class for redirecting std::cerr
class CerrRedirect {
public:
    CerrRedirect(std::streambuf* new_buffer) : old_cerr_buf(std::cerr.rdbuf(new_buffer)) {}
    ~CerrRedirect() { std::cerr.rdbuf(old_cerr_buf); }
private:
    CerrRedirect(const CerrRedirect&) = delete;
    CerrRedirect& operator=(const CerrRedirect&) = delete;
    std::streambuf* old_cerr_buf;
};

void load_history_content() {
    std::ifstream ifs(cipher_utils::HISTORY_FILE);
    if (ifs) {
        std::stringstream ss;
        ss << ifs.rdbuf();
        history_content_buf = ss.str();
        if (history_content_buf.empty()) {
            history_content_buf = "History is empty or the file could not be read.";
        }
    } else {
        history_content_buf = "Error: Could not open history file: " + std::string(cipher_utils::HISTORY_FILE);
    }
}

// --- UI Navigation and State Change Helpers ---
void clear_all_persistent_state() {
    input_file_path_buf[0] = '\0';
    output_file_path_buf[0] = '\0';
    get_item_filename_buf[0] = '\0';
    get_item_destination_buf[0] = '\0';
    admin_password_buf[0] = '\0';
    pegs_value = cipher_utils::MIN_PEG;

    compare_modal_vault_filename_buf[0] = '\0';
    compare_modal_external_enc_filepath_buf[0] = '\0';
    compare_modal_pegs_value = cipher_utils::MIN_PEG;

    history_content_buf.clear();
}

void go_to_screen(AppScreen screen) {
    if (current_screen != screen) {
         gui_message.clear(); 
    }
    current_screen = screen;

    switch (screen) {
        case AppScreen::MainMenu:
            admin_access_granted = false;
            clear_all_persistent_state();
            set_main_gui_message("Welcome to Cipher GUI!", MSG_COLOR_INFO);
            break;
        case AppScreen::EncryptFiles:
        case AppScreen::DecryptFiles:
            break;
        case AppScreen::GetItem:
            get_item_filename_buf[0] = '\0';
            get_item_destination_buf[0] = '\0';
            break;
        case AppScreen::CompareFiles: // This screen is now effectively a placeholder
            // Functionality moved to a modal.
            // We could set a message here or just let it draw its minimal content.
            // For now, it will draw the placeholder from DrawCompareFilesScreenPlaceholder.
            break; 
        case AppScreen::History:
            if (admin_access_granted) {
                load_history_content();
            }
            break;
    }
}

void request_admin_access_for_screen(AppScreen target_screen) {
    screen_requiring_password = target_screen;
    current_modal = ModalState::AdminPasswordPrompt;
    gui_message.clear();
}


// --- UI Drawing Functions ---

bool DrawAdminPasswordPromptModal(const std::string& prompt_message) {
    bool action_taken = false;
    ImGui::OpenPopup("Admin Password Modal"); 

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowFocus();

    if (ImGui::BeginPopupModal("Admin Password Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextUnformatted(prompt_message.c_str());
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5.0f));
        ImGui::TextUnformatted("Please enter the admin password:");
        ImGui::PushItemWidth(250);
        bool enter_pressed = ImGui::InputText("##AdminPasswordInput", admin_password_buf, sizeof(admin_password_buf), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::Dummy(ImVec2(0, 10.0f));

        if (enter_pressed || ImGui::Button("Login", ImVec2(120, 0))) {
            action_taken = true;
            if (cipher_utils::check_admin_password(admin_password_buf)) {
                admin_access_granted = true;
                current_modal = ModalState::None;
                set_main_gui_message("Admin access granted.", MSG_COLOR_SUCCESS);
                go_to_screen(screen_requiring_password); 
                ImGui::CloseCurrentPopup();
            } else {
                set_main_gui_message("Incorrect admin password.", MSG_COLOR_ERROR);
            }
            admin_password_buf[0] = '\0'; 
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            action_taken = true;
            current_modal = ModalState::None;
            gui_message.clear(); 
            admin_password_buf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return action_taken;
}

// New Modal for Compare/Verify
// In jay_gui.cpp, within the anonymous namespace

// In jay_gui.cpp, within the anonymous namespace

bool DrawCompareFilesModal() {
    bool action_taken = false;
    ImGui::OpenPopup("Compare Encrypted File Modal");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // Set a preferred initial width for the modal, similar to main menu content width
    // Height will still be auto-resized.
    float modal_content_width = MAIN_MENU_MIN_CONTENT_WIDTH + 100.0f; // A bit wider for paths
    ImGui::SetNextWindowSize(ImVec2(modal_content_width, 0), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(modal_content_width, 0), ImVec2(modal_content_width + 200.0f, FLT_MAX)); // Allow some horizontal growth if needed

    ImGui::SetNextWindowFocus();

    if (ImGui::BeginPopupModal("Compare Encrypted File Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGuiStyle& style = ImGui::GetStyle();
        float current_content_width = ImGui::GetContentRegionAvail().x; // Actual available width

        ImGui::TextUnformatted("Verify Encrypted File");
        ImGui::TextWrapped("Compares an in-memory encryption of a vault file against an existing external encrypted file.");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5.0f));

        // Path inputs take full available width
        ImGui::PushItemWidth(current_content_width);
        ImGui::InputTextWithHint("##VaultFileModal", "Filename in Vault (e.g., original.txt)", compare_modal_vault_filename_buf, MAX_PATH_LEN);
        ImGui::InputTextWithHint("##ExternalEncFileModal", "Path to External Encrypted File", compare_modal_external_enc_filepath_buf, MAX_PATH_LEN);
        ImGui::PopItemWidth();
        ImGui::Dummy(ImVec2(0, 5.0f));


        // Pegs input (like the image)
        float pegs_label_width = ImGui::CalcTextSize("Pegs Used for Encryption").x;
        float small_button_width = ImGui::GetFrameHeight(); // Square buttons
        float pegs_numeric_input_width = 80.0f;
        // Calculate remaining width for the numeric input if we want to fill space, or keep it fixed.
        // For now, fixed width for numeric input is fine.
        // float available_for_pegs_line = current_content_width;
        // float pegs_numeric_input_width = available_for_pegs_line - pegs_label_width - (small_button_width * 2) - (style.ItemInnerSpacing.x * 3) - style.FramePadding.x *2;
        // pegs_numeric_input_width = std::max(50.0f, pegs_numeric_input_width);


        ImGui::PushItemWidth(pegs_numeric_input_width);
        ImGui::InputInt("##PegsModalInput", &compare_modal_pegs_value);
        ImGui::PopItemWidth();
        compare_modal_pegs_value = std::clamp(compare_modal_pegs_value, cipher_utils::MIN_PEG, cipher_utils::MAX_PEG);

        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        if (ImGui::Button("-", ImVec2(small_button_width, small_button_width))) {
            compare_modal_pegs_value--;
            compare_modal_pegs_value = std::clamp(compare_modal_pegs_value, cipher_utils::MIN_PEG, cipher_utils::MAX_PEG);
        }
        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        if (ImGui::Button("+", ImVec2(small_button_width, small_button_width))) {
            compare_modal_pegs_value++;
            compare_modal_pegs_value = std::clamp(compare_modal_pegs_value, cipher_utils::MIN_PEG, cipher_utils::MAX_PEG);
        }
        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        ImGui::TextUnformatted("Pegs Used for Encryption");
        
        ImGui::Dummy(ImVec2(0, 10.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5.0f));

        // Buttons: Make them a good portion of the modal width, similar to main menu buttons
        float modal_button_width = (current_content_width - style.ItemSpacing.x) / 2.0f; // Two buttons

        if (ImGui::Button("Compare", ImVec2(modal_button_width, 0))) {
            action_taken = true;
            gui_message.clear(); 

            std::string vault_filename = compare_modal_vault_filename_buf;
            std::string external_enc_filepath = compare_modal_external_enc_filepath_buf;

            if (vault_filename.empty() || external_enc_filepath.empty()) {
                set_main_gui_message("Error: All fields (Vault Filename, External File Path, Pegs) must be provided.", MSG_COLOR_ERROR);
            } else {
                std::filesystem::path vault_file_full_path = std::filesystem::path(cipher_utils::PRIVATE_VAULT_DIR) / vault_filename;
                
                std::string vault_content = cipher_utils::load_file_content_to_string(vault_file_full_path.string(), MAX_TEXT_COMPARE_DISPLAY_CHARS);
                std::string external_enc_content = cipher_utils::load_file_content_to_string(external_enc_filepath, MAX_TEXT_COMPARE_DISPLAY_CHARS);

                std::string status_msg_prefix;
                bool problem_loading_files = false;

                if (!std::filesystem::exists(vault_file_full_path)) {
                    status_msg_prefix += "Error: Vault file '" + vault_file_full_path.string() + "' not found. ";
                    problem_loading_files = true;
                } else if (vault_content.empty() && std::filesystem::file_size(vault_file_full_path) > 0) {
                    status_msg_prefix += "Warning: Vault file '" + vault_file_full_path.string() + "' is empty or could not be fully read. ";
                }

                if (!std::filesystem::exists(external_enc_filepath)) {
                     status_msg_prefix += "Error: External encrypted file '" + external_enc_filepath + "' not found. ";
                     problem_loading_files = true;
                } else if (external_enc_content.empty() && std::filesystem::file_size(external_enc_filepath) > 0) {
                     status_msg_prefix += "Warning: External encrypted file '" + external_enc_filepath + "' is empty or could not be fully read. ";
                }

                if (problem_loading_files) {
                    set_main_gui_message(status_msg_prefix, MSG_COLOR_ERROR);
                } else {
                    std::string in_memory_encrypted_vault_content = cipher_utils::process_content_caesar(vault_content, compare_modal_pegs_value, true);
                    
                    cipher_utils::TextCompareResult comparison_result = cipher_utils::compare_string_contents(
                        in_memory_encrypted_vault_content, 
                        external_enc_content
                    );

                    std::string result_details = "Match: " + std::to_string(comparison_result.match_percentage) + "%%.";
                    if (comparison_result.first_diff_offset != -1) {
                        result_details += " First diff at offset: " + std::to_string(comparison_result.first_diff_offset) + ".";
                    } else if (comparison_result.match_percentage >= 99.99f && comparison_result.content1.length() == comparison_result.content2.length()) {
                         result_details += " Contents appear identical.";
                    }
                    
                    std::string final_msg = status_msg_prefix + "Verification Result: " + result_details;
                    ImVec4 final_color = MSG_COLOR_INFO; // Default to info
                    if (!status_msg_prefix.empty()) { // If there were any loading warnings
                        final_color = MSG_COLOR_WARNING;
                    }
                    if (problem_loading_files) { // If critical loading error
                        final_color = MSG_COLOR_ERROR;
                    } else if (comparison_result.match_percentage >= 99.99f && comparison_result.content1.length() == comparison_result.content2.length() && status_msg_prefix.empty()) {
                        final_color = MSG_COLOR_SUCCESS; // Success only if no warnings and perfect match
                    } else if (status_msg_prefix.empty()) { // No loading issues, but mismatch
                        final_color = MSG_COLOR_WARNING; 
                    }


                    set_main_gui_message(final_msg, final_color);
                }
            }
            current_modal = ModalState::None; 
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(modal_button_width, 0))) {
            action_taken = true;
            current_modal = ModalState::None;
            gui_message.clear(); 
            compare_modal_vault_filename_buf[0] = '\0';
            compare_modal_external_enc_filepath_buf[0] = '\0';
            compare_modal_pegs_value = cipher_utils::MIN_PEG;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    return action_taken;
}

ImVec2 DrawMainMenuScreen() {
    ImGuiStyle& style = ImGui::GetStyle();
    float button_width = MAIN_MENU_MIN_CONTENT_WIDTH; 
    float current_y_pos = ImGui::GetCursorPosY(); 

    ImGui::TextUnformatted("Main Menu");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10.0f)); 
    current_y_pos = ImGui::GetCursorPosY();

    struct MenuItem {
        const char* label;
        AppScreen screen_to_open; // For actual screens
        ModalState modal_to_open;  // For modals
        bool requires_admin;
    };

    const MenuItem menu_items[] = {
        {"Encrypt File", AppScreen::EncryptFiles, ModalState::None, false},
        {"Decrypt File", AppScreen::DecryptFiles, ModalState::None, false},
        {"Retrieve Original File", AppScreen::GetItem, ModalState::None, true},
        {"Verify Encrypted File", AppScreen::MainMenu, ModalState::CompareFilesPrompt, false}, // Stays on MainMenu, opens modal
        {"View History", AppScreen::History, ModalState::None, true}
    };
    const float button_height = 35.0f;

    for (const auto& item : menu_items) {
        if (ImGui::Button(item.label, ImVec2(button_width, button_height))) {
            if (item.requires_admin && !admin_access_granted) {
                request_admin_access_for_screen(item.screen_to_open); // target screen for after login
            } else {
                if (item.modal_to_open != ModalState::None) {
                    current_modal = item.modal_to_open;
                    // Clear specific modal buffers when opening
                    if (item.modal_to_open == ModalState::CompareFilesPrompt) {
                        compare_modal_vault_filename_buf[0] = '\0';
                        compare_modal_external_enc_filepath_buf[0] = '\0';
                        compare_modal_pegs_value = cipher_utils::MIN_PEG;
                    }
                    gui_message.clear(); // Clear main message when opening a modal
                } else {
                    go_to_screen(item.screen_to_open);
                }
            }
        }
        ImGui::Dummy(ImVec2(0, 5.0f)); 
    }
    current_y_pos = ImGui::GetCursorPosY() - style.ItemSpacing.y; 
    return ImVec2(MAIN_MENU_MIN_CONTENT_WIDTH, std::max(MAIN_MENU_MIN_CONTENT_HEIGHT, current_y_pos));
}

ImVec2 DrawEncryptDecryptScreen() {
    ImGuiStyle& style = ImGui::GetStyle();
    bool is_encrypt_mode = (current_screen == AppScreen::EncryptFiles);
    float available_width = ENCRYPT_DECRYPT_MIN_CONTENT_WIDTH;
    float current_y_pos = ImGui::GetCursorPosY();

    ImGui::TextUnformatted(is_encrypt_mode ? "Encrypt File & Vault Original" : "Decrypt File");
    ImGui::Separator();
    current_y_pos = ImGui::GetCursorPosY();

    ImGui::PushItemWidth(available_width * 0.70f); 
    ImGui::InputTextWithHint("##InputFilePath", "Input File Path", input_file_path_buf, sizeof(input_file_path_buf));
    if (!is_encrypt_mode) {
        ImGui::InputTextWithHint("##OutputFilePath", "Output File Path", output_file_path_buf, sizeof(output_file_path_buf));
    }
    ImGui::PopItemWidth();

    ImGui::SameLine(0, style.ItemInnerSpacing.x);
    ImGui::PushItemWidth(std::max(100.0f, available_width * 0.25f)); // Ensure pegs input is reasonably wide
    ImGui::InputInt("Pegs", &pegs_value);
    ImGui::PopItemWidth();
    pegs_value = std::clamp(pegs_value, cipher_utils::MIN_PEG, cipher_utils::MAX_PEG);
    
    current_y_pos = ImGui::GetCursorPosY(); 
    ImGui::Dummy(ImVec2(0, 10.0f));
    current_y_pos = ImGui::GetCursorPosY();

    float action_button_width = (available_width - style.ItemSpacing.x) / 2.0f;
    if (ImGui::Button(is_encrypt_mode ? "Encrypt & Vault" : "Decrypt", ImVec2(action_button_width, 0))) {
        gui_message.clear();
        std::ostringstream captured_cerr_output;
        CerrRedirect redirect(captured_cerr_output.rdbuf());
        bool params_valid = false;
        bool operation_successful = false;

        if (is_encrypt_mode) {
            if (cipher_utils::validate_encryption_params_new(input_file_path_buf, pegs_value)) {
                params_valid = true;
                operation_successful = cipher_utils::encrypt_file(input_file_path_buf, pegs_value);
            }
        } else {
            cipher_utils::OperationParams params = {input_file_path_buf, output_file_path_buf, pegs_value};
            if (cipher_utils::validate_decryption_params(params)) {
                params_valid = true;
                operation_successful = cipher_utils::decrypt_file(params.input_file, params.output_file, params.pegs);
            }
        }

        std::string cerr_msg = captured_cerr_output.str();
        if (!params_valid) {
            set_main_gui_message("Validation Failed." + (cerr_msg.empty() ? "" : "\nDetails:\n" + cerr_msg), MSG_COLOR_ERROR);
        } else if (!operation_successful) {
            set_main_gui_message(std::string(is_encrypt_mode ? "Encryption" : "Decryption") + " failed." + (cerr_msg.empty() ? "" : "\nDetails:\n" + cerr_msg), MSG_COLOR_ERROR);
        } else {
            set_main_gui_message(std::string(is_encrypt_mode ? "Encryption" : "Decryption") + " successful!" + (cerr_msg.empty() ? "" : "\nLog:\n" + cerr_msg), MSG_COLOR_SUCCESS);
            if (is_encrypt_mode && operation_successful) { 
                input_file_path_buf[0] = '\0';
            } else if (!is_encrypt_mode && operation_successful) {
                input_file_path_buf[0] = '\0';
                output_file_path_buf[0] = '\0';
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Back to Main Menu", ImVec2(action_button_width, 0))) {
        go_to_screen(AppScreen::MainMenu);
    }
    current_y_pos = ImGui::GetCursorPosY();
    return ImVec2(ENCRYPT_DECRYPT_MIN_CONTENT_WIDTH, std::max(ENCRYPT_DECRYPT_MIN_CONTENT_HEIGHT, current_y_pos));
}

ImVec2 DrawGetItemScreen() {
    ImGuiStyle& style = ImGui::GetStyle();
    float available_width = GET_ITEM_MIN_CONTENT_WIDTH;
    float current_y_pos = ImGui::GetCursorPosY();

    ImGui::TextUnformatted("Retrieve Original File from Vault");
    ImGui::Separator();
    current_y_pos = ImGui::GetCursorPosY();

    ImGui::PushItemWidth(available_width);
    ImGui::InputTextWithHint("##GetItemFilename", "Filename in Vault (e.g., original.txt)", get_item_filename_buf, sizeof(get_item_filename_buf));
    ImGui::InputTextWithHint("##GetItemDest", "Full Destination Path (e.g., /Users/user/Desktop/retrieved.txt)", get_item_destination_buf, sizeof(get_item_destination_buf));
    ImGui::PopItemWidth();
    ImGui::Dummy(ImVec2(0, 10.0f));
    current_y_pos = ImGui::GetCursorPosY();

    float action_button_width = (available_width - style.ItemSpacing.x) / 2.0f;
    if (ImGui::Button("Retrieve File", ImVec2(action_button_width, 0))) {
        gui_message.clear();
        std::ostringstream captured_output;
        CerrRedirect redirect_cerr(captured_output.rdbuf());

        if (strlen(get_item_filename_buf) == 0 || strlen(get_item_destination_buf) == 0) {
            set_main_gui_message("Error: Both Filename in Vault and Destination Path must be provided.", MSG_COLOR_ERROR);
        } else {
            bool success = cipher_utils::retrieve_from_vault(get_item_filename_buf, get_item_destination_buf);
            std::string op_msg = captured_output.str();
            if (success) {
                set_main_gui_message("File retrieval successful!" + (op_msg.empty() ? "" : "\nLog:\n" + op_msg), MSG_COLOR_SUCCESS);
                get_item_filename_buf[0] = '\0'; 
                get_item_destination_buf[0] = '\0';
            } else {
                set_main_gui_message("File retrieval failed." + (op_msg.empty() ? "" : "\nDetails:\n" + op_msg), MSG_COLOR_ERROR);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Back to Main Menu", ImVec2(action_button_width, 0))) {
        go_to_screen(AppScreen::MainMenu);
    }
    current_y_pos = ImGui::GetCursorPosY();
    return ImVec2(GET_ITEM_MIN_CONTENT_WIDTH, std::max(GET_ITEM_MIN_CONTENT_HEIGHT, current_y_pos));
}

ImVec2 DrawHistoryScreen() {
    ImGuiStyle& style = ImGui::GetStyle();
    float current_y_pos = ImGui::GetCursorPosY();

    ImGui::TextUnformatted("Operation History");
    ImGui::Separator();
    current_y_pos = ImGui::GetCursorPosY();

    float bottom_elements_height = ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y;
    float history_text_area_height = ImGui::GetContentRegionAvail().y - bottom_elements_height;
    history_text_area_height = std::max(150.0f, history_text_area_height);

    ImGui::InputTextMultiline("##HistoryContent", &history_content_buf, 
                              ImVec2(-1, history_text_area_height), 
                              ImGuiInputTextFlags_ReadOnly);
    current_y_pos += history_text_area_height; 

    float history_button_width = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2.0f;
    if (ImGui::Button("Refresh History", ImVec2(history_button_width, 0))) {
        load_history_content();
        set_main_gui_message("History refreshed.", MSG_COLOR_INFO);
    }
    ImGui::SameLine();
    if (ImGui::Button("Back to Main Menu", ImVec2(history_button_width, 0))) {
        go_to_screen(AppScreen::MainMenu);
    }
    current_y_pos += ImGui::GetFrameHeightWithSpacing(); 
    return ImVec2(HISTORY_MIN_CONTENT_WIDTH, std::max(HISTORY_MIN_CONTENT_HEIGHT, current_y_pos));
}

// Placeholder for the old CompareFiles screen if AppScreen::CompareFiles is still navigated to.
ImVec2 DrawCompareFilesScreenPlaceholder() {
    ImGui::TextWrapped("File comparison functionality is now available via the 'Verify Encrypted File' option in the Navigation menu, which opens a modal dialog.");
    ImGui::Spacing();
    if (ImGui::Button("Back to Main Menu")) {
        go_to_screen(AppScreen::MainMenu);
    }
    return ImVec2(400, 100); // Arbitrary small size
}


} // end anonymous namespace

// --- Main GUI Application Function ---
int run_gui_application() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    #if defined(__APPLE__)
        const char* glsl_version = "#version 150"; 
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); 
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #else
        const char* glsl_version = "#version 130"; 
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); 
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #endif
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); 

    GLFWwindow* window = glfwCreateWindow(800, 600, APP_TITLE, nullptr, nullptr); 
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 

    #if !defined(__APPLE__)
    // Example for GLAD:
    // if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    //     fprintf(stderr, "Failed to initialize OpenGL context (GLAD)\n");
    //     glfwDestroyWindow(window);
    //     glfwTerminate();
    //     return 1;
    // }
    #endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    if (!cipher_utils::ensure_private_vault_exists()) {
        fprintf(stderr, "Warning: Could not ensure private vault directory exists on startup.\n");
    }
    go_to_screen(AppScreen::MainMenu); 

    ImVec2 last_calculated_os_window_size = ImVec2(0,0);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Handle Modals First
        if (current_modal == ModalState::AdminPasswordPrompt) {
            std::string prompt_msg = "Admin privileges required.";
            if (screen_requiring_password == AppScreen::History) prompt_msg = "Access to Operation History requires Admin password.";
            else if (screen_requiring_password == AppScreen::GetItem) prompt_msg = "Access to Retrieve Original File requires Admin password.";
            DrawAdminPasswordPromptModal(prompt_msg);
        } else if (current_modal == ModalState::CompareFilesPrompt) {
            DrawCompareFilesModal();
        }
        
        // Main application window (full-viewport panel)
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGuiWindowFlags root_panel_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                            ImGuiWindowFlags_MenuBar;

        ImGui::Begin("RootCanvas", nullptr, root_panel_flags);

        float accumulated_chrome_height = ImGui::GetStyle().WindowPadding.y * 2; // Top/Bottom padding of RootCanvas

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit", "Cmd+Q")) { glfwSetWindowShouldClose(window, GLFW_TRUE); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Navigation")) {
                if (ImGui::MenuItem("Main Menu", "Cmd+M")) { go_to_screen(AppScreen::MainMenu); }
                if (ImGui::MenuItem("Encrypt File", "Cmd+E")) { go_to_screen(AppScreen::EncryptFiles); }
                if (ImGui::MenuItem("Decrypt File", "Cmd+D")) { go_to_screen(AppScreen::DecryptFiles); }
                if (ImGui::MenuItem("Verify Encrypted File", "Cmd+V")) { // Changed shortcut, "Cmd+C" might be copy
                    current_modal = ModalState::CompareFilesPrompt;
                    compare_modal_vault_filename_buf[0] = '\0';
                    compare_modal_external_enc_filepath_buf[0] = '\0';
                    compare_modal_pegs_value = cipher_utils::MIN_PEG;
                    gui_message.clear(); 
                }
                ImGui::Separator();
                auto AdminRestrictedMenuItem = [&](const char* label, AppScreen target_screen, ModalState modal_to_open, const char* shortcut = nullptr) {
                    if (ImGui::MenuItem(label, shortcut)) {
                        if (!admin_access_granted) {
                             request_admin_access_for_screen(target_screen); // Target screen after login
                        } else {
                            if (modal_to_open != ModalState::None) current_modal = modal_to_open;
                            else go_to_screen(target_screen);
                        }
                    }
                };
                AdminRestrictedMenuItem("Retrieve Original File", AppScreen::GetItem, ModalState::None, "Cmd+R");
                AdminRestrictedMenuItem("View History", AppScreen::History, ModalState::None, "Cmd+H");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Admin")) {
                if (admin_access_granted) {
                    if (ImGui::MenuItem("Logout Admin")) {
                        admin_access_granted = false;
                        set_main_gui_message("Admin logged out.", MSG_COLOR_INFO);
                        if (current_screen == AppScreen::History || current_screen == AppScreen::GetItem) {
                            go_to_screen(AppScreen::MainMenu);
                        }
                    }
                } else {
                    if (ImGui::MenuItem("Login Admin", "Cmd+L")) {
                        request_admin_access_for_screen(current_screen); 
                    }
                }
                ImGui::EndMenu();
            }
            accumulated_chrome_height += ImGui::GetFrameHeight(); 
            ImGui::EndMenuBar();
        }
        accumulated_chrome_height += ImGui::GetStyle().ItemSpacing.y; 
        ImGui::Spacing(); 

        if (!gui_message.empty()) {
            ImVec2 msg_text_size = ImGui::CalcTextSize(gui_message.c_str(), nullptr, true, ImGui::GetContentRegionAvail().x);
            ImGui::PushStyleColor(ImGuiCol_Text, gui_message_color);
            ImGui::TextWrapped("%s", gui_message.c_str());
            ImGui::PopStyleColor();
            accumulated_chrome_height += msg_text_size.y;
            ImGui::Separator();
            accumulated_chrome_height += ImGui::GetFrameHeightWithSpacing(); 
            ImGui::Spacing();
            accumulated_chrome_height += ImGui::GetStyle().ItemSpacing.y;
        }

        ImVec2 screen_content_size; 
        if (current_modal == ModalState::None) { // Only draw main screen content if no modal is active
            switch (current_screen) {
                case AppScreen::MainMenu:       screen_content_size = DrawMainMenuScreen(); break;
                case AppScreen::EncryptFiles:   
                case AppScreen::DecryptFiles:   screen_content_size = DrawEncryptDecryptScreen(); break;
                case AppScreen::GetItem:        screen_content_size = DrawGetItemScreen(); break;
                case AppScreen::CompareFiles:   screen_content_size = DrawCompareFilesScreenPlaceholder(); break; // Placeholder
                case AppScreen::History:        screen_content_size = DrawHistoryScreen(); break;
                default: // Should not happen if navigation is correct
                    go_to_screen(AppScreen::MainMenu); // Default to main menu
                    screen_content_size = DrawMainMenuScreen(); // Draw main menu this frame
                    break;
            }
        } else {
            // If a modal is active, the underlying screen still "exists" for sizing purposes,
            // but we don't want its content to visually interfere or take focus.
            // The modal will draw on top. We use the last known good size for the underlying content.
            // Or, more simply, just don't resize the window while a modal is up.
            // For dynamic resizing, we need a base content size.
            // Let's use the minimums of the current screen if a modal is up.
            switch (current_screen) {
                 case AppScreen::MainMenu: screen_content_size = ImVec2(MAIN_MENU_MIN_CONTENT_WIDTH, MAIN_MENU_MIN_CONTENT_HEIGHT); break;
                 // ... add other cases with their MIN_CONTENT sizes ...
                 default: screen_content_size = ImVec2(300,200); break; // A generic small size
            }
        }
        ImGui::End(); 

        // Dynamic Window Resizing (only if no modal is active)
        if (current_modal == ModalState::None) {
            ImVec2 desired_os_window_size = ImVec2(
                std::max(screen_content_size.x + ImGui::GetStyle().WindowPadding.x * 2, 350.0f),
                std::max(screen_content_size.y + accumulated_chrome_height, 250.0f) 
            );
            
            if (std::abs(desired_os_window_size.x - last_calculated_os_window_size.x) > 2.0f ||
                std::abs(desired_os_window_size.y - last_calculated_os_window_size.y) > 2.0f) {
                if (desired_os_window_size.x > 0 && desired_os_window_size.y > 0) {
                    glfwSetWindowSize(window, static_cast<int>(desired_os_window_size.x), static_cast<int>(desired_os_window_size.y));
                    last_calculated_os_window_size = desired_os_window_size;
                }
            }
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(CLEAR_COLOR.x * CLEAR_COLOR.w, CLEAR_COLOR.y * CLEAR_COLOR.w, CLEAR_COLOR.z * CLEAR_COLOR.w, CLEAR_COLOR.w);
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
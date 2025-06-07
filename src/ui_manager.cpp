#include "ui_manager.h"

// Note: No 'cipher_utils::' prefixes needed anymore.
#include "cipher_utils.h"

#include <GLFW/glfw3.h> // For window operations (e.g., exit)
#include "imgui.h"
#include "imgui_stdlib.h" // For using std::string with ImGui::InputTextMultiline

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm> // For std::clamp

namespace {
    // RAII class for redirecting std::cerr to capture error messages from backend functions.
    // Encapsulated here as it's an internal implementation detail of the UI manager.
    class CerrRedirect {
    public:
        CerrRedirect(std::streambuf* new_buffer) 
            : old_cerr_buf(std::cerr.rdbuf(new_buffer)) {}
        
        ~CerrRedirect() {
            std::cerr.rdbuf(old_cerr_buf);
        }

    private:
        // Disable copy and assignment
        CerrRedirect(const CerrRedirect&) = delete;
        CerrRedirect& operator=(const CerrRedirect&) = delete;

        std::streambuf* old_cerr_buf;
    };
} // namespace

UIManager::UIManager()
    : current_screen(Screen::MainMenu),
      screen_requiring_password(Screen::MainMenu),
      current_modal(Modal::None),
      admin_access_granted(false),
      gui_message("Welcome to Cipher GUI!"),
      gui_message_color(MSG_COLOR_INFO),
      pegs_value(MIN_PEG),
      compare_modal_pegs_value(MIN_PEG)
{
    clear_all_persistent_state();
    go_to_screen(Screen::MainMenu);
}

bool UIManager::is_modal_active() const noexcept {
    return current_modal != Modal::None;
}

void UIManager::clear_all_persistent_state() {
    // More idiomatic C++ way to clear C-style string buffers
    input_file_path_buf[0] = '\0';
    output_file_path_buf[0] = '\0';
    get_item_filename_buf[0] = '\0';
    get_item_destination_buf[0] = '\0';
    admin_password_buf[0] = '\0';
    compare_modal_vault_filename_buf[0] = '\0';
    compare_modal_external_enc_filepath_buf[0] = '\0';

    pegs_value = MIN_PEG;
    compare_modal_pegs_value = MIN_PEG;
    history_content_buf.clear();
}

std::pair<ImVec2, float> UIManager::draw_ui(GLFWwindow* window) {
    // --- Handle Modals ---
    if (current_modal == Modal::AdminPasswordPrompt) {
        std::string prompt_msg = "Admin privileges required.";
        if (screen_requiring_password == Screen::History) prompt_msg = "Access to Operation History requires Admin password.";
        else if (screen_requiring_password == Screen::GetItem) prompt_msg = "Access to Retrieve Original File requires Admin password.";
        draw_admin_password_prompt_modal(prompt_msg);
    } else if (current_modal == Modal::CompareFilesPrompt) {
        draw_compare_files_modal();
    }

    // --- Main Window Setup ---
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGuiWindowFlags root_panel_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                        ImGuiWindowFlags_MenuBar;

    ImGui::Begin("RootCanvas", nullptr, root_panel_flags);

    float accumulated_chrome_height = ImGui::GetStyle().WindowPadding.y * 2;

    // --- Menu Bar ---
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit", "Cmd+Q")) { glfwSetWindowShouldClose(window, GLFW_TRUE); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Navigation")) {
            if (ImGui::MenuItem("Main Menu", "Cmd+M")) { go_to_screen(Screen::MainMenu); }
            if (ImGui::MenuItem("Encrypt File", "Cmd+E")) { go_to_screen(Screen::Encrypt); }
            if (ImGui::MenuItem("Decrypt File", "Cmd+D")) { go_to_screen(Screen::Decrypt); }
            if (ImGui::MenuItem("Verify Encrypted File", "Cmd+V")) {
                current_modal = Modal::CompareFilesPrompt;
                compare_modal_vault_filename_buf[0] = '\0';
                compare_modal_external_enc_filepath_buf[0] = '\0';
                compare_modal_pegs_value = MIN_PEG;
                gui_message.clear();
            }
            ImGui::Separator();
            // Lambda to simplify creating menu items that require admin access
            auto AdminRestrictedMenuItem = [&](const char* label, Screen target_screen, const char* shortcut = nullptr) {
                if (ImGui::MenuItem(label, shortcut)) {
                    if (!admin_access_granted) {
                         request_admin_access_for_screen(target_screen);
                    } else {
                        go_to_screen(target_screen);
                    }
                }
            };
            AdminRestrictedMenuItem("Retrieve Original File", Screen::GetItem, "Cmd+R");
            AdminRestrictedMenuItem("View History", Screen::History, "Cmd+H");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Admin")) {
            if (admin_access_granted) {
                if (ImGui::MenuItem("Logout Admin")) {
                    admin_access_granted = false;
                    set_main_gui_message("Admin logged out.", MSG_COLOR_INFO);
                    if (current_screen == Screen::History || current_screen == Screen::GetItem) {
                        go_to_screen(Screen::MainMenu);
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

    // --- GUI Message Area ---
    ImGui::Spacing();
    accumulated_chrome_height += ImGui::GetStyle().ItemSpacing.y;
    if (!gui_message.empty()) {
        ImVec2 msg_text_size = ImGui::CalcTextSize(gui_message.c_str(), nullptr, true, ImGui::GetContentRegionAvail().x);
        ImGui::PushStyleColor(ImGuiCol_Text, gui_message_color);
        ImGui::TextWrapped("%s", gui_message.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();
        accumulated_chrome_height += msg_text_size.y + ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    }

    // --- Main Content Area ---
    ImVec2 content_size;
    if (current_modal == Modal::None) {
        switch (current_screen) {
            case Screen::MainMenu:
                content_size = draw_main_menu_screen();
                break;
            case Screen::Encrypt:
                content_size = draw_encrypt_decrypt_screen(true); // Encrypt Mode
                break;
            case Screen::Decrypt:
                content_size = draw_encrypt_decrypt_screen(false); // Decrypt Mode
                break;
            case Screen::GetItem:
                content_size = draw_get_item_screen();
                break;
            case Screen::Compare:
                // This screen is now effectively just a placeholder, as a modal is used
                ImGui::TextWrapped("File comparison is handled via the modal dialog under 'Navigation -> Verify Encrypted File'.");
                if (ImGui::Button("Back to Main Menu")) { go_to_screen(Screen::MainMenu); }
                content_size = { 400, 100 };
                break;
            case Screen::History:
                content_size = draw_history_screen();
                break;
            default: // Failsafe
                go_to_screen(Screen::MainMenu);
                content_size = draw_main_menu_screen();
                break;
        }
    } else {
        // Provide a default size when a modal is active to prevent window collapse
        content_size = { MAIN_MENU_MIN_CONTENT_WIDTH, MAIN_MENU_MIN_CONTENT_HEIGHT };
    }

    ImGui::End();
    return {content_size, accumulated_chrome_height};
}

void UIManager::go_to_screen(Screen screen) {
    if (current_screen != screen) {
         gui_message.clear();
    }
    current_screen = screen;

    switch (screen) {
        case Screen::MainMenu:
            admin_access_granted = false; // Logout admin on returning to main menu
            clear_all_persistent_state();
            set_main_gui_message("Welcome to Cipher GUI!", MSG_COLOR_INFO);
            break;
        case Screen::GetItem:
            get_item_filename_buf[0] = '\0';
            get_item_destination_buf[0] = '\0';
            break;
        case Screen::History:
            if (admin_access_granted) {
                load_history_content();
            }
            break;
        // Other screens don't need special setup
        case Screen::Encrypt:
        case Screen::Decrypt:
        case Screen::Compare:
        default:
            break;
    }
}

void UIManager::set_main_gui_message(const std::string& message, const ImVec4& color) {
    gui_message = message;
    gui_message_color = color;
}

void UIManager::load_history_content() {
    std::ifstream ifs(HISTORY_FILE);
    if (ifs) {
        std::stringstream ss;
        ss << ifs.rdbuf();
        history_content_buf = ss.str();
        if (history_content_buf.empty()) {
            history_content_buf = "History is empty.";
        }
    } else {
        history_content_buf = "Error: Could not open history file: " + HISTORY_FILE;
    }
}

void UIManager::request_admin_access_for_screen(Screen target_screen) {
    screen_requiring_password = target_screen;
    current_modal = Modal::AdminPasswordPrompt;
    gui_message.clear();
}

ImVec2 UIManager::draw_main_menu_screen() {
    float button_width = MAIN_MENU_MIN_CONTENT_WIDTH;
    const float button_height = 35.0f;

    ImGui::TextUnformatted("Main Menu");
    ImGui::Separator();
    ImGui::Dummy({0, 10.0f});

    struct MenuItem {
        const char* label;
        Screen screen_to_open;
        Modal modal_to_open;
        bool requires_admin;
    };

    const MenuItem menu_items[] = {
        {"Encrypt File",           Screen::Encrypt,  Modal::None,                false},
        {"Decrypt File",           Screen::Decrypt,  Modal::None,                false},
        {"Retrieve Original File", Screen::GetItem,  Modal::None,                true},
        {"Verify Encrypted File",  Screen::MainMenu, Modal::CompareFilesPrompt,  false},
        {"View History",           Screen::History,  Modal::None,                true}
    };

    for (const auto& item : menu_items) {
        if (ImGui::Button(item.label, {button_width, button_height})) {
            if (item.requires_admin && !admin_access_granted) {
                request_admin_access_for_screen(item.screen_to_open);
            } else {
                if (item.modal_to_open != Modal::None) {
                    current_modal = item.modal_to_open;
                    if (item.modal_to_open == Modal::CompareFilesPrompt) {
                        compare_modal_vault_filename_buf[0] = '\0';
                        compare_modal_external_enc_filepath_buf[0] = '\0';
                        compare_modal_pegs_value = MIN_PEG;
                    }
                    gui_message.clear();
                } else {
                    go_to_screen(item.screen_to_open);
                }
            }
        }
        ImGui::Dummy({0, 5.0f});
    }

    return {MAIN_MENU_MIN_CONTENT_WIDTH, std::max(MAIN_MENU_MIN_CONTENT_HEIGHT, ImGui::GetCursorPosY())};
}

ImVec2 UIManager::draw_encrypt_decrypt_screen(bool is_encrypt_mode) {
    const char* title = is_encrypt_mode ? "Encrypt File & Vault Original" : "Decrypt File";
    ImGui::TextUnformatted(title);
    ImGui::Separator();

    ImGui::PushItemWidth(-1); // Use full available width
    ImGui::InputTextWithHint("##InputFilePath", "Input File Path", input_file_path_buf, sizeof(input_file_path_buf));
    if (!is_encrypt_mode) {
        ImGui::InputTextWithHint("##OutputFilePath", "Output File Path", output_file_path_buf, sizeof(output_file_path_buf));
    }
    ImGui::InputInt("Pegs", &pegs_value);
    pegs_value = std::clamp(pegs_value, MIN_PEG, MAX_PEG);
    ImGui::PopItemWidth();

    ImGui::Dummy({0, 10.0f});

    float button_width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
    if (ImGui::Button(is_encrypt_mode ? "Encrypt & Vault" : "Decrypt", {button_width, 0})) {
        gui_message.clear();
        std::ostringstream captured_output;
        CerrRedirect redirect(captured_output.rdbuf());
        bool success = false;

        if (is_encrypt_mode) {
            success = encrypt_file(input_file_path_buf, pegs_value);
        } else {
            success = decrypt_file(input_file_path_buf, output_file_path_buf, pegs_value);
        }

        std::string op_msg = captured_output.str();
        if (success) {
            set_main_gui_message(std::string(is_encrypt_mode ? "Encryption" : "Decryption") + " successful!" + (op_msg.empty() ? "" : "\nLog:\n" + op_msg), MSG_COLOR_SUCCESS);
            input_file_path_buf[0] = '\0';
            if (!is_encrypt_mode) output_file_path_buf[0] = '\0';
        } else {
            set_main_gui_message("Operation failed." + (op_msg.empty() ? "" : "\nDetails:\n" + op_msg), MSG_COLOR_ERROR);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Back to Main Menu", {button_width, 0})) {
        go_to_screen(Screen::MainMenu);
    }
    
    return {ENCRYPT_DECRYPT_MIN_CONTENT_WIDTH, std::max(ENCRYPT_DECRYPT_MIN_CONTENT_HEIGHT, ImGui::GetCursorPosY())};
}

ImVec2 UIManager::draw_get_item_screen() {
    ImGui::TextUnformatted("Retrieve Original File from Vault");
    ImGui::Separator();

    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##GetItemFilename", "Filename in Vault (e.g., original.txt)", get_item_filename_buf, sizeof(get_item_filename_buf));
    ImGui::InputTextWithHint("##GetItemDest", "Full Destination Path (e.g., C:\\Users\\user\\Desktop\\retrieved.txt)", get_item_destination_buf, sizeof(get_item_destination_buf));
    ImGui::PopItemWidth();
    ImGui::Dummy({0, 10.0f});

    float button_width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
    if (ImGui::Button("Retrieve File", {button_width, 0})) {
        gui_message.clear();
        if (get_item_filename_buf[0] == '\0' || get_item_destination_buf[0] == '\0') {
            set_main_gui_message("Error: Both fields are required.", MSG_COLOR_ERROR);
        } else {
            std::ostringstream captured_output;
            CerrRedirect redirect(captured_output.rdbuf());
            bool success = retrieve_from_vault(get_item_filename_buf, get_item_destination_buf);
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
    if (ImGui::Button("Back to Main Menu", {button_width, 0})) {
        go_to_screen(Screen::MainMenu);
    }

    return {GET_ITEM_MIN_CONTENT_WIDTH, std::max(GET_ITEM_MIN_CONTENT_HEIGHT, ImGui::GetCursorPosY())};
}

ImVec2 UIManager::draw_history_screen() {
    ImGui::TextUnformatted("Operation History");
    ImGui::Separator();

    float bottom_elements_height = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    ImGui::InputTextMultiline("##HistoryContent", &history_content_buf, {-1, -bottom_elements_height}, ImGuiInputTextFlags_ReadOnly);

    float button_width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
    if (ImGui::Button("Refresh History", {button_width, 0})) {
        load_history_content();
        set_main_gui_message("History refreshed.", MSG_COLOR_INFO);
    }
    ImGui::SameLine();
    if (ImGui::Button("Back to Main Menu", {button_width, 0})) {
        go_to_screen(Screen::MainMenu);
    }

    return {HISTORY_MIN_CONTENT_WIDTH, std::max(HISTORY_MIN_CONTENT_HEIGHT, ImGui::GetCursorPosY())};
}

void UIManager::draw_admin_password_prompt_modal(const std::string& prompt_message) {
    ImGui::OpenPopup("Admin Password Modal");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});

    if (ImGui::BeginPopupModal("Admin Password Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextUnformatted(prompt_message.c_str());
        ImGui::Separator();
        ImGui::Dummy({0, 5.0f});
        ImGui::TextUnformatted("Please enter the admin password:");
        ImGui::PushItemWidth(250);
        bool enter_pressed = ImGui::InputText("##AdminPasswordInput", admin_password_buf, sizeof(admin_password_buf), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::Dummy({0, 10.0f});

        if (enter_pressed || ImGui::Button("Login", {120, 0})) {
            if (check_admin_password(admin_password_buf)) {
                admin_access_granted = true;
                current_modal = Modal::None;
                set_main_gui_message("Admin access granted.", MSG_COLOR_SUCCESS);
                go_to_screen(screen_requiring_password);
                ImGui::CloseCurrentPopup();
            } else {
                set_main_gui_message("Incorrect admin password.", MSG_COLOR_ERROR);
            }
            admin_password_buf[0] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {120, 0})) {
            current_modal = Modal::None;
            gui_message.clear();
            admin_password_buf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void UIManager::draw_compare_files_modal() {
    ImGui::OpenPopup("Compare Encrypted File Modal");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({ENCRYPT_DECRYPT_MIN_CONTENT_WIDTH, 0}, ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Compare Encrypted File Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextUnformatted("Verify Encrypted File");
        ImGui::TextWrapped("This compares an in-memory encryption of a vault file against an existing external encrypted file.");
        ImGui::Separator();
        ImGui::Dummy({0, 5.0f});

        ImGui::PushItemWidth(-1);
        ImGui::InputTextWithHint("##VaultFileModal", "Filename in Vault (e.g., original.txt)", compare_modal_vault_filename_buf, MAX_PATH_LEN);
        ImGui::InputTextWithHint("##ExternalEncFileModal", "Path to External Encrypted File", compare_modal_external_enc_filepath_buf, MAX_PATH_LEN);
        ImGui::InputInt("Pegs Used for Encryption", &compare_modal_pegs_value);
        compare_modal_pegs_value = std::clamp(compare_modal_pegs_value, MIN_PEG, MAX_PEG);
        ImGui::PopItemWidth();
        
        ImGui::Separator();
        ImGui::Dummy({0, 5.0f});

        float button_width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
        if (ImGui::Button("Compare", {button_width, 0})) {
            gui_message.clear();
            std::string vault_filename(compare_modal_vault_filename_buf);
            std::string external_enc_path(compare_modal_external_enc_filepath_buf);

            if (vault_filename.empty() || external_enc_path.empty()) {
                set_main_gui_message("Error: All fields must be provided.", MSG_COLOR_ERROR);
            } else {
                // Use our own path helpers instead of std::filesystem
                std::string vault_file_full_path = path_join(PRIVATE_VAULT_DIR, vault_filename);
                std::string error_msg;
                bool problem = false;

                if (!is_regular_file(vault_file_full_path)) {
                    error_msg += "Error: Vault file not found. ";
                    problem = true;
                }
                if (!is_regular_file(external_enc_path)) {
                    error_msg += "Error: External file not found.";
                    problem = true;
                }

                if (problem) {
                    set_main_gui_message(error_msg, MSG_COLOR_ERROR);
                } else {
                    std::string vault_content = load_file_content_to_string(vault_file_full_path, MAX_TEXT_COMPARE_DISPLAY_CHARS);
                    std::string external_enc_content = load_file_content_to_string(external_enc_path, MAX_TEXT_COMPARE_DISPLAY_CHARS);
                    
                    std::string in_memory_encrypted_content = process_content_caesar(vault_content, compare_modal_pegs_value, true);
                    TextCompareResult res = compare_string_contents(in_memory_encrypted_content, external_enc_content);
                    
                    std::ostringstream result_ss;
                    result_ss.precision(2);
                    result_ss << "Verification Result: Match: " << std::fixed << res.match_percentage << "%.";
                    if (res.first_diff_offset != -1) {
                        result_ss << " First diff at offset: " << res.first_diff_offset << ".";
                    } else if (res.match_percentage >= 99.99f) {
                        result_ss << " Contents appear identical.";
                    }
                    set_main_gui_message(result_ss.str(), (res.match_percentage >= 99.99f) ? MSG_COLOR_SUCCESS : MSG_COLOR_WARNING);
                }
            }
            current_modal = Modal::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {button_width, 0})) {
            current_modal = Modal::None;
            gui_message.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
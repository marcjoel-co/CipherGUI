// Filename: src/ui_manager.cpp
#include "ui_manager.h" 
#include "imgui.h"            // The core ImGui drawing library.
#include "external_editor.h"  // We need to call its functions when the "Open" button is clicked.
#include "diary_manager.h"    // We need to access and modify the diary data.

#include <cstring>   // For C-style string functions like strcpy, strlen.
#include <cctype>    // For character functions like isspace.
#include <ctime>     // For getting the current date to validate against future dates.
#include <cstdio>    // For sscanf to parse the date string.
#include <string>    // For std::string, used in the status message.

// --- COMPILE-TIME SWITCH FOR SHENANIGANS ---
// This is a preprocessor macro. If you change this to 1, the code will use the
// "shenanigans" UI functions instead of the standard ImGui ones. This is a fantastic
// way to toggle experimental or debug features.
#define ENABLE_SHENANIGANS 0

#if ENABLE_SHENANIGANS
    #include "for_shenanigans_ui_demo.h"

    // When shenanigans are enabled, these macros redirect our UI calls to the fun versions.
    #define UI_BUTTON(label, index) shenanigans_ui::ulol(label, index)
    #define UI_BUTTON_SIZED(label, size, index) shenanigans_ui::ulol(label, index)
    #define UI_TABLE_BEGIN(name, cols, flags) shenanigans_ui::ulolTable(name, cols, flags)
    #define UI_TABLE_END() shenanigans_ui::EndTable()
#else
    // When shenanigans are disabled, these macros are just aliases for the normal ImGui functions.
    // This lets us use `UI_BUTTON` everywhere in our code without an `if/else` block each time.
    #define UI_BUTTON(label, index) ImGui::Button(label)
    #define UI_BUTTON_SIZED(label, size, index) ImGui::Button(label, size)
    #define UI_TABLE_BEGIN(name, cols, flags) ImGui::BeginTable(name, cols, flags)
    #define UI_TABLE_END() ImGui::EndTable()
#endif


namespace ui_manager {

// --- UI STATE MANAGEMENT ---
// ImGui is an "immediate mode" library, meaning it redraws everything from scratch each
// frame. It doesn't store state (like which item is selected). So, we must do it ourselves.
// This `UIState` struct acts as the "short-term memory" for our entire UI.
struct UIState {
    // Buffers for the "New Entry" popup text fields.
    char new_date[32] = "YYYY-MM-DD";
    char new_title[128] = "";
    char new_content[9999] = ""; // Note: This is a large buffer for a stack-allocated struct. It's fine for now.
    const char* validation_error = nullptr; // Pointer to an error message string.

    // Flags to control when popups should open. This is the correct ImGui pattern.
    bool open_delete_confirmation_popup = false;
    bool open_delete_all_confirmation_popup = false;

    // State for the main entries table.
    int selected_id = -1;    // The unique ID of the selected entry.
    int selected_index = -1; // The row number of the selected entry.

    // State for the confirmation dialogs.
    int entry_id_to_delete = -1;

    // State for the temporary "toast" notification message.
    std::string status_message;
    float status_message_timer = 0.0f; // Countdown timer.

    // Resets the "New Entry" form.
    void prepareForNew() {
        strcpy(new_date, "YYYY-MM-DD");
        strcpy(new_title, "");
        strcpy(new_content, "");
        validation_error = nullptr;
    }

    // Clears the current table selection.
    void deselect() {
        selected_id = -1;
        selected_index = -1;
    }

    // Sets a status message to be displayed for 3 seconds.
    void setStatus(const std::string& message) {
        status_message = message;
        status_message_timer = 3.0f; 
    }
};

// `static` means there is only ONE instance of this struct for the entire file.
// Its data persists across multiple calls to `draw_ui`, which is how our UI "remembers" things.
static UIState s_ui_state;

// --- HELPER FUNCTIONS ---

// A utility to trim leading/trailing whitespace from user input.
void trimWhitespace(char* str) {
    if (!str || *str == '\0') return;
    char* start = str;
    while (isspace(static_cast<unsigned char>(*start))) start++;
    char* dest = str;
    while (*start != '\0') *dest++ = *start++;
    *dest = '\0';
    char* end = dest - 1;
    while (end >= str && isspace(static_cast<unsigned char>(*end))) end--;
    *(end + 1) = '\0';
}

// --- INPUT VALIDATION LOGIC ---
// This class is a masterpiece of good design. It isolates all the complex validation
// rules into a single, reusable place.
class Validator {
public:
    // The constructor takes all the data it needs to perform its checks.
    Validator(const char* date, const char* title, DiaryManager& diary)
        : m_date(date), m_title(title), m_diary(diary), m_error(nullptr) {}

    // This is the main entry point. It uses a "fluent interface" or "method chaining".
    // Each `check...` function returns a reference to itself, allowing us to chain calls together.
    Validator& checkAll() {
        checkDateFormat()
        .checkDateLogic()
        .checkFutureDate()
        .checkTitle()
        .checkUniqueness();
        return *this;
    }

    // A simple way to see if all checks passed.
    bool isValid() const { return m_error == nullptr; }
    // If validation failed, this returns the specific error message.
    const char* getError() const { return m_error; }

private:
    // This is the key to the fluent interface: if an error has already been found,
    // all subsequent checks are instantly skipped. This is efficient and clean.
    Validator& checkDateFormat() {
        if (m_error) return *this; // Short-circuit if already failed.
        if (sscanf(m_date, "%d-%d-%d", &m_year, &m_month, &m_day) != 3) {
            m_error = "Error: Date format must be YYYY-MM-DD.";
        }
        return *this;
    }

    Validator& checkDateLogic() {
        if (m_error) return *this; // Short-circuit...
        if (m_month < 1 || m_month > 12 || m_day < 1 || m_day > 31) m_error = "Error: Invalid month or day.";
        else if ((m_month == 4 || m_month == 6 || m_month == 9 || m_month == 11) && m_day > 30) m_error = "Error: Invalid day for this month.";
        else if (m_month == 2) {
            bool isLeap = (m_year % 4 == 0 && (m_year % 100 != 0 || m_year % 400 == 0));
            if (m_day > (isLeap ? 29 : 28)) m_error = "Error: Invalid day for February.";
        }
        return *this;
    }

    Validator& checkFutureDate() {
        if (m_error) return *this; // Short-circuit...
        time_t now = time(nullptr);
        tm ltm = *localtime(&now);
        // This logic correctly checks if the entered year/month/day is after the current date.
        if (m_year > 1900 + ltm.tm_year ||
           (m_year == 1900 + ltm.tm_year && m_month > 1 + ltm.tm_mon) ||
           (m_year == 1900 + ltm.tm_year && m_month == 1 + ltm.tm_mon && m_day > ltm.tm_mday)) {
            m_error = "Error: Date cannot be in the future.";
        }
        return *this;
    }

    Validator& checkTitle() {
        if (m_error) return *this; // Short-circuit...
        // After trimming whitespace, a title shouldn't be empty.
        if (strlen(m_title) == 0) m_error = "Error: Title cannot be empty.";
        return *this;
    }

    Validator& checkUniqueness() {
        if (m_error) return *this; // Short-circuit...
        // Asks the DiaryManager if an entry for this date already exists.
        // This is a great example of the UI layer communicating with the data layer.
        if (m_diary.entryExistsOnDate(m_date)) {
            m_error = "Error: An entry for this date already exists.";
        }
        return *this;
    }

    // Private members storing the data needed for validation.
    const char* m_date;
    const char* m_title;
    DiaryManager& m_diary;
    const char* m_error;
    int m_year, m_month, m_day;
};

void drawMainMenuBar(DiaryManager& diary) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Now", "Ctrl+S")) {
                diary.saveDataToFile();
                s_ui_state.setStatus("Data saved successfully!");
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::MenuItem("Populate with Sample Entries")) {
                // Predefined sample data
                const struct { const char* date; const char* title; const char* content; } samples[] = {
                    {"2023-01-15", "First Day of a New Project", "Started working on the CipherGUI project. Feeling optimistic about the progress and challenges ahead."},
                    {"2023-03-22", "A Challenging Bug", "Spent the entire day tracking down a memory leak. Finally found it in the rendering loop. It was a misplaced PopID call."},
                    {"2023-05-01", "Holiday Trip", "Took a short trip to the mountains. The fresh air was exactly what I needed to clear my head."},
                    {"2023-08-11", "Presentation Day", "Presented the project prototype today. The feedback was overwhelmingly positive! All the hard work is paying off."},
                    {"2023-10-26", "Refactoring Old Code", "Decided to refactor the UI manager. It's a lot of work, but it will be worth it for maintainability."}
                };
                
                int added_count = 0;
                for (const auto& sample : samples) {
                    // The validator inside addEntry will prevent duplicates if they already exist
                    if (diary.addEntry(sample.date, sample.title, sample.content)) {
                        added_count++;
                    }
                }
                s_ui_state.setStatus("Added " + std::to_string(added_count) + " sample entries.");
            }

            ImGui::Separator();

            
            if (ImGui::MenuItem("!! Delete All Entries !!")) {
                s_ui_state.open_delete_all_confirmation_popup = true;
            }
            ImGui::EndMenu();
        }
        
        #if ENABLE_SHENANIGANS
        shenanigans_ui::drawShenanigansMenu();
        #endif

        ImGui::EndMenuBar();
    }
}

void drawSelectionActionsToolbar(DiaryManager& diary) {
    if (s_ui_state.selected_id == -1) {
        return;
    }
    int btn_idx = 0; // Index for shenanigans

    ImGui::BeginDisabled(s_ui_state.selected_index == 0);
    if (UI_BUTTON("Move Up", btn_idx++)) {
        diary.moveEntryUp(s_ui_state.selected_id);
        s_ui_state.selected_index--;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(s_ui_state.selected_index >= diary.getEntryCount() - 1);
    if (UI_BUTTON("Move Down", btn_idx++)) {
        diary.moveEntryDown(s_ui_state.selected_id);
        s_ui_state.selected_index++;
    }
    ImGui::EndDisabled();

    ImGui::Separator();
}

void drawDiaryEntriesTable(DiaryManager& diary) {
    int btn_idx = 0; // Index for shenanigans
    if (UI_BUTTON("New Diary Entry", btn_idx++)) {
        s_ui_state.prepareForNew();
        ImGui::OpenPopup("New Entry"); 
    }
    if (s_ui_state.selected_id != -1) {
        ImGui::SameLine();
        if (UI_BUTTON("Clear Selection", btn_idx++)) { s_ui_state.deselect(); }
    }
    ImGui::Separator();
    drawSelectionActionsToolbar(diary);

    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;
    if (UI_TABLE_BEGIN("diary_table", 3, flags)) {
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Title");
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableHeadersRow();

        DiaryEntry* entries = diary.getEntries();
        for (int i = 0; i < diary.getEntryCount(); ++i) {
            ImGui::PushID(entries[i].id);
            ImGui::TableNextRow();

            bool is_selected = (entries[i].id == s_ui_state.selected_id);
            if (is_selected) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_HeaderActive));
            }

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entries[i].date);

            ImGui::TableSetColumnIndex(1);
            if (ImGui::Selectable(entries[i].title, is_selected, ImGuiSelectableFlags_None)) {
                s_ui_state.selected_id = entries[i].id;
                s_ui_state.selected_index = i;
            }

            ImGui::TableSetColumnIndex(2); // Actions column
            if (UI_BUTTON("Open", btn_idx++)) {
                // This function will now BLOCK until the editor is closed.
                external_editor::CertifiedEditor(entries[i]); 
                s_ui_state.setStatus("External editor closed.");

                // Automatically reload after the editor closes.
                if (external_editor::reloadEntry(entries[i])) {
            
                    // After successfully reloading the data into memory,
                    // we must immediately tell the DiaryManager to save
                    // all its data back to the main CSV file.
                    diary.saveDataToFile();

                    s_ui_state.setStatus("Entry updated and saved!");
                } else {
                    s_ui_state.setStatus("Error: Content too long or file was deleted.");
                }
            }
            ImGui::SameLine();
            
    
            if (UI_BUTTON("Delete", btn_idx++)) {
                s_ui_state.entry_id_to_delete = entries[i].id;
                s_ui_state.open_delete_confirmation_popup = true; // Set flag instead of calling OpenPopup
            }
            
            ImGui::PopID();
        }
        UI_TABLE_END();
    }
}

void drawNewEntryPopup(DiaryManager& diary) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("New Entry", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        int btn_idx = 0; // Index for shenanigans
        ImGui::InputText("Date (YYYY-MM-DD)", s_ui_state.new_date, sizeof(s_ui_state.new_date));
        ImGui::InputText("Title", s_ui_state.new_title, sizeof(s_ui_state.new_title));
        ImGui::InputTextMultiline("Chikonent", s_ui_state.new_content, sizeof(s_ui_state.new_content), ImVec2(500, 300));
        
        if (UI_BUTTON_SIZED("Save", ImVec2(120, 0), btn_idx++)) {
            trimWhitespace(s_ui_state.new_title);
            Validator validator(s_ui_state.new_date, s_ui_state.new_title, diary);
            validator.checkAll();
            
            if (validator.isValid()) {
                diary.addEntry(s_ui_state.new_date, s_ui_state.new_title, s_ui_state.new_content);
                s_ui_state.setStatus("New entry created successfully.");
                ImGui::CloseCurrentPopup();
            } else {
                s_ui_state.validation_error = validator.getError();
            }
        }

        if (s_ui_state.validation_error) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), s_ui_state.validation_error);
        }
        
        ImGui::SameLine();
        if (UI_BUTTON_SIZED("Cancel", ImVec2(120, 0), btn_idx++)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void drawStatusMessage() {
    if (s_ui_state.status_message_timer > 0.0f) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 window_pos = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x / 2, viewport->WorkPos.y + viewport->WorkSize.y - 40);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::Begin("Status", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        ImGui::Text("%s", s_ui_state.status_message.c_str());
        ImGui::End();
        s_ui_state.status_message_timer -= ImGui::GetIO().DeltaTime;
    }
}



void draw_ui(DiaryManager& diary) {
    #if ENABLE_SHENANIGANS
    shenanigans_ui::beginShenanigansFrame();
    #endif

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowBgAlpha(1.0f);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar;
    ImGui::Begin("Secretko", nullptr, flags);


    // --- Draw the main content of the window ---
    drawMainMenuBar(diary);
    drawDiaryEntriesTable(diary);

   
    if (s_ui_state.open_delete_confirmation_popup) {
        ImGui::OpenPopup("Confirm Deletion");
        s_ui_state.open_delete_confirmation_popup = false; // Reset the flag
    }

    if (s_ui_state.open_delete_all_confirmation_popup) {
        ImGui::OpenPopup("Confirm Delete All");
        s_ui_state.open_delete_all_confirmation_popup = false; // Reset the flag
    }

    // --- Handle popups that belong to this window ---
    drawNewEntryPopup(diary);

    // 1. Handle the "Confirm Single Deletion" Popup
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Confirm Deletion", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        int btn_idx = 0; // Index for shenanigans
        ImGui::Text("Are you sure you want to permanently delete this entry?");
        ImGui::Separator();
        ImGui::Spacing();

        if (UI_BUTTON_SIZED("Yes, Delete It", ImVec2(120, 0), btn_idx++)) {
            // Deselect if we are deleting the selected item
            if (s_ui_state.entry_id_to_delete == s_ui_state.selected_id) {
                s_ui_state.deselect();
            }
            
            // Perform the actual deletion
            diary.deleteEntry(s_ui_state.entry_id_to_delete);
            
            s_ui_state.setStatus("Entry successfully deleted!");
            s_ui_state.entry_id_to_delete = -1; // Reset state
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (UI_BUTTON_SIZED("Cancel", ImVec2(120, 0), btn_idx++)) {
            s_ui_state.setStatus("Deletion canceled.");
            s_ui_state.entry_id_to_delete = -1; // Reset state
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 2. Handle the "Confirm Delete All" Popup
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Confirm Delete All", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        int btn_idx = 0; // Index for shenanigans
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "WARNING: This is permanent!");
        ImGui::Text("Are you sure you want to delete ALL diary entries?");
        ImGui::Text("This action cannot be undone.");
        ImGui::Separator();

        if (UI_BUTTON_SIZED("Confirm Delete All", ImVec2(150, 0), btn_idx++)) {
            diary.deleteAllEntries(); // This should call saveDataToFile() internally
            s_ui_state.deselect();
            s_ui_state.setStatus("All entries have been deleted.");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (UI_BUTTON_SIZED("Cancel", ImVec2(120, 0), btn_idx++)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();

    // --- Draw global overlays ---
    drawStatusMessage();
}

} // namespace ui_manager
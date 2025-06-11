// Filename: src/external_editor.cpp
//
// This module handles the "Edit in External App" feature. It manages creating,
// launching, and cleaning up temporary text files that allow users to edit
// diary content in their preferred text editor (like Notepad or VS Code).

#include "external_editor.h"
#include <string>
#include <fstream>
#include <cstdlib>   // For system()
#include <cstdio>    // For remove()
#include <cstring>   // For strncpy

namespace external_editor 
{




std::string getTempFilename(int id) 
{
    return "temp_entry_" + std::to_string(id) + ".txt";
}

// Launches the operating system's default text editor for the given file.
void launchEditor(const std::string& filename) {
    // The "start" command on Windows is a robust way to open a file with its
    std::string command = "start /wait \"\" \"" + filename + "\"";

    // Now, this system() call will pause your entire application until you close the editor.
    system(command.c_str());
}

// --- PUBLIC FUNCTIONS ---

// This is the main function called when the user clicks "Open".
// It creates the temp file and launches the editor.
void CertifiedEditor(const DiaryEntry& entry) {
    std::string filename = getTempFilename(entry.id);
    std::ofstream file(filename);
    if (file.is_open()) {
        file << entry.content; // Write the current content to the temp file.
        file.close();          // Close our file handle so the editor can open it.
        launchEditor(filename);
    }
}

// This function should be called to bring the changes from the temp file back
// into the application. It is now the ONLY function responsible for deleting the temp file.
// In src/external_editor.cpp


bool reloadEntry(DiaryEntry& entry) {
    std::string filename = getTempFilename(entry.id);
    printf("DEBUG: Attempting to reload from file: %s\n", filename.c_str());

    std::ifstream file(filename);
    if (!file.is_open()) {
        printf("DEBUG: reloadEntry FAILED. File could not be opened.\n");
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    printf("DEBUG: File read successfully. Content length: %zu, Buffer capacity: %zu\n", content.length(), sizeof(entry.content));


    if (content.length() >= sizeof(entry.content)) {
        printf("FAILED. Content too long  in the buffer.\n");
     
        return false;
    }

   
    strncpy(entry.content, content.c_str(), sizeof(entry.content) - 1);
    entry.content[sizeof(entry.content) - 1] = '\0';

    printf("DEBUG: Content updated in memory. Attempting to delete the temp file...\n");


    if (remove(filename.c_str()) != 0) {

        printf("DEBUG: remove() FAILED! The OS could not delete the file. It might be locked.\n");
        
    } else {
        printf("DEBUG: remove() SUCCEEDED. The temp file has been deleted.\n");
    }
    
    return true;
}

    void cleanupAllTempFiles() {

        
        system("del /q temp_entry_*.txt > nul 2>&1");

    }
} 



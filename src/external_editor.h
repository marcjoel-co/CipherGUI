#pragma once

#include "diary_manager.h" // For the DiaryEntry struct definition

// This namespace encapsulates all logic for interacting with an external text editor.
namespace external_editor {

    /**
     * @brief Creates a temporary file with the entry's content and opens it
     *        in the system's default text editor.
     * @param entry The diary entry to open.
     */
    void CertifiedEditor(const DiaryEntry& entry);

    /**
     * @brief Reads the content from the temporary file, updates the entry in memory,
     *        and deletes the temporary file.
     * @param entry A reference to the DiaryEntry to update.
     * @return True reload was successful ? false otherwise.
     */
    bool reloadEntry(DiaryEntry& entry);

    /**
     * @brief Scans for and deletes all temp_entry_*.txt files in the current directory.
     *        Designed to be called on application shutdown as a failsafe cleanup.
     */
    void cleanupAllTempFiles();
} 

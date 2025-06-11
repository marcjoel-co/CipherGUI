// Filename: src/ui_manager.h

// This is a "header guard". It's the classic C-style way to prevent this file
// from being included multiple times. It does the same thing as `#pragma once`.
// If `UI_MANAGER_H` hasn't been defined yet, it defines it and includes the code.
// If it has been defined, it skips everything until `#endif`.
#ifndef UI_MANAGER_H
#define UI_MANAGER_H

// --- DEPENDENCIES ---
// This is a crucial line. We are telling the compiler that our UI code needs to know
// what a `DiaryManager` is. We need its blueprint (`diary_manager.h`) because our
// `draw_ui` function takes a `DiaryManager` object as a parameter.
#include "diary_manager.h" 

// --- NAMESPACE ---
// A namespace is like a folder for your code. It prevents naming conflicts.
// If another library also had a function called `draw_ui`, we could distinguish ours
// by calling `ui_manager::draw_ui`. It's great for organization.
namespace ui_manager {

    // --- FUNCTION DECLARATION ---
    
    /**
     * @brief The single public function for this module. It's responsible for drawing
     *        the entire user interface for one frame.
     * @param diary A reference to the application's main data object. This gives
     *              the UI read/write access to the diary entries.
     */
    void draw_ui(DiaryManager& diary);

}

#endif // UI_MANAGER_H
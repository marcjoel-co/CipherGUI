// Filename: src/diary_manager.h

// This is a "header guard".
#pragma once


#include <string>


struct DiaryEntry
{
    int id;             
    char date[32];      // The date of the entry,
    char title[128];    // The title of the entry.
    char content[9999]; // The main text content of the entry.
};

// --- Class Definition ---
// Why class ? because OOP; for encaptulation and things 
class DiaryManager

{
public: 
    // called automatically.
    DiaryManager();  // The Constructor:  for setup.
    ~DiaryManager(); // The Destructor: for cleanup.

    // --- Public Interface for Data Manipulation ---
    // These are the "action" functions. They change the data.

    /**
     * @brief Adds a new, empty entry for a given date.
     * @return true if the entry was added, false otherwise (e.g., date duplicate).
     */
    bool addEntry(const char *date, const char *title, const char *content);

    /**
     * @brief Deletes an entry from the collection using its unique ID.
     * @return true if the entry was found and deleted, false otherwise.
     */
    bool deleteEntry(int id);

    /**
     * @brief Moves an entry one position "up" in the list (towards the start).
     * @return true if the entry was found and moved, false otherwise.
     */
    bool moveEntryUp(int id);

    /**
     * @brief Moves an entry one position "down" in the list (towards the end).
     * @return true if the entry was found and moved, false otherwise.
     */
    bool moveEntryDown(int id);

    /** @brief Deletes all entries from memory, effectively resetting the manager. */
    void deleteAllEntries();

    // --- Public Interface for Data Querying ---
    // These are the "getter" functions. They retrieve data without changing it.
    // The `const` at the end promises that these functions won't modify the class's state.

    // Returns a pointer to the beginning of the array of entries.
    DiaryEntry *getEntries() const { return entries; }
    // Returns how many entries are currently stored.
    int getEntryCount() const { return entry_count; }
    // Finds a specific entry by its ID and returns a pointer to it, or nullptr if not found.
    DiaryEntry *getEntryById(int id) const;
    // Checks if an entry for a specific date already exists.
    bool entryExistsOnDate(const char *date) const;

    // --- Public Interface for File Operations ---
    // These functions handle persistence (saving data so it's not lost when the app closes).
    void saveDataToFile();
    void loadDataFromFile();

private: // "private" means these variables and functions can ONLY be used by the DiaryManager itself.
        
    void resize_entry_array();        // to increase the size of the `entries` array when it gets full.
    int findEntryIndex(int id) const; //  find the array position (index) of an entry with a given ID.

    // --- Private Member Variables ---
    // This is the data that each DiaryManager object "owns".

    DiaryEntry *entries;                           // This is where we will store all our DiaryEntry
    int entry_count;                               // The current number of diary entries we are storing.
    int entry_capacity;                            // The total number of entries the `entries` array can currently hold before it needs to be resized.
    int next_id;                                   // A counter to ensure every new entry gets a unique ID.
    const std::string filename = "diary_data.csv"; // The name of the file used for saving and loading. `const` means it can't be changed.
};
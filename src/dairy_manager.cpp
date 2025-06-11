// src
#include "diary_manager.h"
#include "external_editor.h"

#include <fstream>
#include <iostream>
#include <cstring>
#include <utility>
#include <sstream>
#include <algorithm>

/**
 * @brief Safely prepares a string for writing to a CSV file by applying quoting and escaping.
 * @details In short, it
 *          wraps fields with commas/quotes in double-quotes and escapes internal quotes.
 * @param field The raw text to be escaped.
 * @return A std::string that is safe to be written to a CSV file.
 */
std::string escapeCsvField(const char *field)
{
    std::string str_field(field);
    // If the field contains any special character
    if (str_field.find(',') != std::string::npos ||
        str_field.find('"') != std::string::npos ||
        str_field.find('\n') != std::string::npos)
    {
        std::string result = "\""; // Start with a quote.
        // Loop over each character to check for quotes that need escaping.
        for (char c : str_field)
        {
            if (c == '"')
                result += "\"\""; // An inner quote becomes two quotes.
            else
                result += c;
        }
        result += "\""; // End with a quote.
        return result;
    }
    // If no special characters, return the original string for efficiency.
    return str_field;
}

/**
 * @brief Parses a single line from a CSV file directly into a DiaryEntry struct.
 * @details This is more efficient than creating a temporary vector of strings. It handles
 *          both quoted and unquoted fields.
 * @param line The full line of text read from the CSV file.
 * @param out_entry A reference to a DiaryEntry object that this function will fill.
 * @return true if the line was successfully parsed into 4 fields, false otherwise.
 */
bool parseCsvLineToEntry(const std::string &line, DiaryEntry &out_entry)
{
    std::stringstream ss(line); // Use a stringstream for easy parsing.
    std::string field;
    int field_index = 0;

    // Loop as long as there's content in the line and we haven't read all 4 fields yet.
    while (ss.good() && field_index < 4)
    {
        char c = ss.peek();
        // Iquote mode
        if (c == '"')
        {
            ss.get();
            field.clear();
            while (ss.good()) // if stream is
            {
                char current_char = ss.get();

                if (current_char == '"')
                {
               
                    if (ss.peek() == '"') //"John Doe,""123 Main St."",456-7890"`
                    {
                        field += '"'; // Add a single quote to our field.
                        ss.get();     // And consume the second quote from the stream.
                    }
                    else
                    {
                        break; // Otherwise, it's the end of the quoted field.
                    }
                }
                else
                {
                    field += current_char;
                }
            }
            // After the quoted field, consume the following comma.
            if (ss.peek() == ',')
                ss.get();
        }
        else
        {
            // It's an unquoted field, so just read until the next comma.
            std::getline(ss, field, ',');
        }

        // Now that we have the 'field' string, put its value into the correct place in our struct.
        switch (field_index)
        {
        case 0: // ID
            try
            {
                out_entry.id = std::stoi(field);
            }
            catch (...)
            {
                return false; // If ID is not a number, the line is invalid
            }
            break;
        case 1: // Date
            strncpy(out_entry.date, field.c_str(), sizeof(out_entry.date) - 1);
            out_entry.date[sizeof(out_entry.date) - 1] = '\0';
            break;
        case 2: // Title
            strncpy(out_entry.title, field.c_str(), sizeof(out_entry.title) - 1);
            out_entry.title[sizeof(out_entry.title) - 1] = '\0';
            break;
        case 3: // Content
            strncpy(out_entry.content, field.c_str(), sizeof(out_entry.content) - 1);
            out_entry.content[sizeof(out_entry.content) - 1] = '\0';
            break;
        }
        field_index++; // Move on to the next field.
    }
    // A line is only valid if we successfully parsed exactly 4 fields.
    return field_index == 4;
}

// The Constructor: runs when an object is created.
DiaryManager::DiaryManager() : entries(nullptr), entry_count(0), entry_capacity(0), next_id(1)
{
    // 1. Try to load any existing data from the disk.
    loadDataFromFile();
    // 2. If no data was loaded (e.g., first time running), allocate a small initial array.
    if (entry_count == 0)
    {
        entry_capacity = 8;
        entries = new DiaryEntry[entry_capacity];
    }
}

// The Destructor: This runs once when the DiaryManager object is destroyed (app closes).
DiaryManager::~DiaryManager()
{
    // 1. Save all current data to disk so no work is lost.
    saveDataToFile();
    // 2. Free the memory we allocated for the entries array to prevent memory leaks.
    delete[] entries;
    external_editor::cleanupAllTempFiles();
}

bool DiaryManager::addEntry(const char *date, const char *title, const char *content)
{
    if (entryExistsOnDate(date))
        return false; //  No duplicate dates.
    if (entry_count >= entry_capacity)
    {
        resize_entry_array(); // If our array is full, make it bigger.
    }
    DiaryEntry &new_entry = entries[entry_count]; // Get the next empty slot.
    new_entry.id = next_id++;                     // Assign a new unique ID.
    // Safely copy data into the new entry.
    strncpy(new_entry.date, date, sizeof(new_entry.date) - 1);
    strncpy(new_entry.title, title, sizeof(new_entry.title) - 1);
    strncpy(new_entry.content, content, sizeof(new_entry.content) - 1);

    new_entry.date[sizeof(new_entry.date) - 1] = '\0';
    new_entry.title[sizeof(new_entry.title) - 1] = '\0';
    new_entry.content[sizeof(new_entry.content) - 1] = '\0';
    entry_count++;
    return true;
}

bool DiaryManager::deleteEntry(int id)
{
    int index = findEntryIndex(id); // Find the entry to delete.
    if (index != -1)
    { // If found...
        // ...shift all subsequent entries one position to the left, overwriting the deleted one.
        for (int i = index; i < entry_count - 1; ++i)
        {
            entries[i] = entries[i + 1];
        }
        entry_count--;    // Decrement our total count of entries.
        saveDataToFile(); // Persist this change immediately.
        return true;
    }
    return false;
}

bool DiaryManager::moveEntryUp(int id)
{
    int index = findEntryIndex(id);
    if (index > 0)
    {                                                  // Can't move up if it's the first one.
        std::swap(entries[index], entries[index - 1]); // Swap it with the one before it.
        return true;
    }
    return false;
}

bool DiaryManager::moveEntryDown(int id)
{
    int index = findEntryIndex(id);
    if (index != -1 && index < entry_count - 1)
    {                                                  // Can't move down if it's the last one.
        std::swap(entries[index], entries[index + 1]); // Swap it with the one after it.
        return true;
    }
    return false;
}

void DiaryManager::deleteAllEntries()
{
    // Before wiping the data, clean up any temp files from the external editor.
    entry_count = 0;  // Reset the counter to zero.
    next_id = 1;      // Reset the ID generator.
    saveDataToFile(); // Save an empty state to the file, effectively deleting all data.
}

DiaryEntry *DiaryManager::getEntryById(int id) const
{
    int index = findEntryIndex(id);
    return (index != -1) ? &entries[index] : nullptr;
}

bool DiaryManager::entryExistsOnDate(const char *date) const
{
    for (int i = 0; i < entry_count; ++i)
    {
        if (strcmp(entries[i].date, date) == 0)
        {
            return true; // Found a match.
        }
    }
    return false; // No match found.
}

//
int DiaryManager::findEntryIndex(int id) const
{
    for (int i = 0; i < entry_count; ++i)
    {
        if (entries[i].id == id)
            return i;
    }
    return -1; // Sentinel value for "not found".
}

//  manual dynamic array.
void DiaryManager::resize_entry_array()
{
    // Calculate a new, larger capacity (usually double).
    entry_capacity = (entry_capacity == 0) ? 8 : entry_capacity * 2;
    // Allocate a  larger array.
    DiaryEntry *new_entries = new DiaryEntry[entry_capacity];
    // Copy all elements from the old array to the new one.
    for (int i = 0; i < entry_count; ++i)
    {
        new_entries[i] = entries[i];
    }
    // small array to free its memory.
    delete[] entries;
    //`entries`larger array.
    entries = new_entries;
}

void DiaryManager::saveDataToFile()
{
    std::ofstream file(filename);
    if (!file.is_open())
        return;

    file << "id,date,title,content\n"; // Write the CSV header.
    for (int i = 0; i < entry_count; ++i)
    {
        // For each entry, write a line, .
        file << entries[i].id << ","
             << escapeCsvField(entries[i].date) << ","
             << escapeCsvField(entries[i].title) << ","
             << escapeCsvField(entries[i].content) << "\n";
    }
    file.close();
}

// The loading method.
void DiaryManager::loadDataFromFile()
{
    std::ifstream file(filename);
    if (!file.is_open())
        return; // File doesn't exist yet, nothing to load.

    std::string line;

    int entry_line_count = 0;
    std::getline(file, line);
    while (std::getline(file, line))
    {
        if (!line.empty())
        {
            entry_line_count++;
        }
    }

    if (entry_line_count == 0)
    {
        file.close();
        return;
    }
    entry_count = entry_line_count;
    entry_capacity = entry_line_count;
    entries = new DiaryEntry[entry_capacity];

    // Rewind the file and read the data into our new array.
    file.clear();                 // Reset error flags (like end-of-file).
    file.seekg(0, std::ios::beg); // Go back to the beginning of the file.
    std::getline(file, line);     // Skip head

    int current_index = 0;
    int max_id = 0;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;
        // Parse the line directly into the array at the current position.
        if (parseCsvLineToEntry(line, entries[current_index]))
        {
            //
            max_id = std::max(max_id, entries[current_index].id);
            current_index++; 
        }
    }
    file.close();

    // The final count is how many lines sumakses
    entry_count = current_index;
    next_id = max_id + 1;
}

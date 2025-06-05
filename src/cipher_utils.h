#ifndef CIPHER_UTILS_H
#define CIPHER_UTILS_H

#include <string>
#include <vector>
#include <filesystem> // For std::filesystem::path in declarations
#include <cstddef>    // For size_t

// Note: <iostream> removed as it's not typically needed for declarations.
// Add it back ONLY if a function declaration here *must* use std::cout/cerr (e.g., default arg).

namespace cipher_utils {

    // --- Constants ---
    constexpr int MIN_PEG = 1;
    constexpr int MAX_PEG = 255;
    constexpr size_t BUFFER_SIZE = 4096;
    constexpr size_t MAX_FILENAME_BUFFER_SIZE = 260; // If used for C-style array dimensions

    // Declare std::string constants as extern; define them in cipher_utils.cpp
    extern const std::string HISTORY_FILE;
    extern const std::string PRIVATE_VAULT_DIR;
    extern const std::string ADMIN_PASSWORD; // Consider a more secure storage/handling method later

    // --- Structures ---
    struct OperationParams {
        std::string input_file;
        std::string output_file;
        int pegs;
    };

    struct ValidationFlags {
        bool check_input_file = true;
        bool check_output_file = true;
        bool check_pegs = true;
        bool ensure_output_different_from_input = true;
        // This struct is a "literal type" allowing it to be constexpr
    };

    // Use inline constexpr (C++17) for struct constants defined in headers
    inline constexpr ValidationFlags DEFAULT_ENCRYPT_DECRYPT_FLAGS = {true, true, true, true};
    inline constexpr ValidationFlags DEFAULT_DECRYPT_FLAGS_NO_INPUT_CHECK = {false, true, true, true};

    // --- Validation Functions Declarations ---
    bool has_txt_extension(const std::string& filename);
    bool validate_input_file(const std::string& filename);
    // validate_output_file is an internal helper, so not declared here.
    bool validate_derived_output_file(const std::string& derived_output_filename, const std::string& input_filename);
    bool validate_destination_path(const std::string& destination_path, const std::string& source_filename_in_vault);
    bool validate_peg_value(int peg);
    bool validate_operation_parameters(const OperationParams& params, const ValidationFlags& flags);

    // If the old validate_encryption_params is still called by the GUI, declare it:
    // bool validate_encryption_params(const OperationParams& params); // <<<< ADD THIS IF GUI NEEDS IT

    bool validate_encryption_params_new(const std::string& input_file, int pegs);
    bool validate_decryption_params(const OperationParams& params); // This one was already declared

    // --- Core Cipher Operations Declarations ---
    // process_file_core is an internal helper, so not declared here unless you intend it to be public.
    // Based on its usage, it seems internal. If jay_gui needs to call it, add its declaration.
    // bool process_file_core(const std::string& input_file, const std::string& output_file, int pegs, bool encrypt_mode);

    bool encrypt_file(const std::string& input_file, int pegs); // New version
    bool decrypt_file(const std::string& input_file, const std::string& output_file, int pegs);
    bool move_to_vault(const std::string& original_filepath);
    bool retrieve_from_vault(const std::string& filename_in_vault, const std::string& destination_path);

    // --- History and Logging Declarations ---
    void log_operation(const std::string& operation_type, const std::string& input_file, const std::string& output_file, int pegs);
    void log_event(const std::string& event_type, const std::string& details);

    // --- Admin/Security Declarations ---
    bool check_admin_password(const std::string& password_attempt);
    bool ensure_private_vault_exists();

    // --- Console Input Declarations (if keeping for CLI/debug) ---
    bool safe_console_input(std::string& buffer);
    void clear_console_stdin(void);

    // --- Comparison Result Structures ---
    struct TextCompareResult {
        bool files_readable = false;
        std::string content1;
        std::string content2;
        float match_percentage = 0.0f;
        long first_diff_offset = -1; // Consider std::ptrdiff_t if it represents byte difference
        std::string error_message;
    };

    struct BinaryCompareResult {
        bool file1_exists = false;
        bool file2_exists = false;
        uintmax_t file1_size = 0; // Use uintmax_t for file sizes (from std::filesystem::file_size)
        uintmax_t file2_size = 0;
        std::string file1_hash;
        std::string file2_hash;
        bool hashes_match = false;
        bool sizes_match = false;
        std::string error_message_file1;
        std::string error_message_file2;
    };

    
    TextCompareResult compare_text_files(const std::string& filepath1, const std::string& filepath2, size_t max_chars_to_load = 100000);
    BinaryCompareResult compare_binary_files(const std::string& filepath1, const std::string& filepath2);
    std::string calculate_sha256(const std::string& filepath);

    TextCompareResult compare_string_contents(const std::string& content1, const std::string& content2,
                                              const std::string& label1 = "Content 1",
                                              const std::string& label2 = "Content 2");
    std::string process_content_caesar(const std::string& content, int pegs, bool encrypt_mode);
    std::string load_file_content_to_string(const std::string& filepath, size_t max_chars_to_load = 1000000);

} 

#endif // CIPHER_UTILS_H
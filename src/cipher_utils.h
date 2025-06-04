#ifndef CIPHER_UTILS_H
#define CIPHER_UTILS_H

#include <string>
#include <vector>
#include <iostream>   // For std::cerr, std::cout, std::endl
#include <fstream>    // For std::ifstream, std::ofstream
#include <filesystem> // For std::filesystem::path, exists, etc.
#include <chrono>     // For std::chrono::system_clock, etc.
#include <iomanip>    // For std::put_time
#include <limits>     // For std::numeric_limits
#include <system_error> // For std::error_code

namespace cipher_utils {

    // --- Constants ---
    const int MAX_FILENAME_BUFFER_SIZE = 256;       // Still useful for char[] buffers in GUI, though string is preferred internally
    const int BUFFER_SIZE = 4096;       // For file processing buffer
    const int MIN_PEG = 0;
    const int MAX_PEG = 255;
    const std::string HISTORY_FILE = "history.md";

    // --- Structs ---
    struct OperationParams {
        std::string input_file;
        std::string output_file;
        int pegs;
        std::string username; // Retained for future use if needed
        std::string password; // Retained for future use if needed
    };

    struct ValidationFlags {
        bool check_input_file = false;
        bool check_output_file = false;
        bool ensure_output_different_from_input = false;
        bool check_pegs = false;
        // bool check_credentials = false; // Retained if you plan to add auth back
    };

    // Default validation flags
    const ValidationFlags DEFAULT_ENCRYPT_DECRYPT_FLAGS = {true, true, true, true};
    // const ValidationFlags DEFAULT_DECRYPT_FLAGS = {true, true, true, true, true}; // If you add auth


    // --- Function Declarations ---

    // Helpers
    bool has_txt_extension(const std::string& filename);

    // Validation
    bool validate_input_file(const std::string& filename);
    bool validate_output_file(const std::string& output_filename, const std::string& input_filename_for_comparison = ""); // Optional second param
    bool validate_peg_value(int peg);
    bool validate_operation_parameters(const OperationParams& params, const ValidationFlags& flags);
    bool validate_encryption_params(const OperationParams& params);
    bool validate_decryption_params(const OperationParams& params);
    // If you had validate_decryption_with_auth_params, declare it here if keeping.

    // Core Operations
    bool process_file_core(const std::string& input_file, const std::string& output_file, int pegs, bool encrypt_mode);
    bool encrypt_file(const std::string& input_file, const std::string& output_file, int pegs);
    bool decrypt_file(const std::string& input_file, const std::string& output_file, int pegs);

    // History and Logging
    void log_operation(const std::string& operation_type, const std::string& input_file, const std::string& output_file, int pegs);

    // Console Input (if still used anywhere or for testing, otherwise can be removed)
    bool safe_console_input(std::string& buffer);
    void clear_console_stdin(void);

    // User Authentication (declare if you re-add it)
    // struct User { ... };
    // bool authenticate_user(const std::string& username, const std::string& password);

} // namespace cipher_utils

#endif // CIPHER_UTILS_H
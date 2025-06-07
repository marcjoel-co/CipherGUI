#pragma once

#include <string>
#include <vector>
#include <cstddef> // For size_t

// --- Constants ---
constexpr int MIN_PEG = 1;
constexpr int MAX_PEG = 255;
constexpr size_t BUFFER_SIZE = 4096;
constexpr size_t MAX_FILENAME_BUFFER_SIZE = 260;

extern const std::string HISTORY_FILE;
extern const std::string PRIVATE_VAULT_DIR;
extern const std::string ADMIN_PASSWORD;

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
};

inline constexpr ValidationFlags DEFAULT_ENCRYPT_DECRYPT_FLAGS = {true, true, true, true};
inline constexpr ValidationFlags DEFAULT_DECRYPT_FLAGS_NO_INPUT_CHECK = {false, true, true, true};

struct TextCompareResult {
    bool files_readable = false;
    std::string content1;
    std::string content2;
    float match_percentage = 0.0f;
    long first_diff_offset = -1;
    std::string error_message;
};

struct BinaryCompareResult {
    bool file1_exists = false;
    bool file2_exists = false;
    unsigned long long file1_size = 0;
    unsigned long long file2_size = 0;
    std::string file1_hash;
    std::string file2_hash;
    bool hashes_match = false;
    bool sizes_match = false;
    std::string error_message_file1;
    std::string error_message_file2;
};

// --- Public Function Declarations ---

// Publicly accessible Filesystem Helpers
std::string path_join(const std::string& p1, const std::string& p2);
bool is_regular_file(const std::string& path);
bool is_directory(const std::string& path);
bool create_directory(const std::string& path);
std::string path_get_filename(const std::string& path);
std::string path_get_parent(const std::string& path);

// Validation Functions
bool has_txt_extension(const std::string& filename);
bool validate_input_file(const std::string& filename);
bool validate_peg_value(int peg);
bool validate_operation_parameters(const OperationParams& params, const ValidationFlags& flags);
bool validate_encryption_params_new(const std::string& input_file, int pegs);
bool validate_decryption_params(const OperationParams& params);

// Core Cipher Operations
bool encrypt_file(const std::string& input_file, int pegs);
bool decrypt_file(const std::string& input_file, const std::string& output_file, int pegs);
bool move_to_vault(const std::string& original_filepath);
bool retrieve_from_vault(const std::string& filename_in_vault, const std::string& destination_path);

// History and Logging
void log_operation(const std::string& operation_type, const std::string& input_file, const std::string& output_file, int pegs);
void log_event(const std::string& event_type, const std::string& details);

// Admin/Security
bool check_admin_password(const std::string& password_attempt);
bool ensure_private_vault_exists();

// File/Content Comparison and Processing
TextCompareResult compare_text_files(const std::string& filepath1, const std::string& filepath2, size_t max_chars_to_load = 100000);
BinaryCompareResult compare_binary_files(const std::string& filepath1, const std::string& filepath2);
std::string calculate_sha256(const std::string& filepath);
TextCompareResult compare_string_contents(const std::string& content1, const std::string& content2, const std::string& label1 = "Content 1", const std::string& label2 = "Content 2");
std::string process_content_caesar(const std::string& content, int pegs, bool encrypt_mode);
std::string load_file_content_to_string(const std::string& filepath, size_t max_chars_to_load = 1000000);
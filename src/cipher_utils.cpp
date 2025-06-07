#include "cipher_utils.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstdio> // For std::rename, std::remove

// OpenSSL for SHA256 hashing
#include <openssl/evp.h>
#include <openssl/err.h>

// Platform-specific includes for directory operations
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #include <direct.h> // For _mkdir
#else
    #include <sys/stat.h> // For stat, mkdir
#endif

// --- Definitions for Global Constants ---
const std::string HISTORY_FILE = "history.md";
const std::string PRIVATE_VAULT_DIR = ".private_vault";
const std::string ADMIN_PASSWORD = "supersecretpassword123";

// --- Publicly Available Filesystem Helpers (Implementations) ---

#if defined(_WIN32) || defined(_WIN64)
    const char PATH_SEPARATOR = '\\';
#else
    const char PATH_SEPARATOR = '/';
#endif

std::string path_join(const std::string& p1, const std::string& p2) {
    if (p1.empty()) return p2;
    if (p2.empty()) return p1;
    if (p1.back() == '/' || p1.back() == '\\') {
        return p1 + p2;
    }
    return p1 + PATH_SEPARATOR + p2;
}

std::string path_get_parent(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
}

std::string path_get_filename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

#if defined(_WIN32) || defined(_WIN64)
    bool is_directory(const std::string& path) {
        DWORD attrib = GetFileAttributesA(path.c_str());
        return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
    }
    bool create_directory(const std::string& path) {
        return _mkdir(path.c_str()) == 0;
    }
#else // For macOS, Linux, etc.
    bool is_directory(const std::string& path) {
        struct stat info;
        if (stat(path.c_str(), &info) != 0) return false;
        return (info.st_mode & S_IFDIR) != 0;
    }
    bool create_directory(const std::string& path) {
        return mkdir(path.c_str(), 0755) == 0;
    }
#endif

bool file_exists(const std::string& path) {
    std::ifstream f(path.c_str());
    return f.good();
}

bool is_regular_file(const std::string& path) {
    return file_exists(path) && !is_directory(path);
}

// --- Anonymous Namespace for INTERNAL (File-Local) Helper Functions ---
namespace {

    long long get_file_size(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return -1;
        return static_cast<long long>(file.tellg());
    }

    std::string path_get_extension(const std::string& path) {
        std::string filename = path_get_filename(path);
        size_t pos = filename.find_last_of('.');
        return (pos != std::string::npos) ? filename.substr(pos) : "";
    }

    bool manual_copy_file(const std::string& src, const std::string& dest) {
        std::ifstream src_stream(src, std::ios::binary);
        if (!src_stream) return false;
        std::ofstream dest_stream(dest, std::ios::binary | std::ios::trunc);
        if (!dest_stream) return false;
        dest_stream << src_stream.rdbuf();
        return src_stream.good() && dest_stream.good();
    }
    
    bool validate_output_file(const std::string& output_filename, const std::string& input_filename) {
        if (!input_filename.empty() && output_filename == input_filename) {
            std::cerr << "Error (Output): Output file cannot be the same as the input file.\n";
            return false;
        }
        std::string parent_dir = path_get_parent(output_filename);
        if (!is_directory(parent_dir)) {
            std::cerr << "Error (Output): Directory '" << parent_dir << "' does not exist.\n";
            return false;
        }
        std::string temp_file_path = path_join(parent_dir, "write_check.tmp");
        std::ofstream temp_stream(temp_file_path);
        if (!temp_stream.is_open()) {
            std::cerr << "Error (Output): Cannot write to output directory '" << parent_dir << "'. Check permissions.\n";
            return false;
        }
        temp_stream.close();
        std::remove(temp_file_path.c_str());
        return true;
    }

    bool process_file_core(const std::string& input_file, const std::string& output_file, int pegs, bool encrypt_mode) {
        std::ifstream in(input_file, std::ios::binary);
        if (!in) {
            std::cerr << "Error: Could not open input file: " << input_file << '\n';
            return false;
        }
        std::ofstream out(output_file, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "Error: Could not open output file: " << output_file << '\n';
            return false;
        }
        const char* mode_str = encrypt_mode ? "Encrypting" : "Decrypting";
        std::cout << mode_str << " " << input_file << " -> " << output_file << " (Pegs: " << pegs << ")\n";
        std::vector<unsigned char> buffer(BUFFER_SIZE);
        while (in.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || in.gcount() > 0) {
            size_t bytes_read = static_cast<size_t>(in.gcount());
            for (size_t i = 0; i < bytes_read; ++i) {
                if (encrypt_mode) {
                    buffer[i] = static_cast<unsigned char>((buffer[i] + pegs) % 256);
                } else {
                    buffer[i] = static_cast<unsigned char>((buffer[i] - pegs + 256) % 256);
                }
            }
            if (!out.write(reinterpret_cast<const char*>(buffer.data()), bytes_read)) {
                std::cerr << "Error: A write error occurred during processing.\n";
                return false;
            }
        }
        if (in.bad()) {
            std::cerr << "Error: A read error occurred on input file " << input_file << ".\n";
            return false;
        }
        std::cout << "Success: File processing complete.\n";
        log_operation((encrypt_mode ? "ENCRYPT" : "DECRYPT"), input_file, output_file, pegs);
        return true;
    }
    
    void log_to_file(const std::string& message) {
        std::ofstream file(HISTORY_FILE, std::ios::app);
        if (!file) {
            std::cerr << "Warning: Could not open history file '" << HISTORY_FILE << "' for logging.\n";
            return;
        }
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm_obj{};
    #if defined(_WIN32) || defined(_WIN64)
        localtime_s(&now_tm_obj, &now_c);
    #else
        localtime_r(&now_c, &now_tm_obj);
    #endif
        file << std::put_time(&now_tm_obj, "%Y-%m-%d %H:%M:%S") << " | " << message << '\n';
    }

} // End anonymous namespace

// --- Public Function Implementations ---

// --- Validation Functions ---
bool has_txt_extension(const std::string& filename) {
    return path_get_extension(filename) == ".txt";
}

bool validate_input_file(const std::string& filename) {
    if (!is_regular_file(filename)) {
        std::cerr << "Error (Input): File '" << filename << "' does not exist or is not a regular file.\n";
        return false;
    }
    if (get_file_size(filename) == 0) {
        std::cerr << "Error (Input): File '" << filename << "' is empty.\n";
        return false;
    }
    return true;
}

bool validate_peg_value(int peg) {
    if (peg >= MIN_PEG && peg <= MAX_PEG) {
        return true;
    }
    std::cerr << "Error: Peg value " << peg << " is out of range (" << MIN_PEG << "-" << MAX_PEG << ").\n";
    return false;
}

bool validate_operation_parameters(const OperationParams& params, const ValidationFlags& flags) {
    if (flags.check_input_file && !validate_input_file(params.input_file)) {
        return false;
    }
    if (flags.check_output_file && !validate_output_file(params.output_file, flags.ensure_output_different_from_input ? params.input_file : "")) {
        return false;
    }
    if (flags.check_pegs && !validate_peg_value(params.pegs)) {
        return false;
    }
    return true;
}

bool validate_encryption_params_new(const std::string& input_file, int pegs) {
    if (!validate_input_file(input_file)) return false;
    if (!validate_peg_value(pegs)) return false;
    
    std::string derived_output = path_join(path_get_parent(input_file), "enc_" + path_get_filename(input_file));
    return validate_output_file(derived_output, input_file);
}

bool validate_decryption_params(const OperationParams& params) {
    return validate_operation_parameters(params, DEFAULT_ENCRYPT_DECRYPT_FLAGS);
}

// --- Core Cipher Operations ---
bool encrypt_file(const std::string& input_file, int pegs) {
    if (path_get_filename(input_file).rfind("enc_", 0) == 0) {
        std::cerr << "Error: File '" << input_file << "' appears to be already encrypted (name starts with 'enc_').\n";
        log_event("ENCRYPT_FAIL", "Attempted to re-encrypt file: " + input_file);
        return false;
    }
    if (!validate_encryption_params_new(input_file, pegs)) {
        return false;
    }
    
    std::string output_file = path_join(path_get_parent(input_file), "enc_" + path_get_filename(input_file));
    
    if (!process_file_core(input_file, output_file, pegs, true)) {
        log_event("ENCRYPT_FAIL", "Core processing failed for: " + input_file);
        return false;
    }
    
    if (!move_to_vault(input_file)) {
        std::cerr << "Warning: Encryption succeeded, but failed to move original file to the vault.\n";
        log_event("VAULT_FAIL", "Failed to move " + input_file + " to vault post-encryption.");
    }
    return true;
}

bool decrypt_file(const std::string& input_file, const std::string& output_file, int pegs) {
    OperationParams params = {input_file, output_file, pegs};
    if (!validate_decryption_params(params)) {
        return false;
    }
    return process_file_core(input_file, output_file, pegs, false);
}

bool move_to_vault(const std::string& original_filepath) {
    if (!ensure_private_vault_exists()) return false;
    
    if (!is_regular_file(original_filepath)) {
        std::cerr << "Error (Vault): Source '" << original_filepath << "' is not a valid file to move.\n";
        return false;
    }
    
    std::string dest_in_vault = path_join(PRIVATE_VAULT_DIR, path_get_filename(original_filepath));
    if (file_exists(dest_in_vault)) {
        std::cerr << "Error (Vault): A file with the name '" << path_get_filename(original_filepath)
                  << "' already exists in the vault.\n";
        return false;
    }
    
    if (std::rename(original_filepath.c_str(), dest_in_vault.c_str()) != 0) {
        std::cerr << "Error (Vault): Failed to move '" << original_filepath << "'. Check permissions.\n";
        return false;
    }
    
    log_event("VAULT_STORE", "Moved to vault: " + path_get_filename(original_filepath));
    return true;
}

bool retrieve_from_vault(const std::string& filename_in_vault, const std::string& destination_path) {
    if (!ensure_private_vault_exists()) {
        std::cerr << "Error (Retrieve): Private vault does not exist.\n";
        return false;
    }
    
    std::string source_in_vault = path_join(PRIVATE_VAULT_DIR, filename_in_vault);
    if (!is_regular_file(source_in_vault)) {
        std::cerr << "Error (Retrieve): File '" << filename_in_vault << "' not found in the vault.\n";
        return false;
    }
    
    if (!validate_output_file(destination_path, source_in_vault)) {
        return false;
    }
    
    if (!manual_copy_file(source_in_vault, destination_path)) {
        std::cerr << "Error (Retrieve): Failed to copy file from vault to '" << destination_path << "'.\n";
        log_event("RETRIEVE_FAIL", "Failed copy from " + filename_in_vault + " to " + destination_path);
        return false;
    }
    
    std::cout << "Info: File '" << filename_in_vault << "' retrieved to '" << destination_path << "'.\n";
    log_event("VAULT_RETRIEVE", filename_in_vault + " retrieved to " + destination_path);
    return true;
}

// --- History and Logging ---
void log_operation(const std::string& op_type, const std::string& in_file, const std::string& out_file, int pegs) {
    std::ostringstream msg;
    msg << op_type << ": " << in_file << " -> " << out_file << " (pegs: " << pegs << ")";
    log_to_file(msg.str());
}

void log_event(const std::string& event_type, const std::string& details) {
    std::ostringstream msg;
    msg << "EVENT (" << event_type << "): " << details;
    log_to_file(msg.str());
}

// --- Admin/Security ---
bool check_admin_password(const std::string& password_attempt) {
    return password_attempt == ADMIN_PASSWORD;
}

bool ensure_private_vault_exists() {
    if (is_directory(PRIVATE_VAULT_DIR)) {
        return true;
    }
    if (file_exists(PRIVATE_VAULT_DIR)) {
        std::cerr << "Error: Vault path '" << PRIVATE_VAULT_DIR << "' exists but is not a directory.\n";
        return false;
    }
    if (!create_directory(PRIVATE_VAULT_DIR)) {
        std::cerr << "Error: Could not create private vault directory '" << PRIVATE_VAULT_DIR << "'.\n";
        return false;
    }
    std::cout << "Info: Private vault directory created: '" << PRIVATE_VAULT_DIR << "'\n";
    return true;
}

// --- Comparison and Hashing ---
std::string calculate_sha256(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        log_event("HASH_ERROR", "Could not open file for hashing: " + filepath);
        return "";
    }

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_get_digestbyname("SHA256");
    if (!mdctx || !md) {
        log_event("HASH_ERROR", "OpenSSL SHA256 setup failed for: " + filepath);
        if (mdctx) EVP_MD_CTX_free(mdctx);
        return "";
    }

    if (1 != EVP_DigestInit_ex(mdctx, md, nullptr)) {
        log_event("HASH_ERROR", "EVP_DigestInit_ex failed for: " + filepath);
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    std::vector<char> read_buffer(BUFFER_SIZE);
    while (file.read(read_buffer.data(), read_buffer.size()) || file.gcount() > 0) {
        if (1 != EVP_DigestUpdate(mdctx, read_buffer.data(), file.gcount())) {
            log_event("HASH_ERROR", "EVP_DigestUpdate failed for: " + filepath);
            EVP_MD_CTX_free(mdctx);
            return "";
        }
    }

    if (file.bad()) {
        log_event("HASH_ERROR", "File read error during hashing: " + filepath);
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (1 != EVP_DigestFinal_ex(mdctx, hash, &hash_len)) {
        log_event("HASH_ERROR", "EVP_DigestFinal_ex failed for: " + filepath);
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    EVP_MD_CTX_free(mdctx);

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < hash_len; ++i) {
        ss << std::setw(2) << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string load_file_content_to_string(const std::string& filepath, size_t max_chars_to_load) {
    if (!is_regular_file(filepath)) {
        log_event("LOAD_FAIL", "File not regular or not found: " + filepath);
        return "";
    }
    std::ifstream file_stream(filepath, std::ios::binary);
    if (!file_stream) {
        log_event("LOAD_FAIL", "Could not open file: " + filepath);
        return "";
    }
    std::string content;
    content.resize(max_chars_to_load);
    file_stream.read(&content[0], max_chars_to_load);
    content.resize(file_stream.gcount());
    
    if (file_stream.bad()) {
        log_event("LOAD_FAIL", "Error reading file: " + filepath);
    }
    return content;
}

TextCompareResult compare_string_contents(const std::string& content1, const std::string& content2,
                                          const std::string& label1, const std::string& label2) {
    TextCompareResult result;
    result.files_readable = true;
    result.content1 = content1;
    result.content2 = content2;

    const size_t len1 = content1.length();
    const size_t len2 = content2.length();
    const size_t min_len = std::min(len1, len2);
    const size_t max_len = std::max(len1, len2);
    size_t matching_chars = 0;
    result.first_diff_offset = -1;

    for (size_t i = 0; i < min_len; ++i) {
        if (content1[i] == content2[i]) {
            matching_chars++;
        } else if (result.first_diff_offset == -1) {
            result.first_diff_offset = static_cast<long>(i);
        }
    }

    if (max_len > 0) {
        result.match_percentage = (static_cast<float>(matching_chars) / static_cast<float>(max_len)) * 100.0f;
    } else { // Both are empty
        result.match_percentage = 100.0f;
    }
    
    if (len1 != len2 && result.first_diff_offset == -1) {
        result.first_diff_offset = static_cast<long>(min_len);
    }
    
    log_event("COMPARE_STRINGS", "Compared " + label1 + " with " + label2);
    return result;
}

TextCompareResult compare_text_files(const std::string& filepath1, const std::string& filepath2, size_t max_chars) {
    TextCompareResult result;
    if (!is_regular_file(filepath1)) {
        result.error_message = "File 1 not found or is not a regular file: " + filepath1;
        return result;
    }
    if (!is_regular_file(filepath2)) {
        result.error_message = "File 2 not found or is not a regular file: " + filepath2;
        return result;
    }
    result.content1 = load_file_content_to_string(filepath1, max_chars);
    result.content2 = load_file_content_to_string(filepath2, max_chars);
    result.files_readable = true;

    return compare_string_contents(result.content1, result.content2, filepath1, filepath2);
}

BinaryCompareResult compare_binary_files(const std::string& filepath1, const std::string& filepath2) {
    BinaryCompareResult result;

    // Process File 1
    result.file1_exists = is_regular_file(filepath1);
    if (result.file1_exists) {
        long long size = get_file_size(filepath1);
        if (size < 0) {
            result.error_message_file1 = "Could not read size of '" + filepath1 + "'.";
        } else {
            result.file1_size = static_cast<unsigned long long>(size);
            result.file1_hash = calculate_sha256(filepath1);
            if (result.file1_hash.empty() && result.file1_size > 0) {
                result.error_message_file1 = "Failed to calculate SHA256 hash for '" + filepath1 + "'.";
            }
        }
    } else {
        result.error_message_file1 = "File not found or is not a regular file: " + filepath1;
    }

    // Process File 2
    result.file2_exists = is_regular_file(filepath2);
    if (result.file2_exists) {
        long long size = get_file_size(filepath2);
        if (size < 0) {
            result.error_message_file2 = "Could not read size of '" + filepath2 + "'.";
        } else {
            result.file2_size = static_cast<unsigned long long>(size);
            result.file2_hash = calculate_sha256(filepath2);
            if (result.file2_hash.empty() && result.file2_size > 0) {
                result.error_message_file2 = "Failed to calculate SHA256 hash for '" + filepath2 + "'.";
            }
        }
    } else {
        result.error_message_file2 = "File not found or is not a regular file: " + filepath2;
    }

    // Compare results
    if (result.file1_exists && result.file2_exists) {
        result.sizes_match = (result.file1_size == result.file2_size);
        bool hash1_ok = !result.file1_hash.empty() || result.file1_size == 0;
        bool hash2_ok = !result.file2_hash.empty() || result.file2_size == 0;
        if (hash1_ok && hash2_ok) {
            result.hashes_match = (result.file1_hash == result.file2_hash);
        }
    }
    
    log_event("COMPARE_BINARY", "Compared " + filepath1 + " with " + filepath2);
    return result;
}

std::string process_content_caesar(const std::string& content, int pegs, bool encrypt_mode) {
    std::string processed_content = content;
    for (char& c : processed_content) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (encrypt_mode) {
            uc = static_cast<unsigned char>((uc + pegs) % 256);
        } else {
            uc = static_cast<unsigned char>((uc - pegs + 256) % 256);
        }
        c = static_cast<char>(uc);
    }
    return processed_content;
}
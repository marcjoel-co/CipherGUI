#include "cipher_utils.h" // Should be first

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>     // For std::cerr, std::cout (consider replacing with GUI feedback/logging)
#include <chrono>
#include <iomanip>      // For std::put_time, std::setw, std::setfill, std::hex
#include <limits>
#include <system_error>
#include <sstream>      // For std::ostringstream in calculate_sha256
#include <algorithm>    // For std::min, std::max in compare_text_files

#include <openssl/evp.h>   // For EVP API (modern OpenSSL hashing)
#include <openssl/err.h>   // Optional: For OpenSSL error messages like ERR_print_errors_fp

// All functions and definitions below should be within the cipher_utils namespace

namespace cipher_utils {

    // --- Definitions for extern const std::string variables from cipher_utils.h ---
    const std::string HISTORY_FILE = "history.md";
    const std::string PRIVATE_VAULT_DIR = ".private_vault";
    const std::string ADMIN_PASSWORD = "supersecretpassword123"; // Reminder: CHANGE THIS!

    // --- Anonymous Namespace for Internal Helper Functions ---
    namespace {

        // This function is only used within this .cpp file by validate_operation_parameters
        bool validate_output_file(const std::string& output_filename, const std::string& input_filename_for_comparison) {
            if (!input_filename_for_comparison.empty()) {
                std::filesystem::path out_fs_path(output_filename);
                std::filesystem::path in_fs_path(input_filename_for_comparison);
                if (std::filesystem::exists(out_fs_path) && std::filesystem::exists(in_fs_path)) {
                    std::error_code ec;
                    if (std::filesystem::equivalent(out_fs_path, in_fs_path, ec)) {
                        std::cerr << "Error (Output): File '" << output_filename
                                  << "' cannot be the same as the input file '" << input_filename_for_comparison << "'.\n";
                        return false;
                    }
                    if (ec) {
                        std::cerr << "Warning (Output): Could not determine if output is same as input: " << ec.message() << "\n";
                    }
                } else if (output_filename == input_filename_for_comparison) { // Fallback for non-existent files
                    std::cerr << "Error (Output): File '" << output_filename
                              << "' cannot be the same as the input file '" << input_filename_for_comparison << "'.\n";
                    return false;
                }
            }

            std::filesystem::path out_path(output_filename);
            std::filesystem::path parent_dir = out_path.parent_path();
            if (parent_dir.empty()) {
                parent_dir = "."; // Current directory
            }

            if (!std::filesystem::exists(parent_dir)) {
                std::cerr << "Error (Output): Directory for output file '" << output_filename << "' (" << parent_dir.string() << ") does not exist.\n";
                return false;
            }
            if (!std::filesystem::is_directory(parent_dir)) {
                std::cerr << "Error (Output): Path for output file directory '" << parent_dir.string() << "' is not a directory.\n";
                return false;
            }
            if (std::filesystem::exists(out_path) && !std::filesystem::is_regular_file(out_path)) {
                std::cerr << "Error (Output): Path '" << output_filename << "' exists but is not a regular file.\n";
                return false;
            }

            std::filesystem::path temp_file_path = parent_dir / ("temp_write_check_" + out_path.filename().string() + ".tmp");
            std::ofstream temp_stream(temp_file_path);
            if (!temp_stream.is_open()) {
                std::cerr << "Error (Output): Cannot write to output directory '" << parent_dir.string()
                          << "'. Check permissions for file '" << output_filename << "'.\n";
                return false;
            }
            temp_stream.close();
            std::error_code ec_remove;
            std::filesystem::remove(temp_file_path, ec_remove);
            return true;
        }

    } // End anonymous namespace

    // --- Helper: Extension Check ---
    bool has_txt_extension(const std::string& filename) {
        std::filesystem::path filePath(filename);
        return filePath.extension() == ".txt";
    }

    // --- Validation Functions Implementations ---
    bool validate_input_file(const std::string& filename) {
        std::filesystem::path filePath(filename);
        if (!std::filesystem::exists(filePath)) {
            std::cerr << "Error (Input): File '" << filename << "' does not exist.\n";
            return false;
        }
        if (!std::filesystem::is_regular_file(filePath)) {
            std::cerr << "Error (Input): '" << filename << "' is not a regular file.\n";
            return false;
        }
        std::error_code ec;
        uintmax_t fileSize = std::filesystem::file_size(filePath, ec);
        if (ec) {
            std::cerr << "Error (Input): Could not get size of file '" << filename << "': " << ec.message() << "\n";
            return false;
        }
        if (fileSize == 0) {
            std::cerr << "Error (Input): File '" << filename << "' is empty.\n";
            return false;
        }
        return true;
    }

    bool validate_peg_value(int peg) {
        return (peg >= MIN_PEG && peg <= MAX_PEG);
    }

    bool validate_operation_parameters(const OperationParams& params, const ValidationFlags& flags) {
        if (flags.check_input_file) {
            if (!validate_input_file(params.input_file)) {
                return false;
            }
        }
        if (flags.check_output_file) {
            std::string input_for_comparison = flags.ensure_output_different_from_input ? params.input_file : "";
            if (!validate_output_file(params.output_file, input_for_comparison)) {
                return false;
            }
        }
        if (flags.check_pegs) {
            if (!validate_peg_value(params.pegs)) {
                std::cerr << "Error: Peg value " << params.pegs << " is out of range ("
                          << MIN_PEG << "-" << MAX_PEG << ").\n";
                return false;
            }
        }
        return true;
    }

    bool validate_decryption_params(const OperationParams& params) {
        return validate_operation_parameters(params, DEFAULT_ENCRYPT_DECRYPT_FLAGS);
    }

    // --- Core Cipher Operations Implementations ---
    bool process_file_core(const std::string& input_file, const std::string& output_file, int pegs, bool encrypt_mode) {
        std::ifstream in(input_file, std::ios::binary);
        if (!in) {
            std::cerr << "Error opening input file: " << input_file << std::endl;
            return false;
        }
        std::ofstream out(output_file, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "Error opening output file: " << output_file << std::endl;
            return false;
        }
        std::cout << (encrypt_mode ? "Encrypting " : "Decrypting ")
                  << input_file << " -> " << output_file << " (Pegs: " << pegs << ")\n";
        std::vector<unsigned char> buffer_vec(BUFFER_SIZE);
        while (true) {
            in.read(reinterpret_cast<char*>(buffer_vec.data()), buffer_vec.size());
            size_t bytes_read = static_cast<size_t>(in.gcount());
            if (bytes_read == 0) {
                if (in.eof()) break;
                if (in.bad()) {
                    std::cerr << "Read error (badbit) occurred on input file " << input_file << ".\n";
                    return false;
                }
                if (in.fail()) {
                    std::cerr << "Logical read error (failbit) on input file " << input_file << ".\n";
                    return false;
                }
                break;
            }
            for (size_t i = 0; i < bytes_read; ++i) {
                if (encrypt_mode) {
                    buffer_vec[i] = static_cast<unsigned char>((buffer_vec[i] + pegs) % 256);
                } else {
                    buffer_vec[i] = static_cast<unsigned char>((buffer_vec[i] - pegs + 256) % 256);
                }
            }
            if (!out.write(reinterpret_cast<const char*>(buffer_vec.data()), bytes_read)) {
                std::cerr << "Write error occurred during " << (encrypt_mode ? "encryption" : "decryption") << ".\n";
                return false;
            }
        }
        if (in.bad()) {
            std::cerr << "Read error (badbit) occurred on input file " << input_file << " (after loop).\n";
            return false;
        }
        std::cout << "File " << (encrypt_mode ? "encrypted" : "decrypted") << " successfully.\n";
        log_operation((encrypt_mode ? "ENCRYPT" : "DECRYPT"), input_file, output_file, pegs);
        return true;
    }

    bool decrypt_file(const std::string& input_file, const std::string& output_file, int pegs) {
        return process_file_core(input_file, output_file, pegs, false);
    }

    // --- History and Logging Implementations ---
    void log_operation(const std::string& operation_type, const std::string& input_file, const std::string& output_file, int pegs) {
        std::ofstream file(HISTORY_FILE, std::ios::app);
        if (!file) {
            std::cerr << "Warning: Could not log operation to '" << HISTORY_FILE << "'.\n";
            return;
        }
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm_obj{};
        #ifdef _WIN32
            localtime_s(&now_tm_obj, &now_c);
        #else
            localtime_r(&now_c, &now_tm_obj);
        #endif
        file << operation_type << ": " << input_file << " -> " << output_file
             << " (pegs: " << pegs << ") | "
             << std::put_time(&now_tm_obj, "%Y-%m-%d %H:%M:%S") << '\n';
    }

    void log_event(const std::string& event_type, const std::string& details) {
        std::ofstream file(HISTORY_FILE, std::ios::app);
        if (!file) {
            std::cerr << "Warning: Could not log event to '" << HISTORY_FILE << "'.\n";
            return;
        }
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm_obj{};
        #ifdef _WIN32
            localtime_s(&now_tm_obj, &now_c);
        #else
            localtime_r(&now_c, &now_tm_obj);
        #endif
        file << "EVENT (" << event_type << "): " << details
             << " | " << std::put_time(&now_tm_obj, "%Y-%m-%d %H:%M:%S") << '\n';
    }

    // --- Console Input Implementations ---
    bool safe_console_input(std::string& buffer) {
        if (std::getline(std::cin, buffer)) {
            return true;
        }
        if (std::cin.eof()) { /* Usually not an error for GUIs */ }
        else if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        return false;
    }

    void clear_console_stdin(void) {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    // --- NEW Functions for Refactoring (Vault, Auto-Output Encryption, etc.) ---
    bool ensure_private_vault_exists() {
        std::filesystem::path vault_path(PRIVATE_VAULT_DIR);
        if (!std::filesystem::exists(vault_path)) {
            std::error_code ec;
            std::filesystem::create_directory(vault_path, ec);
            if (ec) {
                std::cerr << "Error: Could not create private vault directory '" << PRIVATE_VAULT_DIR << "': " << ec.message() << "\n";
                return false;
            }
            std::cout << "Info: Private vault directory '" << PRIVATE_VAULT_DIR << "' created.\n";
        } else if (!std::filesystem::is_directory(vault_path)) {
            std::cerr << "Error: Path for private vault '" << PRIVATE_VAULT_DIR << "' exists but is not a directory.\n";
            return false;
        }
        return true;
    }

    bool check_admin_password(const std::string& password_attempt) {
        return password_attempt == ADMIN_PASSWORD;
    }

    bool move_to_vault(const std::string& original_filepath_str) {
        if (!ensure_private_vault_exists()) return false;
        std::filesystem::path original_filepath(original_filepath_str);
        if (!std::filesystem::exists(original_filepath) || !std::filesystem::is_regular_file(original_filepath)) {
            std::cerr << "Error (Vault Move): Source file '" << original_filepath_str << "' does not exist or is not a regular file.\n";
            return false;
        }
        std::filesystem::path vault_path_obj(PRIVATE_VAULT_DIR);
        std::filesystem::path destination_in_vault = vault_path_obj / original_filepath.filename();
        if (std::filesystem::exists(destination_in_vault)) {
            std::cerr << "Error (Vault Move): File '" << original_filepath.filename().string() 
                      << "' already exists in the vault. Original not moved.\n";
            return false; 
        }
        std::error_code ec;
        std::filesystem::rename(original_filepath, destination_in_vault, ec);
        if (ec) {
            std::cerr << "Error (Vault Move): Failed to move '" << original_filepath_str
                      << "' to vault: " << ec.message() << "\n";
            return false;
        }
        log_event("VAULT_STORE", original_filepath.filename().string() + " moved to vault.");
        return true;
    }

    bool validate_derived_output_file(const std::string& derived_output_filename, const std::string& input_filename) {
        std::filesystem::path out_path(derived_output_filename);
        std::filesystem::path in_path(input_filename);
        if (std::filesystem::exists(out_path) && std::filesystem::exists(in_path)) {
             std::error_code ec;
             if (std::filesystem::equivalent(out_path, in_path, ec)) {
                std::cerr << "Error (Output): Derived output file '" << derived_output_filename
                        << "' is the same as the input file. This should not happen with 'enc_' prefix.\n";
                return false;
            }
        } else if (out_path == in_path) {
             std::cerr << "Error (Output): Derived output file '" << derived_output_filename
                        << "' is the same as the input file (path match).\n";
            return false;
        }
        std::filesystem::path parent_dir = out_path.parent_path();
        if (parent_dir.empty()) parent_dir = ".";
        if (!std::filesystem::exists(parent_dir) || !std::filesystem::is_directory(parent_dir)) {
            std::cerr << "Error (Output): Directory for derived output file '" << derived_output_filename << "' (" << parent_dir.string() << ") does not exist or is not a directory.\n";
            return false;
        }
        if (std::filesystem::exists(out_path) && !std::filesystem::is_regular_file(out_path)) {
            std::cerr << "Error (Output): Path '" << derived_output_filename << "' exists but is not a regular file.\n";
            return false;
        }
        std::filesystem::path temp_file_path = parent_dir / ("temp_write_check_derived_" + out_path.filename().string() + ".tmp");
        std::ofstream temp_stream(temp_file_path);
        if (!temp_stream.is_open()) {
            std::cerr << "Error (Output): Cannot write to output directory '" << parent_dir.string()
                      << "' for derived file '" << derived_output_filename << "'. Check permissions.\n";
            return false;
        }
        temp_stream.close();
        std::filesystem::remove(temp_file_path);
        return true;
    }

    bool validate_encryption_params_new(const std::string& input_file, int pegs) {
        if (!validate_input_file(input_file)) {
            return false;
        }
        if (!validate_peg_value(pegs)) {
            std::cerr << "Error: Peg value " << pegs << " is out of range ("
                      << MIN_PEG << "-" << MAX_PEG << ").\n";
            return false;
        }
        std::filesystem::path p(input_file);
        std::string derived_output_filename = (p.parent_path() / ("enc_" + p.filename().string())).string();
        if (!validate_derived_output_file(derived_output_filename, input_file)) {
            return false;
        }
        return true;
    }

    bool encrypt_file(const std::string& input_file_str, int pegs) {
        std::filesystem::path p_input(input_file_str);
        if (p_input.filename().string().rfind("enc_", 0) == 0) {
            std::cerr << "Error: File '" << input_file_str << "' already appears to be encrypted (starts with 'enc_'). Aborting.\n";
            log_event("ENCRYPT_FAIL", "Attempt to encrypt already encrypted file: " + input_file_str);
            return false;
        }
        if (!validate_encryption_params_new(input_file_str, pegs)) {
            return false;
        }
        std::string output_file_str = (p_input.parent_path() / ("enc_" + p_input.filename().string())).string();
        if (!process_file_core(input_file_str, output_file_str, pegs, true)) {
            log_event("ENCRYPT_FAIL", "Core processing failed for: " + input_file_str);
            return false; 
        }
        if (!move_to_vault(input_file_str)) {
            std::cerr << "Warning: Encryption succeeded for '" << input_file_str << "' to '" << output_file_str
                      << "', but failed to move original to vault. Original file remains in place.\n";
            log_event("VAULT_FAIL", "Failed to move " + input_file_str + " to vault after encryption to " + output_file_str);
        }
        return true;
    }

    bool validate_destination_path(const std::string& destination_path_str, const std::string& source_filename_in_vault) {
        std::filesystem::path dest_path(destination_path_str);
        std::filesystem::path vault_dir_path(PRIVATE_VAULT_DIR);
        std::filesystem::path source_in_vault_full_path = vault_dir_path / source_filename_in_vault;
        std::error_code ec_equiv;
        if (std::filesystem::exists(dest_path) && std::filesystem::exists(source_in_vault_full_path) &&
            std::filesystem::equivalent(dest_path, source_in_vault_full_path, ec_equiv)) {
            std::cerr << "Error (Retrieve): Destination path '" << destination_path_str 
                      << "' is the same as the source file in the vault. Choose a different path.\n";
            return false;
        }
        if (ec_equiv) { /* Handle error if equivalent check fails */ }
        std::filesystem::path parent_dir = dest_path.parent_path();
        if (parent_dir.empty()) parent_dir = ".";
        if (!std::filesystem::exists(parent_dir) || !std::filesystem::is_directory(parent_dir)) {
            std::cerr << "Error (Retrieve): Directory for destination file '" << destination_path_str << "' (" << parent_dir.string() << ") does not exist or is not a directory.\n";
            return false;
        }
        if (std::filesystem::exists(dest_path) && !std::filesystem::is_regular_file(dest_path)) {
            std::cerr << "Error (Retrieve): Destination path '" << destination_path_str << "' exists but is not a regular file. Please choose a different path or filename.\n";
            return false;
        }
        std::filesystem::path temp_file_path = parent_dir / ("temp_write_check_retrieve_" + dest_path.filename().string() + ".tmp");
        std::ofstream temp_stream(temp_file_path);
        if (!temp_stream.is_open()) {
            std::cerr << "Error (Retrieve): Cannot write to destination directory '" << parent_dir.string()
                      << "' for file '" << destination_path_str << "'. Check permissions.\n";
            return false;
        }
        temp_stream.close();
        std::filesystem::remove(temp_file_path);
        return true;
    }

    bool retrieve_from_vault(const std::string& filename_in_vault, const std::string& destination_path_str) {
        if (!ensure_private_vault_exists()) {
            std::cerr << "Error (Retrieve): Private vault does not exist.\n";
            return false;
        }
        std::filesystem::path vault_fs_path(PRIVATE_VAULT_DIR);
        std::filesystem::path source_in_vault = vault_fs_path / filename_in_vault;
        if (!std::filesystem::exists(source_in_vault) || !std::filesystem::is_regular_file(source_in_vault)) {
            std::cerr << "Error (Retrieve): File '" << filename_in_vault << "' not found or is not a regular file in the vault ('" << source_in_vault.string() << "').\n";
            return false;
        }
        if (!validate_destination_path(destination_path_str, filename_in_vault)) {
            return false;
        }
        std::filesystem::path destination_fs_path(destination_path_str);
        std::filesystem::copy_options copy_opts = std::filesystem::copy_options::overwrite_existing; 
        std::error_code ec;
        std::filesystem::copy_file(source_in_vault, destination_fs_path, copy_opts, ec);
        if (ec) {
            std::cerr << "Error (Retrieve): Failed to copy '" << filename_in_vault << "' from vault to '"
                      << destination_path_str << "': " << ec.message() << "\n";
            log_event("VAULT_RETRIEVE_FAIL", "Failed to copy " + filename_in_vault + " to " + destination_path_str + ": " + ec.message());
            return false;
        }
        std::cout << "Info: File '" << filename_in_vault << "' successfully retrieved from vault to '" << destination_path_str << "'.\n";
        log_event("VAULT_RETRIEVE", filename_in_vault + " retrieved to " + destination_path_str);
        return true;
    }

    // --- Implementation for calculate_sha256 (using EVP API) ---
    std::string calculate_sha256(const std::string& filepath_str) {
        std::filesystem::path filepath(filepath_str);
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            log_event("HASH_ERROR", "Could not open file for hashing: " + filepath_str);
            return ""; 
        }

        EVP_MD_CTX *mdctx = nullptr;
        const EVP_MD *md = nullptr;
        unsigned char hash[EVP_MAX_MD_SIZE]; 
        unsigned int hash_len = 0;
        
        md = EVP_get_digestbyname("SHA256");
        if (md == nullptr) {
            log_event("HASH_ERROR", "SHA256 not available via EVP for: " + filepath_str);
            // ERR_print_errors_fp(stderr); // Uncomment for OpenSSL error details
            return "";
        }

        mdctx = EVP_MD_CTX_new();
        if (mdctx == nullptr) {
            log_event("HASH_ERROR", "Failed to create EVP_MD_CTX for: " + filepath_str);
            // ERR_print_errors_fp(stderr);
            return "";
        }

        if (1 != EVP_DigestInit_ex(mdctx, md, nullptr)) {
            log_event("HASH_ERROR", "Failed to initialize EVP_Digest for: " + filepath_str);
            // ERR_print_errors_fp(stderr);
            EVP_MD_CTX_free(mdctx);
            return "";
        }

        std::vector<char> R_buffer(BUFFER_SIZE); 
        while (file.read(R_buffer.data(), R_buffer.size()) || file.gcount() > 0) {
            if (1 != EVP_DigestUpdate(mdctx, R_buffer.data(), file.gcount())) {
                log_event("HASH_ERROR", "Failed to update EVP_Digest for: " + filepath_str);
                // ERR_print_errors_fp(stderr);
                EVP_MD_CTX_free(mdctx);
                return "";
            }
        }

        if (file.bad()) {
            log_event("HASH_ERROR", "Error reading file during hashing: " + filepath_str);
            EVP_MD_CTX_free(mdctx);
            return "";
        }

        if (1 != EVP_DigestFinal_ex(mdctx, hash, &hash_len)) {
            log_event("HASH_ERROR", "Failed to finalize EVP_Digest for: " + filepath_str);
            // ERR_print_errors_fp(stderr);
            EVP_MD_CTX_free(mdctx);
            return "";
        }

        EVP_MD_CTX_free(mdctx); 

        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < hash_len; i++) {
            ss << std::setw(2) << static_cast<int>(hash[i]);
        }
        return ss.str();
    }

    // --- Comparison Function Implementations ---
    TextCompareResult compare_text_files(const std::string& filepath1_str, const std::string& filepath2_str, size_t max_chars_to_load) {
        TextCompareResult result;
        std::filesystem::path path1(filepath1_str);
        std::filesystem::path path2(filepath2_str);

        // File 1 Handling
        std::ifstream file1_stream(path1, std::ios::binary);
        if (!std::filesystem::exists(path1) || !std::filesystem::is_regular_file(path1)) {
            result.error_message = "File 1 ('" + path1.string() + "') not found or is not a regular file.";
            log_event("COMPARE_TEXT_FAIL", result.error_message);
            return result;
        }
        if (!file1_stream.is_open()) {
            result.error_message = "Could not open File 1 ('" + path1.string() + "') for reading.";
            log_event("COMPARE_TEXT_FAIL", result.error_message);
            return result;
        }

        // File 2 Handling
        std::ifstream file2_stream(path2, std::ios::binary);
        if (!std::filesystem::exists(path2) || !std::filesystem::is_regular_file(path2)) {
            if (!result.error_message.empty()) result.error_message += " ";
            result.error_message += "File 2 ('" + path2.string() + "') not found or is not a regular file.";
            log_event("COMPARE_TEXT_FAIL", "File 2 error: " + path2.string());
            file1_stream.close();
            return result;
        }
        if (!file2_stream.is_open()) {
            if (!result.error_message.empty()) result.error_message += " ";
            result.error_message += "Could not open File 2 ('" + path2.string() + "') for reading.";
            log_event("COMPARE_TEXT_FAIL", "File 2 open error: " + path2.string());
            file1_stream.close();
            return result;
        }

        result.files_readable = true;

        std::vector<char> buffer1_vec(max_chars_to_load);
        file1_stream.read(buffer1_vec.data(), max_chars_to_load);
        result.content1.assign(buffer1_vec.data(), file1_stream.gcount());
        if (file1_stream.bad()) {
            if (!result.error_message.empty()) result.error_message += " ";
            result.error_message += "Error reading File 1 ('" + path1.string() + "').";
            result.files_readable = false;
        }
        file1_stream.close();

        std::vector<char> buffer2_vec(max_chars_to_load);
        file2_stream.read(buffer2_vec.data(), max_chars_to_load);
        result.content2.assign(buffer2_vec.data(), file2_stream.gcount());
        if (file2_stream.bad()) {
            if (!result.error_message.empty()) result.error_message += " ";
            result.error_message += "Error reading File 2 ('" + path2.string() + "')."; // Corrected string concat
            result.files_readable = false;
        }
        file2_stream.close();

        if (!result.files_readable && result.content1.empty() && result.content2.empty()) {
            log_event("COMPARE_TEXT_FAIL", "Failed to read content from both files.");
            return result;
        }

        size_t len1 = result.content1.length();
        size_t len2 = result.content2.length();
        size_t min_len = std::min(len1, len2);
        size_t max_len = std::max(len1, len2);
        size_t matching_chars = 0;
        result.first_diff_offset = -1;

        for (size_t i = 0; i < min_len; ++i) {
            if (result.content1[i] == result.content2[i]) {
                matching_chars++;
            } else if (result.first_diff_offset == -1) {
                result.first_diff_offset = static_cast<long>(i);
            }
        }

        if (max_len > 0) {
            result.match_percentage = (static_cast<float>(matching_chars) / static_cast<float>(max_len)) * 100.0f;
        } else if (len1 == 0 && len2 == 0) {
             result.match_percentage = 100.0f;
        } else {
             result.match_percentage = 0.0f;
        }

        if (len1 != len2 && result.first_diff_offset == -1) {
            result.first_diff_offset = static_cast<long>(min_len);
        }

        log_event("COMPARE_TEXT", "Compared: " + path1.string() + " with " + path2.string() +
                                  ". Match: " + std::to_string(result.match_percentage) + "%. First diff at: " + std::to_string(result.first_diff_offset));
        return result;
    }

    BinaryCompareResult compare_binary_files(const std::string& filepath1_str, const std::string& filepath2_str) {
        BinaryCompareResult result;
        std::filesystem::path p1(filepath1_str);
        std::filesystem::path p2(filepath2_str);

        // File 1
        result.file1_exists = std::filesystem::exists(p1) && std::filesystem::is_regular_file(p1);
        if (result.file1_exists) {
            std::error_code ec;
            result.file1_size = std::filesystem::file_size(p1, ec);
            if (ec) {
                result.error_message_file1 = "Could not get size of '" + p1.string() + "': " + ec.message();
                result.file1_exists = false; 
            } else {
                result.file1_hash = calculate_sha256(p1.string()); // Uses EVP version
                if (result.file1_hash.empty() && result.file1_size > 0) {
                    result.error_message_file1 = "Could not calculate SHA256 hash for '" + p1.string() + "'.";
                }
            }
        } else {
            result.error_message_file1 = "File 1 ('" + p1.string() + "') not found or is not a regular file.";
        }

        // File 2
        result.file2_exists = std::filesystem::exists(p2) && std::filesystem::is_regular_file(p2);
        if (result.file2_exists) {
            std::error_code ec;
            result.file2_size = std::filesystem::file_size(p2, ec);
            if (ec) {
                result.error_message_file2 = "Could not get size of '" + p2.string() + "': " + ec.message();
                result.file2_exists = false;
            } else {
                result.file2_hash = calculate_sha256(p2.string()); // Uses EVP version
                if (result.file2_hash.empty() && result.file2_size > 0) {
                    result.error_message_file2 = "Could not calculate SHA256 hash for '" + p2.string() + "'.";
                }
            }
        } else {
            result.error_message_file2 = "File 2 ('" + p2.string() + "') not found or is not a regular file.";
        }

        if (result.file1_exists && result.file2_exists) {
            result.sizes_match = (result.file1_size == result.file2_size);
            
            bool file1_hash_ok = !result.file1_hash.empty() || (result.file1_hash.empty() && result.file1_size == 0);
            bool file2_hash_ok = !result.file2_hash.empty() || (result.file2_hash.empty() && result.file2_size == 0);

            if (file1_hash_ok && file2_hash_ok) {
                 result.hashes_match = (result.file1_hash == result.file2_hash);
            } else {
                result.hashes_match = false;
            }
        } else { 
            result.sizes_match = false;
            result.hashes_match = false;
        }
        
        std::string log_msg = "Compared binary: " + p1.string() + " with " + p2.string() + ". ";
        if (result.file1_exists && result.file2_exists) {
            log_msg += "Sizes match: " + std::string(result.sizes_match ? "Yes" : "No") +
                       ". Hashes match: " + std::string(result.hashes_match ? "Yes" : "No");
        } else {
            log_msg += "One or both files not found/accessible for full comparison.";
        }
        log_event("COMPARE_BINARY", log_msg);
        return result;
    }

      std::string process_content_caesar(const std::string& content, int pegs, bool encrypt_mode) {
        std::string processed_content = content; // Make a mutable copy
        for (char &c : processed_content) {
            // Cast to unsigned char for defined behavior with arithmetic operations
            // that might go out of signed char range before modulo.
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

    // --- Content Loading and Comparison Helpers ---
    std::string load_file_content_to_string(const std::string& filepath_str, size_t max_chars_to_load) {
        std::filesystem::path filepath(filepath_str);
        std::string content;

        if (!std::filesystem::exists(filepath) || !std::filesystem::is_regular_file(filepath)) {
            // Optionally log or return an error indicator, for now, returns empty string
            log_event("LOAD_CONTENT_FAIL", "File not found or not regular: " + filepath_str);
            return "";
        }

        std::ifstream file_stream(filepath, std::ios::binary); // Read as binary to preserve all bytes
        if (!file_stream.is_open()) {
            log_event("LOAD_CONTENT_FAIL", "Could not open file: " + filepath_str);
            return "";
        }

        // Read up to max_chars_to_load
        content.resize(max_chars_to_load); // Pre-allocate
        file_stream.read(&content[0], max_chars_to_load);
        content.resize(file_stream.gcount()); // Resize to actual number of characters read

        if (file_stream.bad()) {
            log_event("LOAD_CONTENT_FAIL", "Error reading file: " + filepath_str);
            // Content might be partially read, or empty if read failed immediately
        }
        file_stream.close();
        return content;
    }

    TextCompareResult compare_string_contents(const std::string& content1, const std::string& content2,
                                              const std::string& label1, const std::string& label2) {
        TextCompareResult result;
        result.files_readable = true; // Assuming strings are always "readable"
        result.content1 = content1;   // Store copies for display if needed (though GUI might not need it here)
        result.content2 = content2;

        size_t len1 = content1.length();
        size_t len2 = content2.length();
        size_t min_len = std::min(len1, len2);
        size_t max_len = std::max(len1, len2);
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
        } else if (len1 == 0 && len2 == 0) { // Both contents are empty
            result.match_percentage = 100.0f;
        } else { // One is empty, other is not
            result.match_percentage = 0.0f;
        }

        if (len1 != len2 && result.first_diff_offset == -1) {
            result.first_diff_offset = static_cast<long>(min_len);
        }
        
        // No specific error message from this function, caller handles context
        // log_event("COMPARE_STRINGS", "Compared " + label1 + " with " + label2 + ". Match: " + std::to_string(result.match_percentage) + "%");
        return result;
    }


} // namespace cipher_utils
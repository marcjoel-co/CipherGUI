#include "cipher_utils.h"
#include <algorithm> // Not strictly used yet, but common for C++ utilities

namespace cipher_utils {

// --- Helper: Extension Check ---
bool has_txt_extension(const std::string& filename) {
    if (filename.length() < 4) return false; // Must be at least "a.txt"
    return filename.substr(filename.length() - 4) == ".txt";
    // A more robust way using std::filesystem:
    // std::filesystem::path filePath(filename);
    // return filePath.extension() == ".txt";
}

// --- Validation Functions Implementations ---
bool validate_input_file(const std::string& filename) {
    if (!has_txt_extension(filename)) {
        std::cerr << "Error (Input): File '" << filename << "' must have a .txt extension.\n";
        return false;
    }
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

bool validate_output_file(const std::string& output_filename, const std::string& input_filename_for_comparison) {
    if (!has_txt_extension(output_filename)) {
        std::cerr << "Error (Output): File '" << output_filename << "' must have a .txt extension.\n";
        return false;
    }

    if (!input_filename_for_comparison.empty()) {
         // Check if paths are equivalent only if both exist, otherwise it's not a meaningful comparison for overwriting
        std::filesystem::path out_fs_path(output_filename);
        std::filesystem::path in_fs_path(input_filename_for_comparison);
        if (std::filesystem::exists(out_fs_path) && std::filesystem::exists(in_fs_path)) {
            std::error_code ec;
            if (std::filesystem::equivalent(out_fs_path, in_fs_path, ec)) {
                 std::cerr << "Error (Output): File '" << output_filename
                          << "' cannot be the same as the input file '" << input_filename_for_comparison << "'.\n";
                return false;
            }
            if (ec) { // Error during equivalent check, treat as potential issue
                 std::cerr << "Warning (Output): Could not determine if output is same as input: " << ec.message() << "\n";
            }
        } else if (output_filename == input_filename_for_comparison) { // Fallback to string comparison if files don't exist
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

    // Basic writability check
    std::filesystem::path temp_file_path = parent_dir / "temp_write_check.tmp";
    std::ofstream temp_stream(temp_file_path);
    if (!temp_stream.is_open()) {
        std::cerr << "Error (Output): Cannot write to output directory '" << parent_dir.string()
                  << "'. Check permissions for file '" << output_filename << "'.\n";
        return false;
    }
    temp_stream.close();
    std::error_code ec_remove;
    std::filesystem::remove(temp_file_path, ec_remove);
    if (ec_remove) {
        std::cerr << "Warning (Output): Failed to remove temp write-check file: " << ec_remove.message() << "\n";
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

bool validate_encryption_params(const OperationParams& params) {
    return validate_operation_parameters(params, DEFAULT_ENCRYPT_DECRYPT_FLAGS);
}

bool validate_decryption_params(const OperationParams& params) {
    return validate_operation_parameters(params, DEFAULT_ENCRYPT_DECRYPT_FLAGS);
}


// --- Core Cipher Operations Implementations ---
// Common file processing logic
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
    while (in.read(reinterpret_cast<char*>(buffer_vec.data()), buffer_vec.size()) || in.gcount() > 0) {
        size_t bytes_read = static_cast<size_t>(in.gcount());
        if (bytes_read == 0 && in.eof()) break; // EOF and no bytes read in last attempt
        if (bytes_read == 0 && !in.eof()) { // Error or nothing read but not EOF
            if(in.bad()) std::cerr << "Read error occurred on input file " << input_file << ".\n";
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
            return false; // in and out will auto-close
        }
    }
    
    if (in.bad()) {
        std::cerr << "Read error (badbit) occurred on input file " << input_file << ".\n";
        return false;
    }

    std::cout << "File " << (encrypt_mode ? "encrypted" : "decrypted") << " successfully.\n";
    log_operation((encrypt_mode ? "ENCRYPT" : "DECRYPT"), input_file, output_file, pegs);
    return true;
}

bool encrypt_file(const std::string& input_file, const std::string& output_file, int pegs) {
    return process_file_core(input_file, output_file, pegs, true);
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
    std::tm now_tm = *std::localtime(&now_c); // Note: localtime is not thread-safe

    file << operation_type << ": " << input_file << " -> " << output_file
         << " (pegs: " << pegs << ") | "
         << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << '\n';
}

// --- Console Input Implementations (if needed) ---
bool safe_console_input(std::string& buffer) {
    if (std::getline(std::cin, buffer)) {
        return true;
    }
    if (std::cin.eof()) {
        std::cerr << "EOF reached on input." << std::endl;
    } else if (std::cin.fail()) {
        std::cerr << "Input error." << std::endl;
        std::cin.clear();
        clear_console_stdin();
    }
    return false;
}

void clear_console_stdin(void) {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

} // namespace cipher_utils
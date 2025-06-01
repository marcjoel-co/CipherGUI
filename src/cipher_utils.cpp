// src/cipher_utils.cpp
#include "cipher_utils.h"

// Safely reads filename into a buffer 
int safe_input(char *buffer, int size) {
    if (fgets(buffer, size, stdin) == NULL) {
        return 0;
    }
    buffer[strcspn(buffer, "\n")] = 0;
    return 1;
}

// Clears input buffer
void clear_stdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// ... PASTE ALL YOUR OTHER FUNCTION IMPLEMENTATIONS HERE ...
// ... (handle_file_error, validate_file, validate_peg_value, encrypt_file, decrypt_file, read_file, search_files, view_history, log_encryption) ...

// Make sure this function is present and uncommented if you use it
void handle_file_error(const char *filename) {
    printf("Error: Unable to open or process file '%s'.\n"
           "Ensure the file exists and you have the necessary permissions.\n", 
           filename);
}

// Validates file existence, non-emptiness, and .txt extension
int validate_file(const char *filename) {
    // First check if the file has .txt extension
    if (!has_txt_extension(filename)) {
        printf("Error: File '%s' must have a .txt extension.\n", filename);
        return 0;
    }

    // Try to open the file
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Error: Cannot open file '%s'. Check if the file exists.\n", filename);
        return 0;
    }

    // Check if file is empty
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);

    if (file_size == 0) {
        printf("Error: File '%s' is empty.\n", filename);
        return 0;
    }

    return 1;
}

// check if the peg is within the range
int validate_peg_value(int peg) {

    // check if input is from 0 to 256 
    return (peg >= MIN_PEG && peg <= MAX_PEG);
}

// main encryption 
int encrypt_file(const char *input_file, const char *output_file, int pegs) {
    FILE *in = NULL, *out = NULL;
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int success = 0;

    // Open input and output files
    in = fopen(input_file, "rb");
    if (in == NULL) {
        printf("Error opening input file: %s\n", input_file);
        return 0;
    }

    out = fopen(output_file, "wb");
    if (out == NULL) {
        printf("Error opening output file: %s\n", output_file);
        fclose(in);
        return 0;
    }

    printf("Encrypting %s -> %s (Pegs: %d)\n", input_file, output_file, pegs);

    // Process file in chunks with direct peg value
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, in)) > 0) { //4096
        for (size_t i = 0; i < bytes_read; i++) {  
            buffer[i] = (buffer[i] + pegs) % 256;

        }

        if (fwrite(buffer, 1, bytes_read, out) != bytes_read) { // 
            printf("Write error occurred during encryption.\n");
            goto cleanup;
    }
        }

    success = 1;
    printf("File encrypted successfully.\n");

cleanup:
    if (in) fclose(in);
    if (out) fclose(out);
    return success;
}

// decryption (just like encryption but reverse)
int decrypt_file(const char *input_file, const char *output_file, int pegs) {
    FILE *in = NULL, *out = NULL;
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int success = 0;

    // Open input and output files
    in = fopen(input_file, "rb");
    if (in == NULL) {
        printf("Error opening input file: %s\n", input_file);
        return 0;
    }

    out = fopen(output_file, "wb");
    if (out == NULL) {
        printf("Error opening output file: %s\n", output_file);
        fclose(in);
        return 0;
    }

    printf("Decrypting %s -> %s (Pegs: %d)\n", input_file, output_file, pegs);

    // Process file in chunks with reversed peg value
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, in)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            buffer[i] = (buffer[i] - pegs + 256) % 256; // Ensure result is non-negative
        }

        if (fwrite(buffer, 1, bytes_read, out) != bytes_read) {
            printf("Write error occurred during decryption.\n");
            goto cleanup;
        }
    }

    success = 1;
    printf("File decrypted successfully.\n");

cleanup:
    if (in) fclose(in);
    if (out) fclose(out);
    return success;
}

// Displays file contents
void read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        handle_file_error(filename);
        return;
    }

    char buffer[BUFFER_SIZE];
    printf("\nFile contents:\n");
    printf("-------------------\n");
    
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        printf("%s", buffer);
    }
    printf("\n-------------------\n");
    
    fclose(file);
}

// Lists files in current directory
void search_files(void) {
    DIR *dir; // struct type
    struct dirent *entry; //each entry
    int count = 0;

    dir = opendir("."); // "." represent current directory
    if (dir == NULL) {  // make sure that the directory exist 
        printf("Error: Cannot open current directory.\n");
        return;
    }

    printf("\nFiles in current directory:\n");
    printf("-------------------\n");
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  //d_type is a type of file
            printf("%s\n", entry->d_name);
            count++;
        }
    }
    
    printf("-------------------\n");
    printf("Total files: %d\n", count);
    
    closedir(dir);
}

// Displays encryption history
void view_history(void) {
    FILE *file = fopen(HISTORY_FILE, "r");
    if (file == NULL) {
        printf("No encryption history found.\n");
        return;
    }

    char buffer[BUFFER_SIZE];
    printf("\nEncryption History:\n");
    printf("-------------------\n");
    
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        printf("%s", buffer);
    }
    printf("-------------------\n");
    
    fclose(file);
}

// Logs encryption operations with timestamp
void log_encryption(const char *input_file, const char *output_file, int pegs) {
    FILE *file = fopen(HISTORY_FILE, "a");
    if (file == NULL) {
        printf("Warning: Could not log encryption history.\n");
        return;
    }

    time_t now;
    time(&now);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0';  // Remove newline

    fprintf(file, "%s -> %s (pegs: %d) | %s\n", 
           input_file, output_file, pegs, date);
    
    fclose(file);
}

int has_txt_extension(const char *filename) {
    const char *dot = strrchr(filename, '.'); // Needs <cstring>
    if (dot == NULL) {
        return 0;
    }
    return strcmp(dot, ".txt") == 0; // Needs <cstring>
}
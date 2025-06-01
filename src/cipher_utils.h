// src/cipher_utils.h
#ifndef CIPHER_UTILS_H
#define CIPHER_UTILS_H

#include <cstdio>       // For FILE, printf, scanf, getchar, fgets, fopen, fclose, fseek, ftell, fprintf
#include <cstring>      // For strcmp, strcspn, strlen, strrchr
#include <cctype>       // For isalpha (though not explicitly used in your pasted code, good to keep if needed elsewhere)
#include <dirent.h>     // For opendir, readdir, closedir, struct dirent, DIR
#include <ctime>        // For time_t, time, ctime

// Constants (C++ style preferred eventually, but #define is fine for now to get it compiling)
#define MAX_FILENAME 256
#define BUFFER_SIZE 4096
#define MAX_PEG 255
#define MIN_PEG 0
#define HISTORY_FILE "history.md"

// Function Declarations
int safe_input(char *buffer, int size);
void clear_stdin(void);
void handle_file_error(const char *filename);
int validate_file(const char *filename);
int validate_peg_value(int peg);
int has_txt_extension(const char *filename); // You had this commented out at the end, make sure to uncomment the declaration if you use it.

int encrypt_file(const char *input_file, const char *output_file, int pegs);
int decrypt_file(const char *input_file, const char *output_file, int pegs);
void read_file(const char *filename);
void search_files(void);
void view_history(void);
void log_encryption(const char *input_file, const char *output_file, int pegs);

#endif // CIPHER_UTILS_H
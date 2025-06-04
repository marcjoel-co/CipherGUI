/**
 * @file main.cpp
 * @brief Entry point for the Cipher GUI application.
 *
 * This file contains the main() function which initializes and runs the
 * GUI application defined in jay_gui.cpp.
 */

#include "jay_gui.h" // For run_gui_application()
#include <iostream>  

/**
 * @brief Main function for the Cipher GUI application.
 * @return int Returns 0 on successful execution, non-zero on failure.
 */
int main() {
    
    std::cout << "Starting Cipher GUI Application..." << std::endl; // Now std::cout is known

    // Run the GUI application loop defined in jay_gui.cpp
    // This function will block until the GUI is closed.
    int result = run_gui_application();

    if (result == 0) {
        std::cout << "Cipher GUI Application exited successfully." << std::endl;
    } else {
        std::cerr << "Cipher GUI Application exited with an error code: " << result << std::endl; // std::cerr also needs <iostream>
    }

    return result;
}
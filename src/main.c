/**
 * @file main.c
 * @brief Command-line interface and program entry point
 *
 * This file contains the main() function that serves as the entry point for the
 * NETTF file transfer tool. It handles command-line argument parsing, validation,
 * and dispatches to the appropriate client or server functionality. The program
 * supports two main modes: receive (server) and send (client).
 */

#include "platform.h"  // Cross-platform socket abstraction
#include <getopt.h>     // Not used but included for potential future CLI options

// Forward declarations for functions implemented in other modules
void send_file(const char *target_ip, int port, const char *filepath);
void receive_file(int port);

/**
 * @brief Display usage information for the program
 *
 * Prints the command syntax and provides examples of how to use the program
 * in both sender and receiver modes.
 *
 * @param program_name Name of the executable (typically argv[0])
 */
void print_usage(const char *program_name) {
    printf("Usage:\n");
    printf("  %s receive <PORT>\n", program_name);                                 // Receiver mode
    printf("  %s send <TARGET_IP> <PORT> <FILE_PATH>\n", program_name);             // Sender mode
    printf("\nExamples:\n");
    printf("  %s receive 8080\n", program_name);                                   // Receiver example
    printf("  %s send 192.168.1.100 8080 /path/to/file.txt\n", program_name);     // Sender example
}

/**
 * @brief Main program entry point
 *
 * This function handles command-line argument parsing and dispatches to the
 * appropriate functionality based on the provided arguments. The program
 * supports two main operational modes:
 *
 * 1. Receiver mode: ./nettf receive <PORT>
 *    - Starts a server that listens on the specified port
 *    - Waits for an incoming connection and receives a file
 *
 * 2. Sender mode: ./nettf send <TARGET_IP> <PORT> <FILE_PATH>
 *    - Connects to a receiver at the specified IP and port
 *    - Sends the specified file
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return EXIT_SUCCESS on successful execution, EXIT_FAILURE on error
 */
int main(int argc, char *argv[]) {
    // Validate minimum argument count (program name + command + at least one parameter)
    if (argc < 3) {
        print_usage(argv[0]);      // Show usage information
        return EXIT_FAILURE;       // Exit with error status
    }

    // Parse command: "receive" mode
    if (strcmp(argv[1], "receive") == 0) {
        // Receive mode requires exactly 3 arguments: program receive port
        if (argc != 3) {
            print_usage(argv[0]);  // Show correct usage
            return EXIT_FAILURE;
        }

        // Convert port argument from string to integer
        int port = atoi(argv[2]);

        // Validate port range: valid TCP ports are 1-65535
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Error: Port must be between 1 and 65535\n");
            return EXIT_FAILURE;
        }

        // Start the receiver (server) functionality
        receive_file(port);
    }
    // Parse command: "send" mode
    else if (strcmp(argv[1], "send") == 0) {
        // Send mode requires exactly 5 arguments: program send ip port filepath
        if (argc != 5) {
            print_usage(argv[0]);  // Show correct usage
            return EXIT_FAILURE;
        }

        // Extract command-line arguments
        const char *target_ip = argv[2];  // IP address of receiver
        int port = atoi(argv[3]);         // Port number
        const char *filepath = argv[4];   // Path to file to send

        // Validate port range
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Error: Port must be between 1 and 65535\n");
            return EXIT_FAILURE;
        }

        // Start the sender (client) functionality
        send_file(target_ip, port, filepath);
    }
    // Handle invalid commands
    else {
        fprintf(stderr, "Error: Invalid command '%s'\n", argv[1]);
        print_usage(argv[0]);          // Show available commands
        return EXIT_FAILURE;
    }

    // If we reach here, the operation completed successfully
    return EXIT_SUCCESS;
}
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
#include "discovery.h"  // Network discovery functionality
#include "protocol.h"   // Protocol definitions (includes DEFAULT_NETTF_PORT)
#include <getopt.h>     // Not used but included for potential future CLI options

// Forward declarations for functions implemented in other modules
void send_file(const char *target_ip, int port, const char *filepath, const char *target_dir);
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
    printf("  %s discover [--timeout <ms>]\n", program_name);                     // Discovery mode
    printf("  %s receive\n", program_name);                                         // Receiver mode
    printf("  %s send <TARGET_IP> <FILE_OR_DIR_PATH> [TARGET_DIR]\n", program_name); // Sender mode
    printf("\nOptions:\n");
    printf("  --timeout <ms> Set timeout for network operations (default: 1000ms)\n");
    printf("\nExamples:\n");
    printf("  %s discover\n", program_name);                                       // Discovery with port 9876 check
    printf("  %s receive\n", program_name);                                        // Receiver example
    printf("  %s send <TARGET_IP> /path/to/file.txt\n", program_name);            // File transfer example
    printf("  %s send <TARGET_IP> /path/to/file.txt downloads/\n", program_name);  // File with target dir
    printf("  %s send <TARGET_IP> /path/to/directory/\n", program_name);          // Directory transfer example
    printf("  %s send <TARGET_IP> /path/to/directory/ backups/\n", program_name);  // Directory with target dir
    printf("\nNote: All transfers use port %d by default.\n", DEFAULT_NETTF_PORT);
}

/**
 * @brief Main program entry point
 *
 * This function handles command-line argument parsing and dispatches to the
 * appropriate functionality based on the provided arguments. The program
 * supports three main operational modes:
 *
 * 1. Discovery mode: ./nettf discover [--services] [--timeout <ms>]
 *    - Scans local network for available devices
 *    - Optionally checks for NETTF service on discovered devices
 *
 * 2. Receiver mode: ./nettf receive <PORT>
 *    - Starts a server that listens on the specified port
 *    - Waits for an incoming connection and receives a file/directory
 *
 * 3. Sender mode: ./nettf send <TARGET_IP> <PORT> <FILE_OR_DIR_PATH>
 *    - Connects to a receiver at the specified IP and port
 *    - Sends the specified file or directory
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return EXIT_SUCCESS on successful execution, EXIT_FAILURE on error
 */
int main(int argc, char *argv[]) {
    // Validate minimum argument count (program name + command)
    if (argc < 2) {
        print_usage(argv[0]);      // Show usage information
        return EXIT_FAILURE;       // Exit with error status
    }

    // Parse command: "discover" mode
    if (strcmp(argv[1], "discover") == 0) {
        int timeout_ms = 1000;

        // Parse optional arguments
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
                timeout_ms = atoi(argv[i + 1]);
                if (timeout_ms <= 0) {
                    fprintf(stderr, "Error: Timeout must be a positive number\n");
                    return EXIT_FAILURE;
                }
                i++;  // Skip the timeout value
            } else {
                fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        }

        // Initialize network subsystem
        net_init();

        // Perform network discovery
        NetworkDevice devices[256];
        int device_count = discover_network_devices(devices, 256, 1, timeout_ms);

        if (device_count < 0) {
            fprintf(stderr, "Error: Network discovery failed\n");
            net_cleanup();
            return EXIT_FAILURE;
        }

        // Print results
        print_discovered_devices(devices, device_count, 0);  // show_services parameter no longer used

        // Print summary
        printf("\nDiscovery completed. Found %d device(s).\n", device_count);

        // Count NETTF services (always checked now)
        int nettfservices_count = 0;
        for (int i = 0; i < device_count; i++) {
            if (devices[i].has_nettf_service) {
                nettfservices_count++;
            }
        }
        printf("%d device(s) have NETTF service running on port %d.\n", nettfservices_count, DEFAULT_NETTF_PORT);

        net_cleanup();
        return EXIT_SUCCESS;
    }

    // Validate argument count for send/receive commands
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Parse command: "receive" mode
    if (strcmp(argv[1], "receive") == 0) {
        // Receive mode requires exactly 2 arguments: program receive
        if (argc != 2) {
            print_usage(argv[0]);  // Show correct usage
            return EXIT_FAILURE;
        }

        // Start the receiver (server) functionality with default port
        receive_file(DEFAULT_NETTF_PORT);
    }
    // Parse command: "send" mode
    else if (strcmp(argv[1], "send") == 0) {
        // Send mode requires 4 or 5 arguments: program send ip filepath [target_dir]
        if (argc < 4 || argc > 5) {
            print_usage(argv[0]);  // Show correct usage
            return EXIT_FAILURE;
        }

        // Extract command-line arguments
        const char *target_ip = argv[2];   // IP address of receiver
        const char *filepath = argv[3];    // Path to file to send
        const char *target_dir = NULL;     // Target directory (optional)

        // Check if target directory is provided
        if (argc == 5) {
            target_dir = argv[4];
        }

        // Start the sender (client) functionality with default port
        send_file(target_ip, DEFAULT_NETTF_PORT, filepath, target_dir);
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
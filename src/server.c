/**
 * @file server.c
 * @brief Server-side file receiving implementation
 *
 * This file implements the server (receiver) functionality for the NETTF file transfer tool.
 * The server is responsible for listening for incoming connections, accepting client
 * connections, and receiving files using the defined protocol. It includes comprehensive
 * error handling and proper resource management.
 */

#include "platform.h"  // Cross-platform socket abstraction
#include "protocol.h"  // File transfer protocol definitions
#include "signals.h"   // Signal handling

/**
 * @brief Start a server to receive files on a specific port
 *
 * This function implements the complete server-side file receiving workflow:
 * 1. Initialize network subsystem
 * 2. Create TCP socket
 * 3. Set socket options for address reuse
 * 4. Bind socket to local port
 * 5. Start listening for connections
 * 6. Accept incoming connection
 * 7. Receive file using protocol
 * 8. Clean up resources
 *
 * The server accepts only one connection, handles the file transfer, then exits.
 * All steps include comprehensive error handling with proper resource cleanup.
 *
 * @param port Port number to listen on (e.g., 8080)
 * @return Does not return on error (exits with EXIT_FAILURE)
 */
void receive_file(int port) {
    // Step 1: Initialize network subsystem
    // On Windows, this calls WSAStartup(); on POSIX systems, this does nothing
    net_init();

    // Step 2: Create TCP socket for IPv4 communication
    // AF_INET: IPv4 address family
    // SOCK_STREAM: TCP (reliable, connection-oriented)
    // 0: Let system choose protocol (TCP for SOCK_STREAM)
    SOCKET_T server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET_T) {
        perror("socket");              // Print system error for socket creation failure
        net_cleanup();                 // Clean up network subsystem before exit
        exit(EXIT_FAILURE);            // Cannot continue without a valid socket
    }

    // Optimize server socket for high-speed transfers
    optimize_socket(server_socket);

    // Step 3: Enable address reuse to prevent "address already in use" errors
    // This is useful when restarting the server quickly
    int opt = 1;                       // Boolean true for enabling option
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == SOCKET_ERROR) {
        perror("setsockopt");          // Print system error for socket option failure
        close_socket(server_socket);   // Clean up socket before exit
        net_cleanup();                 // Clean up network subsystem
        exit(EXIT_FAILURE);            // Cannot continue if option fails
    }

    // Step 4: Setup server address structure and bind to port
    SOCKADDR_IN_T server_addr;         // IPv4 address structure
    memset(&server_addr, 0, sizeof(server_addr));  // Zero-initialize structure
    server_addr.sin_family = AF_INET;              // IPv4 address family
    server_addr.sin_addr.s_addr = INADDR_ANY;       // Accept connections on any interface
    server_addr.sin_port = htons(port);            // Port in network byte order

    // Bind socket to local address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        perror("bind");                // Print system error for bind failure
        close_socket(server_socket);   // Clean up socket before exit
        net_cleanup();                 // Clean up network subsystem
        exit(EXIT_FAILURE);            // Cannot continue if bind fails
    }

    // Step 5: Start listening for incoming connections
    // Backlog of 1 means we only accept one pending connection at a time
    if (listen(server_socket, 1) == SOCKET_ERROR) {
        perror("listen");              // Print system error for listen failure
        close_socket(server_socket);   // Clean up socket before exit
        net_cleanup();                 // Clean up network subsystem
        exit(EXIT_FAILURE);            // Cannot continue if listen fails
    }

    // Display server status
    printf("Listening on port %d...\n", port);
    printf("Server started. Waiting for connections...\n");
    printf("Press Ctrl+C to stop the server\n\n");

    // Accept connections in a loop to handle multiple file transfers
    while (1) {
        // Check for shutdown signal
        int shutdown = signals_should_shutdown();
        if (shutdown == 1) {
            printf("\nShutdown requested. Press Ctrl+C again to force exit...\n");
            printf("Waiting for current transfer to complete...\n");
            signals_acknowledge_shutdown();
        } else if (shutdown == 2) {
            printf("\nForced exit! Closing server.\n");
            close_socket(server_socket);
            net_cleanup();
            exit(EXIT_FAILURE);
        }

        printf("Waiting for incoming connection...\n");

        // Accept incoming connection
        SOCKADDR_IN_T client_addr;         // Structure to hold client address information

        // Platform-specific address length type
#ifdef _WIN32
        int client_addr_len = sizeof(client_addr);      // Windows uses int
#else
        socklen_t client_addr_len = sizeof(client_addr); // POSIX uses socklen_t
#endif

        SOCKET_T client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET_T) {
            perror("accept");              // Print system error for accept failure
            continue;                     // Continue accepting other connections
        }

        // Optimize client socket for high-speed transfers
        optimize_socket(client_socket);

        // Convert client IP address to string for display
        char client_ip[INET_ADDRSTRLEN];   // Buffer for IP address string (IPv4 max 15 chars + null)
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Connection established from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // Detect transfer type and receive using appropriate protocol
        int transfer_type = detect_transfer_type(client_socket);
        if (transfer_type == -1) {
            fprintf(stderr, "Error detecting transfer type\n");
        } else if (transfer_type == 0) {
            // Standard file transfer
            if (recv_file_protocol(client_socket) != 0) {
                fprintf(stderr, "Error receiving file\n");
            }
        } else if (transfer_type == 1) {
            // Standard directory transfer
            if (recv_directory_protocol(client_socket) != 0) {
                fprintf(stderr, "Error receiving directory\n");
            }
        } else if (transfer_type == 2) {
            // File transfer with target directory
            if (recv_file_with_target_protocol(client_socket) != 0) {
                fprintf(stderr, "Error receiving file with target directory\n");
            }
        } else if (transfer_type == 3) {
            // Directory transfer with target directory
            if (recv_directory_with_target_protocol(client_socket) != 0) {
                fprintf(stderr, "Error receiving directory with target directory\n");
            }
        } else {
            fprintf(stderr, "Error: Unknown transfer type %d\n", transfer_type);
        }

        // Close client connection but keep server running
        close_socket(client_socket);
        printf("\nTransfer completed. Waiting for next connection...\n");
        printf("--------------------------------------------------\n");
    }

    // This code will never be reached due to infinite loop
    // close_socket(server_socket);       // Close server listening socket
    // net_cleanup();                     // Clean up network subsystem
}
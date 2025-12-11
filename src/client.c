/**
 * @file client.c
 * @brief Client-side file sending implementation
 *
 * This file implements the client (sender) functionality for the NETTF file transfer tool.
 * The client is responsible for initiating connections to a server and sending files
 * using the defined protocol. It includes comprehensive error handling and resource
 * cleanup on both success and failure paths.
 */

#include "platform.h"  // Cross-platform socket abstraction
#include "protocol.h"  // File transfer protocol definitions

/**
 * @brief Send a file to a remote server
 *
 * This function implements the complete client-side file sending workflow:
 * 1. Initialize network subsystem
 * 2. Create TCP socket
 * 3. Setup server address structure
 * 4. Connect to remote server
 * 5. Send file using protocol
 * 6. Clean up resources
 *
 * The function includes comprehensive error handling at each step and ensures
 * proper cleanup of resources on both success and failure paths.
 *
 * @param target_ip IP address of the receiver (e.g., "192.168.1.100")
 * @param port Port number the receiver is listening on (e.g., 8080)
 * @param filepath Path to the file to send
 * @return Does not return on error (exits with EXIT_FAILURE)
 */
void send_file(const char *target_ip, int port, const char *filepath) {
    // Step 1: Initialize network subsystem
    // On Windows, this calls WSAStartup(); on POSIX systems, this does nothing
    net_init();

    // Step 2: Create TCP socket for IPv4 communication
    // AF_INET: IPv4 address family
    // SOCK_STREAM: TCP (reliable, connection-oriented)
    // 0: Let system choose protocol (TCP for SOCK_STREAM)
    SOCKET_T client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET_T) {
        perror("socket");              // Print system error for socket creation failure
        net_cleanup();                 // Clean up network subsystem before exit
        exit(EXIT_FAILURE);            // Cannot continue without a valid socket
    }

    // Step 3: Setup server address structure
    SOCKADDR_IN_T server_addr;         // IPv4 address structure
    memset(&server_addr, 0, sizeof(server_addr));  // Zero-initialize structure
    server_addr.sin_family = AF_INET;  // IPv4 address family
    server_addr.sin_port = htons(port); // Port in network byte order

    // Convert IP address string to binary format
    // inet_pton: "Presentation to Network" - converts text IP to binary
    if (inet_pton(AF_INET, target_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", target_ip);
        close_socket(client_socket);   // Clean up socket before exit
        net_cleanup();                 // Clean up network subsystem
        exit(EXIT_FAILURE);            // Cannot continue with invalid IP
    }

    // Step 4: Connect to remote server
    // This initiates the TCP three-way handshake (SYN, SYN-ACK, ACK)
    printf("Connecting to %s:%d...\n", target_ip, port);
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        perror("connect");             // Print system error for connection failure
        close_socket(client_socket);   // Clean up socket before exit
        net_cleanup();                 // Clean up network subsystem
        exit(EXIT_FAILURE);            // Cannot continue if connection fails
    }

    // Step 5: Send file using the defined protocol
    printf("Connected! Sending file: %s\n", filepath);
    send_file_protocol(client_socket, filepath);

    // Step 6: Clean up resources on successful completion
    close_socket(client_socket);       // Close TCP connection
    net_cleanup();                     // Clean up network subsystem
}
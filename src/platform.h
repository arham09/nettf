/**
 * @file platform.h
 * @brief Cross-platform socket abstraction layer
 *
 * This header provides a unified interface for socket programming across
 * Windows (Winsock2) and POSIX systems (Linux/macOS). It normalizes
 * differences in socket types, error codes, and function signatures.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

// Platform detection: Check if we're compiling on Windows
#ifdef _WIN32
    #define IS_WINDOWS 1
    // Windows-specific headers
    #include <winsock2.h>     // Core Winsock functionality
    #include <ws2tcpip.h>     // Newer Winsock functions (inet_pton, etc.)

    // Type aliases to normalize Windows socket types
    #define SOCKET_T SOCKET           // Windows SOCKET type
    #define INVALID_SOCKET_T INVALID_SOCKET  // Windows invalid socket constant
    typedef struct sockaddr_in SOCKADDR_IN_T; // IPv4 address structure
#else
    #define IS_WINDOWS 0
    // POSIX (Linux/macOS) headers
    #include <sys/socket.h>   // Core socket functions
    #include <netinet/in.h>   // Internet address structures
    #include <arpa/inet.h>    // IP address manipulation functions
    #include <unistd.h>       // Unix standard functions (close)

    // Type aliases to normalize POSIX socket types
    typedef int SOCKET_T;           // POSIX: sockets are file descriptors (integers)
    #define INVALID_SOCKET_T -1     // POSIX: -1 indicates invalid socket
    #define SOCKET_ERROR -1         // POSIX: -1 indicates socket error
    typedef struct sockaddr_in SOCKADDR_IN_T; // IPv4 address structure
#endif

// Standard C library headers needed by all platforms
#include <stdio.h>      // Input/output functions (printf, perror, etc.)
#include <stdlib.h>     // Memory allocation, exit, etc.
#include <string.h>     // String manipulation functions

#ifdef _WIN32
    // Windows-specific: Automatically link Winsock library during compilation
    #pragma comment(lib, "ws2_32.lib")

    // Function declarations for Windows (htonll/ntohll not built-in)
    uint64_t htonll(uint64_t value);  // Host to Network byte order (64-bit)
    uint64_t ntohll(uint64_t value);  // Network to Host byte order (64-bit)
#elif !defined(__APPLE__) && !defined(__linux__)
    // Function declarations for systems that don't have htonll/ntohll
    uint64_t htonll(uint64_t value);  // Host to Network byte order (64-bit)
    uint64_t ntohll(uint64_t value);  // Network to Host byte order (64-bit)
#endif

// Function declarations for platform-independent socket operations
void net_init(void);      // Initialize network subsystem (WSAStartup on Windows)
void net_cleanup(void);   // Clean up network subsystem (WSACleanup on Windows)
void close_socket(SOCKET_T s);  // Close socket platform-independently
void optimize_socket(SOCKET_T s);  // Optimize socket for high-speed transfers

#endif // PLATFORM_H
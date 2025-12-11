/**
 * @file platform.c
 * @brief Cross-platform socket abstraction implementation
 *
 * This file implements platform-specific socket operations and provides
 * compatibility functions for network byte order conversion on different
 * operating systems.
 */

#include "platform.h"

// Platform-specific implementations for htonll/ntohll
// Only needed on Windows and systems that don't provide them
#if defined(_WIN32) || (!defined(__APPLE__) && !defined(__linux__))

/**
 * @brief Convert 64-bit value from host byte order to network byte order
 *
 * Network byte order is always big-endian. This function handles conversion
 * from little-endian (common on x86) to big-endian for network transmission.
 *
 * @param value 64-bit integer in host byte order
 * @return 64-bit integer in network byte order
 */
uint64_t htonll(uint64_t value) {
    // Check if system is already big-endian by converting 1
    // If htonl(1) == 1, system is big-endian (network byte order)
    if (htonl(1) == 1) {
        return value; // No conversion needed on big-endian systems
    } else {
        // Little-endian system: need to swap bytes
        // Split 64-bit value into two 32-bit parts
        uint64_t lower = htonl(value & 0xFFFFFFFF);           // Convert lower 32 bits
        uint64_t upper = htonl((value >> 32) & 0xFFFFFFFF);   // Convert upper 32 bits
        return (upper << 32) | lower;                         // Reassemble with swapped order
    }
}

/**
 * @brief Convert 64-bit value from network byte order to host byte order
 *
 * This function is the inverse of htonll(). It converts data received
 * from the network (big-endian) to the host system's byte order.
 *
 * @param value 64-bit integer in network byte order
 * @return 64-bit integer in host byte order
 */
uint64_t ntohll(uint64_t value) {
    // Check if system is already big-endian
    if (ntohl(1) == 1) {
        return value; // No conversion needed on big-endian systems
    } else {
        // Little-endian system: need to swap bytes
        uint64_t lower = ntohl(value & 0xFFFFFFFF);           // Convert lower 32 bits
        uint64_t upper = ntohl((value >> 32) & 0xFFFFFFFF);   // Convert upper 32 bits
        return (upper << 32) | lower;                         // Reassemble with swapped order
    }
}

#endif

/**
 * @brief Initialize the network subsystem
 *
 * On Windows, this must be called before any socket operations to
 * initialize the Winsock library. On POSIX systems, this function
 * does nothing as no initialization is required.
 *
 * Terminates the program on failure with EXIT_FAILURE.
 */
void net_init(void) {
#ifdef _WIN32
    WSADATA wsaData;  // Structure to receive Winsock implementation details

    // Request Winsock version 2.2 (MAKEWORD combines major/minor versions)
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        fprintf(stderr, "WSAStartup failed with error: %d\n", result);
        exit(EXIT_FAILURE);  // Cannot continue without Winsock
    }

    // Verify that the loaded Winsock DLL supports the requested version
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        fprintf(stderr, "Could not find a usable version of Winsock.dll\n");
        WSACleanup();  // Clean up the loaded version before exiting
        exit(EXIT_FAILURE);
    }
#else
    // POSIX systems (Linux/macOS) don't need explicit socket initialization
#endif
}

/**
 * @brief Clean up the network subsystem
 *
 * On Windows, this cleans up the Winsock library and should be called
 * when the program is done with network operations. On POSIX systems,
 * this function does nothing.
 */
void net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();  // Release Winsock resources
#else
    // No cleanup needed on POSIX systems
#endif
}

/**
 * @brief Close a socket in a platform-independent manner
 *
 * Uses the appropriate function call based on the platform:
 * - Windows: closesocket()
 * - POSIX: close()
 *
 * @param s Socket descriptor to close
 */
void close_socket(SOCKET_T s) {
#ifdef _WIN32
    closesocket(s);  // Windows-specific socket close function
#else
    close(s);        // POSIX: sockets are file descriptors, use close()
#endif
}
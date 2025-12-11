/**
 * @file protocol.h
 * @brief File transfer protocol definitions and function declarations
 *
 * This file defines the file transfer protocol used by the NETTF file transfer tool.
 * The protocol uses a simple binary format with a fixed-size header followed by
 * the filename and file content. All multi-byte integers are transmitted in
 * network byte order (big-endian) for cross-platform compatibility.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "platform.h"  // Cross-platform socket types and functions
#include <sys/stat.h>   // File status operations (stat() for file size)
#include <time.h>       // Time functions for transfer speed calculation

// Protocol constants
#define HEADER_SIZE 16    // Total header size: 8 bytes (file_size) + 8 bytes (filename_len)
#define CHUNK_SIZE 4096   // Size of file chunks for transfer (4KB optimal for network efficiency)

/**
 * @brief Protocol header structure for file transfer metadata
 *
 * This structure is transmitted at the beginning of each file transfer.
 * All fields are converted to network byte order before transmission.
 *
 * Updated to support very large files:
 * - file_size: uint64_t (up to 16 exabytes)
 * - filename_len: uint64_t (up to 16 exabytes)
 * This eliminates the 4GB limitation for both file size and filename length.
 */
typedef struct {
    uint64_t file_size;    // Size of file in bytes (8 bytes, up to 16 exabytes)
    uint64_t filename_len; // Length of filename in bytes (8 bytes, up to 16 exabytes)
} FileHeader;

// Function declarations for protocol operations

/**
 * @brief Send all bytes from a buffer, handling partial sends
 *
 * TCP may send fewer bytes than requested. This function ensures that
 * all data is sent by repeatedly calling send() until the entire buffer
 * has been transmitted.
 *
 * @param s Socket descriptor
 * @param data Pointer to data buffer
 * @param len Number of bytes to send
 * @return 0 on success, -1 on error
 */
int send_all(SOCKET_T s, const void *data, size_t len);

/**
 * @brief Receive all bytes into a buffer, handling partial receives
 *
 * TCP may receive fewer bytes than requested. This function ensures that
 * all expected data is received by repeatedly calling recv() until the
 * entire buffer has been filled.
 *
 * @param s Socket descriptor
 * @param buf Pointer to receive buffer
 * @param len Number of bytes to receive
 * @return 0 on success, -1 on error
 */
int recv_all(SOCKET_T s, void *buf, size_t len);

/**
 * @brief Send a file using the defined protocol
 *
 * This function implements the complete file sending protocol:
 * 1. Get file size using stat()
 * 2. Extract filename from path (security: strip directory components)
 * 3. Send header with file_size and filename_len in network byte order
 * 4. Send filename
 * 5. Send file content in CHUNK_SIZE chunks with progress tracking
 *
 * @param s Socket descriptor
 * @param filepath Path to file to send
 * @return Does not return on error (exits with EXIT_FAILURE)
 */
void send_file_protocol(SOCKET_T s, const char *filepath);

/**
 * @brief Receive a file using the defined protocol
 *
 * This function implements the complete file receiving protocol:
 * 1. Receive header (12 bytes)
 * 2. Convert file_size and filename_len from network to host byte order
 * 3. Allocate memory for filename and receive it
 * 4. Create file locally (saves to current directory)
 * 5. Receive file content in chunks until file_size bytes received
 *
 * @param s Socket descriptor
 * @return 0 on success, -1 on error
 */
int recv_file_protocol(SOCKET_T s);

// Helper functions for transfer progress display
void format_bytes(uint64_t bytes, char *buffer, size_t buffer_size);
void format_speed(double bytes_per_sec, char *buffer, size_t buffer_size);
void format_time(int seconds, char *buffer, size_t buffer_size);

#endif // PROTOCOL_H
/**
 * @file protocol.c
 * @brief File transfer protocol implementation
 *
 * This file implements the file transfer protocol including reliable data
 * transmission, file metadata handling, and progress tracking. The protocol
 * ensures cross-platform compatibility through network byte order conversion
 * and secure filename handling.
 */

#include "protocol.h"
#include <errno.h>  // For error codes (perror functionality)

/**
 * @brief Send all bytes from a buffer, handling partial sends
 *
 * TCP sockets may send fewer bytes than requested due to network conditions,
 * internal buffering, or flow control. This function ensures reliable transmission
 * by repeatedly calling send() until the entire buffer has been sent.
 *
 * @param s Socket descriptor to send data through
 * @param data Pointer to data buffer to send
 * @param len Number of bytes to send
 * @return 0 on success, -1 on error (prints error message)
 */
int send_all(SOCKET_T s, const void *data, size_t len) {
    const char *ptr = (const char *)data;  // Cast to char pointer for byte-level operations
    size_t total_sent = 0;                 // Track total bytes sent so far

    // Keep sending until all bytes have been transmitted
    while (total_sent < len) {
        // Attempt to send remaining bytes
        ssize_t sent = send(s, ptr + total_sent, len - total_sent, 0);

        if (sent == SOCKET_ERROR) {
            perror("send");  // Print system error message
            return -1;       // Return error indicator
        }

        if (sent == 0) {
            fprintf(stderr, "Connection closed by peer\n");
            return -1;       // Return error if connection closed unexpectedly
        }

        total_sent += sent;  // Update progress counter
    }
    return 0;  // Success: all bytes sent
}

/**
 * @brief Receive all bytes into a buffer, handling partial receives
 *
 * TCP sockets may receive fewer bytes than requested due to network conditions
 * or data arriving in multiple packets. This function ensures complete reception
 * by repeatedly calling recv() until the expected number of bytes has been received.
 *
 * @param s Socket descriptor to receive data from
 * @param buf Pointer to receive buffer
 * @param len Number of bytes to receive
 * @return 0 on success, -1 on error (prints error message)
 */
int recv_all(SOCKET_T s, void *buf, size_t len) {
    char *ptr = (char *)buf;           // Cast to char pointer for byte-level operations
    size_t total_received = 0;         // Track total bytes received so far

    // Keep receiving until all expected bytes have arrived
    while (total_received < len) {
        // Attempt to receive remaining bytes
        ssize_t received = recv(s, ptr + total_received, len - total_received, 0);

        if (received == SOCKET_ERROR) {
            perror("recv");  // Print system error message
            return -1;       // Return error indicator
        }

        if (received == 0) {
            fprintf(stderr, "Connection closed by peer\n");
            return -1;       // Return error if connection closed unexpectedly
        }

        total_received += received;  // Update progress counter
    }
    return 0;  // Success: all bytes received
}

/**
 * @brief Send a file using the defined file transfer protocol
 *
 * This function implements the complete file sending protocol with error handling,
 * security features, and progress tracking. It follows these steps:
 * 1. Open file in binary mode (critical for file integrity)
 * 2. Get file size using stat()
 * 3. Extract filename from path (security feature)
 * 4. Send header with metadata in network byte order
 * 5. Send filename
 * 6. Send file content in chunks with progress display
 *
 * @param s Socket descriptor to send file through
 * @param filepath Path to the file to send
 * @return Does not return on error (exits with EXIT_FAILURE)
 */
void send_file_protocol(SOCKET_T s, const char *filepath) {
    // Open file in binary read mode - "rb" is critical for preserving file integrity
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("fopen");      // Print system error for file open failure
        exit(EXIT_FAILURE);   // Terminate program on critical error
    }

    // Get file metadata (especially size) using stat() system call
    struct stat st;
    if (stat(filepath, &st) != 0) {
        perror("stat");       // Print system error for stat failure
        fclose(file);         // Clean up: close opened file
        exit(EXIT_FAILURE);   // Terminate program on critical error
    }

    uint64_t file_size = st.st_size;  // Extract file size (up to 16 exabytes with uint64_t)

    // Security feature: Extract filename without directory path
    // This prevents path traversal attacks on the receiver
    const char *filename = strrchr(filepath, '/');  // Look for Unix path separator
#ifdef _WIN32
    // Also check Windows path separator
    const char *filename_win = strrchr(filepath, '\\');
    if (filename_win > filename) filename = filename_win;
#endif

    if (!filename) {
        filename = filepath;    // No path separator, use original path as filename
    } else {
        filename++;             // Move past the path separator to get just filename
    }

    uint64_t filename_len = strlen(filename);  // Get filename length for protocol

    // Prepare protocol header with network byte order conversion
    FileHeader header;
    header.file_size = htonll(file_size);      // Convert 64-bit size to network byte order
    header.filename_len = htonll(filename_len); // Convert 64-bit length to network byte order

    // Send protocol header (16 bytes total)
    if (send_all(s, &header, HEADER_SIZE) != 0) {
        fclose(file);  // Clean up on error
        exit(EXIT_FAILURE);
    }

    // Send filename (variable length)
    if (send_all(s, filename, filename_len) != 0) {
        fclose(file);  // Clean up on error
        exit(EXIT_FAILURE);
    }

    // Send file content in chunks with enhanced progress tracking
    char buffer[CHUNK_SIZE];        // 4KB buffer for efficient I/O
    size_t bytes_read;              // Number of bytes read from file
    uint64_t total_sent = 0;        // Track total progress

    // Time tracking for transfer speed calculation
    time_t start_time = time(NULL);
    time_t last_update = start_time;

    // Read and send file in chunks until EOF
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        if (send_all(s, buffer, bytes_read) != 0) {
            fclose(file);           // Clean up on error
            exit(EXIT_FAILURE);
        }
        total_sent += bytes_read;   // Update progress counter

        // Update progress display every second
        time_t current_time = time(NULL);
        if (current_time != last_update || total_sent == file_size) {
            double elapsed_seconds = difftime(current_time, start_time);
            double speed = elapsed_seconds > 0 ? (double)total_sent / elapsed_seconds : 0;

            // Calculate estimated time remaining
            int eta_seconds = 0;
            if (speed > 0 && total_sent < file_size) {
                eta_seconds = (int)((double)(file_size - total_sent) / speed);
            }

            // Format all display components
            char total_str[32], sent_str[32], speed_str[32], eta_str[32], elapsed_str[32];
            format_bytes(file_size, total_str, sizeof(total_str));
            format_bytes(total_sent, sent_str, sizeof(sent_str));
            format_speed(speed, speed_str, sizeof(speed_str));
            format_time(eta_seconds, eta_str, sizeof(eta_str));
            format_time((int)elapsed_seconds, elapsed_str, sizeof(elapsed_str));

            // Display comprehensive progress information
            printf("\r\033[K");  // Clear current line
            printf("Progress: %.2f%% | %s/%s | Speed: %s | Elapsed: %s | ETA: %s",
                   (double)total_sent / file_size * 100,
                   sent_str, total_str, speed_str, elapsed_str, eta_str);
            fflush(stdout);

            last_update = current_time;
        }
    }

    // Check for file read errors (distinguish from EOF)
    if (ferror(file)) {
        perror("fread");           // Print file read error
        fclose(file);              // Clean up on error
        exit(EXIT_FAILURE);        // Terminate on file error
    }

    fclose(file);  // Clean up file handle
    printf("\nFile sent successfully!\n");  // Success message on new line
}

/**
 * @brief Receive a file using the defined file transfer protocol
 *
 * This function implements the complete file receiving protocol with error handling
 * and progress tracking. It follows these steps:
 * 1. Receive 12-byte protocol header
 * 2. Convert metadata from network to host byte order
 * 3. Allocate memory and receive filename
 * 4. Create file locally (in current directory)
 * 5. Receive file content in chunks until complete
 * 6. Clean up memory and close file
 *
 * @param s Socket descriptor to receive file from
 * @return 0 on success, -1 on error (prints error message)
 */
int recv_file_protocol(SOCKET_T s) {
    FileHeader header;  // Buffer for protocol header

    // Step 1: Receive 16-byte protocol header (8 bytes file_size + 8 bytes filename_len)
    if (recv_all(s, &header, HEADER_SIZE) != 0) {
        return -1;  // Error message already printed by recv_all()
    }

    // Step 2: Convert metadata from network byte order to host byte order
    uint64_t file_size = ntohll(header.file_size);      // Convert 64-bit file size
    uint64_t filename_len = ntohll(header.filename_len); // Convert 64-bit filename length

    // Step 3: Allocate memory for filename (+1 for null terminator)
    char *filename = malloc(filename_len + 1);
    if (!filename) {
        perror("malloc");  // Print memory allocation error
        return -1;
    }

    // Receive filename data
    if (recv_all(s, filename, filename_len) != 0) {
        free(filename);    // Clean up memory on error
        return -1;
    }
    filename[filename_len] = '\0';  // Null-terminate the filename string

    // Display file information
    printf("Receiving file: %s (%llu bytes)\n", filename, (unsigned long long)file_size);

    // Step 4: Create file locally in binary write mode
    // File will be created in current working directory
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("fopen");     // Print file creation error
        free(filename);      // Clean up memory on error
        return -1;
    }

    // Step 5: Receive file content in chunks with enhanced progress tracking
    char buffer[CHUNK_SIZE];      // Buffer for receiving file chunks
    uint64_t total_received = 0;  // Track progress

    // Time tracking for transfer speed calculation
    time_t start_time = time(NULL);
    time_t last_update = start_time;

    // Keep receiving until all file bytes have been received
    while (total_received < file_size) {
        // Calculate how many bytes to receive this iteration
        size_t to_receive = file_size - total_received;  // Remaining bytes
        if (to_receive > CHUNK_SIZE) {
            to_receive = CHUNK_SIZE;  // Limit to chunk size
        }

        // Receive chunk data from network
        if (recv_all(s, buffer, to_receive) != 0) {
            fclose(file);      // Clean up file handle
            free(filename);    // Clean up memory
            return -1;
        }

        // Write received data to file
        if (fwrite(buffer, 1, to_receive, file) != to_receive) {
            perror("fwrite");  // Print file write error
            fclose(file);      // Clean up file handle
            free(filename);    // Clean up memory
            return -1;
        }

        total_received += to_receive;  // Update progress counter

        // Update progress display every second
        time_t current_time = time(NULL);
        if (current_time != last_update || total_received == file_size) {
            double elapsed_seconds = difftime(current_time, start_time);
            double speed = elapsed_seconds > 0 ? (double)total_received / elapsed_seconds : 0;

            // Calculate estimated time remaining
            int eta_seconds = 0;
            if (speed > 0 && total_received < file_size) {
                eta_seconds = (int)((double)(file_size - total_received) / speed);
            }

            // Format all display components
            char total_str[32], received_str[32], speed_str[32], eta_str[32], elapsed_str[32];
            format_bytes(file_size, total_str, sizeof(total_str));
            format_bytes(total_received, received_str, sizeof(received_str));
            format_speed(speed, speed_str, sizeof(speed_str));
            format_time(eta_seconds, eta_str, sizeof(eta_str));
            format_time((int)elapsed_seconds, elapsed_str, sizeof(elapsed_str));

            // Display comprehensive progress information
            printf("\r\033[K");  // Clear current line
            printf("Progress: %.2f%% | %s/%s | Speed: %s | Elapsed: %s | ETA: %s",
                   (double)total_received / file_size * 100,
                   received_str, total_str, speed_str, elapsed_str, eta_str);
            fflush(stdout);

            last_update = current_time;
        }
    }

    // Step 6: Clean up resources
    fclose(file);       // Close file handle
    free(filename);     // Free allocated memory
    printf("\nFile received successfully!\n");  // Success message on new line

    return 0;  // Success
}

/**
 * @brief Format bytes into human-readable format (KB, MB, GB, etc.)
 *
 * @param bytes Number of bytes to format
 * @param buffer Buffer to store formatted string
 * @param buffer_size Size of the buffer
 */
void format_bytes(uint64_t bytes, char *buffer, size_t buffer_size) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unit_index = 0;
    double size = (double)bytes;

    while (size >= 1024.0 && unit_index < 5) {
        size /= 1024.0;
        unit_index++;
    }

    if (unit_index == 0) {
        snprintf(buffer, buffer_size, "%.0f %s", size, units[unit_index]);
    } else {
        snprintf(buffer, buffer_size, "%.2f %s", size, units[unit_index]);
    }
}

/**
 * @brief Format transfer speed into human-readable format
 *
 * @param bytes_per_sec Transfer speed in bytes per second
 * @param buffer Buffer to store formatted string
 * @param buffer_size Size of the buffer
 */
void format_speed(double bytes_per_sec, char *buffer, size_t buffer_size) {
    char speed_buffer[32];
    format_bytes((uint64_t)bytes_per_sec, speed_buffer, sizeof(speed_buffer));
    snprintf(buffer, buffer_size, "%s/s", speed_buffer);
}

/**
 * @brief Format seconds into human-readable time format
 *
 * @param seconds Number of seconds to format
 * @param buffer Buffer to store formatted string
 * @param buffer_size Size of the buffer
 */
void format_time(int seconds, char *buffer, size_t buffer_size) {
    if (seconds < 60) {
        snprintf(buffer, buffer_size, "%ds", seconds);
    } else if (seconds < 3600) {
        int minutes = seconds / 60;
        int secs = seconds % 60;
        snprintf(buffer, buffer_size, "%dm %ds", minutes, secs);
    } else {
        int hours = seconds / 3600;
        int minutes = (seconds % 3600) / 60;
        int secs = seconds % 60;
        snprintf(buffer, buffer_size, "%dh %dm %ds", hours, minutes, secs);
    }
}
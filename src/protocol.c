/**
 * @file protocol.c
 * @brief File transfer protocol implementation
 *
 * This file implements the file transfer protocol including reliable data
 * transmission, file metadata handling, and progress tracking. The protocol
 * ensures cross-platform compatibility through network byte order conversion
 * and secure filename handling.
 */

#define _GNU_SOURCE  // Enable strdup() on Linux systems
#include "protocol.h"
#include "adaptive.h"    // Adaptive chunk sizing
#include "signals.h"     // Signal handling
#include <errno.h>  // For error codes (perror functionality)
#include <dirent.h> // For directory operations
#include <string.h> // For string manipulation functions

// Maximum chunk size for buffer allocation (from adaptive.h)
#define MAX_CHUNK_BUFFER_SIZE (2 * 1024 * 1024)  // 2 MB

// Define htonll/ntohll for systems that don't have them (like Linux)
#ifndef htonll
static inline uint64_t htonll(uint64_t value) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32) | htonl(value >> 32);
    #else
        return value;
    #endif
}
#endif

#ifndef ntohll
static inline uint64_t ntohll(uint64_t value) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return ((uint64_t)ntohl(value & 0xFFFFFFFF) << 32) | ntohl(value >> 32);
    #else
        return value;
    #endif
}
#endif

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

    // Initialize adaptive chunk sizing
    AdaptiveState adaptive;
    adaptive_init(&adaptive, file_size);

    // Prepare protocol header with network byte order conversion
    FileHeader header;
    header.file_size = htonll(file_size);      // Convert 64-bit size to network byte order
    header.filename_len = htonll(filename_len); // Convert 64-bit length to network byte order

    // Send magic number to indicate file transfer
    uint32_t magic = htonl(FILE_MAGIC);
    if (send_all(s, &magic, MAGIC_SIZE) != 0) {
        fclose(file);  // Clean up on error
        exit(EXIT_FAILURE);
    }

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
    char *buffer = malloc(MAX_CHUNK_BUFFER_SIZE);  // Allocate max buffer size
    if (!buffer) {
        perror("malloc");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    size_t bytes_read;              // Number of bytes read from file
    uint64_t total_sent = 0;        // Track total progress

    // Time tracking for transfer speed calculation
    time_t start_time = time(NULL);
    time_t last_update = start_time;
    time_t chunk_start = start_time;

    // Read and send file in chunks until EOF
    size_t chunk_size = adaptive_get_chunk_size(&adaptive);
    while ((bytes_read = fread(buffer, 1, chunk_size, file)) > 0) {
        time_t chunk_end = time(NULL);
        double chunk_elapsed = difftime(chunk_end, chunk_start);
        chunk_start = chunk_end;

        if (send_all(s, buffer, bytes_read) != 0) {
            free(buffer);
            fclose(file);           // Clean up on error
            exit(EXIT_FAILURE);
        }

        // Update adaptive state
        adaptive_update(&adaptive, bytes_read, chunk_elapsed);

        total_sent += bytes_read;   // Update progress counter

        // Check for shutdown signal
        int shutdown = signals_should_shutdown();
        if (shutdown == 1) {
            printf("\nShutdown requested. Press Ctrl+C again to force exit...\n");
            signals_acknowledge_shutdown();
        } else if (shutdown == 2) {
            printf("\nForced exit! File may be incomplete.\n");
            free(buffer);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        // Update progress display every second
        time_t current_time = time(NULL);
        if (current_time != last_update || total_sent == file_size) {
            double elapsed_seconds = difftime(current_time, start_time);
            double speed = elapsed_seconds > 0 ? (double)total_sent / elapsed_seconds : 0;

            // Get current chunk size for display
            chunk_size = adaptive_get_chunk_size(&adaptive);
            char chunk_str[32];
            adaptive_format_chunk_size(chunk_size, chunk_str, sizeof(chunk_str));

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
            printf("Progress: %.2f%% | %s/%s | Speed: %s | Chunk: %s | Elapsed: %s | ETA: %s",
                   (double)total_sent / file_size * 100,
                   sent_str, total_str, speed_str, chunk_str, elapsed_str, eta_str);
            fflush(stdout);

            last_update = current_time;
        }

        // Get next chunk size (may have changed)
        chunk_size = adaptive_get_chunk_size(&adaptive);
    }

    // Check for file read errors (distinguish from EOF)
    if (ferror(file)) {
        perror("fread");           // Print file read error
        free(buffer);
        fclose(file);              // Clean up on error
        exit(EXIT_FAILURE);        // Terminate on file error
    }

    free(buffer);
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

    // Initialize adaptive chunk sizing
    AdaptiveState adaptive;
    adaptive_init(&adaptive, file_size);

    // Step 4: Create file locally in binary write mode
    // File will be created in current working directory
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("fopen");     // Print file creation error
        free(filename);      // Clean up memory on error
        return -1;
    }

    // Step 5: Receive file content in chunks with enhanced progress tracking
    char *buffer = malloc(MAX_CHUNK_BUFFER_SIZE);  // Allocate max buffer size
    if (!buffer) {
        perror("malloc");
        fclose(file);
        free(filename);
        return -1;
    }

    uint64_t total_received = 0;  // Track progress

    // Time tracking for transfer speed calculation
    time_t start_time = time(NULL);
    time_t last_update = start_time;
    time_t chunk_start = start_time;

    // Keep receiving until all file bytes have been received
    while (total_received < file_size) {
        // Calculate how many bytes to receive this iteration
        size_t chunk_size = adaptive_get_chunk_size(&adaptive);
        size_t to_receive = file_size - total_received;  // Remaining bytes
        if (to_receive > chunk_size) {
            to_receive = chunk_size;  // Limit to chunk size
        }

        // Receive chunk data from network
        if (recv_all(s, buffer, to_receive) != 0) {
            fclose(file);      // Clean up file handle
            free(buffer);
            free(filename);    // Clean up memory
            return -1;
        }

        time_t chunk_end = time(NULL);
        double chunk_elapsed = difftime(chunk_end, chunk_start);
        chunk_start = chunk_end;

        // Write received data to file
        if (fwrite(buffer, 1, to_receive, file) != to_receive) {
            perror("fwrite");  // Print file write error
            fclose(file);      // Clean up file handle
            free(buffer);
            free(filename);    // Clean up memory
            return -1;
        }

        // Update adaptive state
        adaptive_update(&adaptive, to_receive, chunk_elapsed);

        total_received += to_receive;  // Update progress counter

        // Check for shutdown signal
        int shutdown = signals_should_shutdown();
        if (shutdown == 1) {
            printf("\nShutdown requested. Press Ctrl+C again to force exit...\n");
            signals_acknowledge_shutdown();
        } else if (shutdown == 2) {
            printf("\nForced exit! File may be incomplete.\n");
            fclose(file);
            free(buffer);
            free(filename);
            exit(EXIT_FAILURE);
        }

        // Update progress display every second
        time_t current_time = time(NULL);
        if (current_time != last_update || total_received == file_size) {
            double elapsed_seconds = difftime(current_time, start_time);
            double speed = elapsed_seconds > 0 ? (double)total_received / elapsed_seconds : 0;

            // Get current chunk size for display
            chunk_size = adaptive_get_chunk_size(&adaptive);
            char chunk_str[32];
            adaptive_format_chunk_size(chunk_size, chunk_str, sizeof(chunk_str));

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
            printf("Progress: %.2f%% | %s/%s | Speed: %s | Chunk: %s | Elapsed: %s | ETA: %s",
                   (double)total_received / file_size * 100,
                   received_str, total_str, speed_str, chunk_str, elapsed_str, eta_str);
            fflush(stdout);

            last_update = current_time;
        }
    }

    // Step 6: Clean up resources
    fclose(file);       // Close file handle
    free(buffer);
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

/**
 * @brief Send a single file within a directory with relative path
 *
 * @param s Socket descriptor
 * @param base_path Base directory path
 * @param relative_path Relative path from base
 */
void send_single_file_in_dir(SOCKET_T s, const char *base_path, const char *relative_path) {
    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, relative_path);

    // Open file in binary read mode
    FILE *file = fopen(full_path, "rb");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Get file metadata
    struct stat st;
    if (stat(full_path, &st) != 0) {
        perror("stat");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    uint64_t file_size = st.st_size;
    uint64_t rel_path_len = strlen(relative_path);

    // Initialize adaptive chunk sizing
    AdaptiveState adaptive;
    adaptive_init(&adaptive, file_size);

    // Send file header with relative path
    FileHeader header;
    header.file_size = htonll(file_size);
    header.filename_len = htonll(rel_path_len);

    if (send_all(s, &header, HEADER_SIZE) != 0) {
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Send relative path
    if (send_all(s, relative_path, rel_path_len) != 0) {
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Send file content in chunks
    char *buffer = malloc(MAX_CHUNK_BUFFER_SIZE);
    if (!buffer) {
        perror("malloc");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    size_t bytes_read;
    size_t chunk_size = adaptive_get_chunk_size(&adaptive);
    time_t chunk_start = time(NULL);

    while ((bytes_read = fread(buffer, 1, chunk_size, file)) > 0) {
        time_t chunk_end = time(NULL);
        double chunk_elapsed = difftime(chunk_end, chunk_start);
        chunk_start = chunk_end;

        if (send_all(s, buffer, bytes_read) != 0) {
            free(buffer);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        adaptive_update(&adaptive, bytes_read, chunk_elapsed);
        chunk_size = adaptive_get_chunk_size(&adaptive);

        // Check for shutdown signal
        int shutdown = signals_should_shutdown();
        if (shutdown == 1) {
            printf("\nShutdown requested. Press Ctrl+C again to force exit...\n");
            signals_acknowledge_shutdown();
        } else if (shutdown == 2) {
            printf("\nForced exit!\n");
            free(buffer);
            fclose(file);
            exit(EXIT_FAILURE);
        }
    }

    if (ferror(file)) {
        perror("fread");
        free(buffer);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    free(buffer);
    fclose(file);
}

/**
 * @brief Recursively send all files in a directory
 *
 * @param s Socket descriptor
 * @param base_path Base directory path
 * @param current_path Current directory path (relative to base)
 */
void send_directory_recursive(SOCKET_T s, const char *base_path, const char *current_path) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[4096];
    char relative_path[4096];

    // Open current directory
    if (current_path[0] == '\0') {
        snprintf(full_path, sizeof(full_path), "%s", base_path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, current_path);
    }

    dir = opendir(full_path);
    if (!dir) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct relative path
        if (current_path[0] == '\0') {
            snprintf(relative_path, sizeof(relative_path), "%s", entry->d_name);
        } else {
            snprintf(relative_path, sizeof(relative_path), "%s/%s", current_path, entry->d_name);
        }

        // Construct full path for stat
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, relative_path);

        // Get file/directory status
        if (stat(full_path, &st) != 0) {
            closedir(dir);
            perror("stat");
            exit(EXIT_FAILURE);
        }

        if (S_ISDIR(st.st_mode)) {
            // Recursively process subdirectory
            send_directory_recursive(s, base_path, relative_path);
        } else if (S_ISREG(st.st_mode)) {
            // Send regular file
            send_single_file_in_dir(s, base_path, relative_path);
        }
    }

    closedir(dir);
}

/**
 * @brief Send a directory using the defined protocol
 */
void send_directory_protocol(SOCKET_T s, const char *dirpath) {

    uint64_t total_files, total_size;

    // Count files and calculate total size
    if (count_directory_files(dirpath, &total_files, &total_size) != 0) {
        exit(EXIT_FAILURE);
    }

    // Extract base directory name
    const char *base_name = strrchr(dirpath, '/');
#ifdef _WIN32
    const char *base_name_win = strrchr(dirpath, '\\');
    if (base_name_win > base_name) base_name = base_name_win;
#endif

    if (!base_name) {
        base_name = dirpath;
    } else {
        base_name++;
    }

    uint64_t base_name_len = strlen(base_name);

    // Send magic number to indicate directory transfer
    uint32_t magic = htonl(DIR_MAGIC);
    if (send_all(s, &magic, MAGIC_SIZE) != 0) {
        exit(EXIT_FAILURE);
    }

    // Send directory header
    DirectoryHeader dir_header;
    dir_header.total_files = htonll(total_files);
    dir_header.total_size = htonll(total_size);
    dir_header.base_path_len = htonll(base_name_len);

    if (send_all(s, &dir_header, DIR_HEADER_SIZE) != 0) {
        exit(EXIT_FAILURE);
    }

    // Send base directory name
    if (send_all(s, base_name, base_name_len) != 0) {
        exit(EXIT_FAILURE);
    }

    printf("Sending directory: %s (%llu files, ", base_name, (unsigned long long)total_files);

    char size_str[32];
    format_bytes(total_size, size_str, sizeof(size_str));
    printf("%s total)\n", size_str);

    // Send all files recursively
    time_t start_time = time(NULL);

    send_directory_recursive(s, dirpath, "");

    // Send end marker (file_size = 0, filename_len = 0)
    FileHeader end_header;
    end_header.file_size = htonll(0);
    end_header.filename_len = htonll(0);
    if (send_all(s, &end_header, HEADER_SIZE) != 0) {
        exit(EXIT_FAILURE);
    }

    // Display final statistics
    time_t end_time = time(NULL);
    double elapsed_seconds = difftime(end_time, start_time);
    double speed = elapsed_seconds > 0 ? (double)total_size / elapsed_seconds : 0;

    char speed_str[32], elapsed_str[32];
    format_speed(speed, speed_str, sizeof(speed_str));
    format_time((int)elapsed_seconds, elapsed_str, sizeof(elapsed_str));

    printf("\nDirectory sent successfully!\n");
    printf("Total: %llu files, %s transferred\n",
           (unsigned long long)total_files, size_str);
    printf("Average speed: %s | Total time: %s\n", speed_str, elapsed_str);
}

/**
 * @brief Receive a single file within a directory
 *
 * @param s Socket descriptor
 * @param base_dir Base directory path
 * @return 0 on success, -1 on error
 */
int receive_single_file_in_dir(SOCKET_T s, const char *base_dir) {
    FileHeader header;

    // Receive file header
    if (recv_all(s, &header, HEADER_SIZE) != 0) {
        return -1;
    }

    // Convert header from network byte order
    uint64_t file_size = ntohll(header.file_size);
    uint64_t filename_len = ntohll(header.filename_len);

    // Check for end marker
    if (file_size == 0 && filename_len == 0) {
        return 1;  // End of directory transfer
    }

    // Receive relative path
    char *relative_path = malloc(filename_len + 1);
    if (!relative_path) {
        perror("malloc");
        return -1;
    }

    if (recv_all(s, relative_path, filename_len) != 0) {
        free(relative_path);
        return -1;
    }
    relative_path[filename_len] = '\0';

    // Construct full file path
    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, relative_path);

    // Create directory structure if needed
    char *dir_path = strdup(full_path);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (create_directory_recursive(dir_path) != 0) {
            free(relative_path);
            free(dir_path);
            return -1;
        }
    }
    free(dir_path);

    printf("Receiving: %s\n", relative_path);

    // Initialize adaptive chunk sizing
    AdaptiveState adaptive;
    adaptive_init(&adaptive, file_size);

    // Create and write file
    FILE *file = fopen(full_path, "wb");
    if (!file) {
        perror("fopen");
        free(relative_path);
        return -1;
    }

    // Receive file content
    char *buffer = malloc(MAX_CHUNK_BUFFER_SIZE);
    if (!buffer) {
        perror("malloc");
        fclose(file);
        free(relative_path);
        return -1;
    }

    uint64_t total_received = 0;
    size_t chunk_size = adaptive_get_chunk_size(&adaptive);
    time_t chunk_start = time(NULL);

    while (total_received < file_size) {
        size_t to_receive = file_size - total_received;
        if (to_receive > chunk_size) {
            to_receive = chunk_size;
        }

        if (recv_all(s, buffer, to_receive) != 0) {
            free(buffer);
            fclose(file);
            free(relative_path);
            return -1;
        }

        if (fwrite(buffer, 1, to_receive, file) != to_receive) {
            perror("fwrite");
            free(buffer);
            fclose(file);
            free(relative_path);
            return -1;
        }

        time_t chunk_end = time(NULL);
        double chunk_elapsed = difftime(chunk_end, chunk_start);
        chunk_start = chunk_end;

        adaptive_update(&adaptive, to_receive, chunk_elapsed);
        chunk_size = adaptive_get_chunk_size(&adaptive);

        total_received += to_receive;

        // Check for shutdown signal
        int shutdown = signals_should_shutdown();
        if (shutdown == 1) {
            printf("\nShutdown requested. Press Ctrl+C again to force exit...\n");
            signals_acknowledge_shutdown();
        } else if (shutdown == 2) {
            printf("\nForced exit!\n");
            free(buffer);
            fclose(file);
            free(relative_path);
            exit(EXIT_FAILURE);
        }
    }

    free(buffer);
    fclose(file);
    free(relative_path);
    return 0;
}

/**
 * @brief Receive a directory using the defined protocol
 *
 * @param s Socket descriptor
 * @return 0 on success, -1 on error
 */
int recv_directory_protocol(SOCKET_T s) {
    DirectoryHeader dir_header;

    // Receive directory header
    if (recv_all(s, &dir_header, DIR_HEADER_SIZE) != 0) {
        return -1;
    }

    // Convert header from network byte order
    uint64_t total_files = ntohll(dir_header.total_files);
    uint64_t total_size = ntohll(dir_header.total_size);
    uint64_t base_name_len = ntohll(dir_header.base_path_len);

    // Receive base directory name
    char *base_name = malloc(base_name_len + 1);
    if (!base_name) {
        perror("malloc");
        return -1;
    }

    if (recv_all(s, base_name, base_name_len) != 0) {
        free(base_name);
        return -1;
    }
    base_name[base_name_len] = '\0';

    printf("Receiving directory: %s (%llu files, ", base_name, (unsigned long long)total_files);

    char size_str[32];
    format_bytes(total_size, size_str, sizeof(size_str));
    printf("%s total)\n", size_str);

    // Create base directory
    if (create_directory_recursive(base_name) != 0) {
        free(base_name);
        return -1;
    }

    // Receive all files
    time_t start_time = time(NULL);
    uint64_t files_received = 0;

    while (1) {
        int result = receive_single_file_in_dir(s, base_name);
        if (result == 1) {
            break;  // End of transfer
        } else if (result != 0) {
            free(base_name);
            return -1;  // Error
        }

        files_received++;
    }

    // Display final statistics
    time_t end_time = time(NULL);
    double elapsed_seconds = difftime(end_time, start_time);
    double speed = elapsed_seconds > 0 ? (double)total_size / elapsed_seconds : 0;

    char speed_str[32], elapsed_str[32];
    format_speed(speed, speed_str, sizeof(speed_str));
    format_time((int)elapsed_seconds, elapsed_str, sizeof(elapsed_str));

    free(base_name);

    printf("\nDirectory received successfully!\n");
    printf("Total: %llu files received\n", (unsigned long long)files_received);
    printf("Average speed: %s | Total time: %s\n", speed_str, elapsed_str);

    return 0;
}

/**
 * @brief Detect transfer type by reading magic number
 *
 * @param s Socket descriptor
 * @return 0 for file transfer, 1 for directory transfer, -1 on error
 */
int detect_transfer_type(SOCKET_T s) {
    uint32_t magic;

    // Read magic number
    if (recv_all(s, &magic, MAGIC_SIZE) != 0) {
        return -1;
    }

    // Check magic number and return corresponding type
    uint32_t magic_host = ntohl(magic);
    if (magic_host == FILE_MAGIC) {
        return 0;  // File transfer
    } else if (magic_host == DIR_MAGIC) {
        return 1;  // Directory transfer
    } else if (magic_host == TARGET_FILE_MAGIC) {
        return 2;  // File transfer with target directory
    } else if (magic_host == TARGET_DIR_MAGIC) {
        return 3;  // Directory transfer with target directory
    } else {
        fprintf(stderr, "Error: Unknown transfer type magic number: 0x%08X\n", magic_host);
        return -1;
    }
}

/**
 * @brief Check if a path is a directory
 *
 * @param path Path to check
 * @return 1 if it's a directory, 0 if it's a file, -1 on error
 */
int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;  // Error getting file status
    }
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

/**
 * @brief Recursively count files and calculate total size in a directory
 *
 * @param dirpath Path to the directory
 * @param total_files Pointer to store total file count
 * @param total_size Pointer to store total size
 * @return 0 on success, -1 on error
 */
int count_directory_files(const char *dirpath, uint64_t *total_files, uint64_t *total_size) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[4096];

    *total_files = 0;
    *total_size = 0;

    dir = opendir(dirpath);
    if (!dir) {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct full path
        snprintf(full_path, sizeof(full_path), "%s/%s", dirpath, entry->d_name);

        // Get file/directory status
        if (stat(full_path, &st) != 0) {
            closedir(dir);
            perror("stat");
            return -1;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recursively process subdirectory
            uint64_t sub_files, sub_size;
            if (count_directory_files(full_path, &sub_files, &sub_size) != 0) {
                closedir(dir);
                return -1;
            }
            *total_files += sub_files;
            *total_size += sub_size;
        } else if (S_ISREG(st.st_mode)) {
            // Regular file
            (*total_files)++;
            *total_size += st.st_size;
        }
    }

    closedir(dir);
    return 0;
}

/**
 * @brief Create directory recursively (like mkdir -p)
 *
 * @param dirpath Directory path to create
 * @return 0 on success, -1 on error
 */
int create_directory_recursive(const char *dirpath) {
    char path[4096];
    char *p;

    // Make a copy of the path to modify
    strncpy(path, dirpath, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    // Skip leading slash
    p = path;
    if (*p == '/') p++;

    while (*p) {
        // Find next slash
        while (*p && *p != '/') p++;

        if (*p) {
            *p = '\0';  // Temporarily terminate string

            // Create directory if it doesn't exist
            if (mkdir(path, 0755) != 0 && errno != EEXIST) {
                perror("mkdir");
                return -1;
            }

            *p = '/';  // Restore slash
            p++;       // Move past slash
        }
    }

    // Create final directory
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }

    return 0;
}

/**
 * @brief Sanitize and validate target directory path
 *
 * Ensures the target directory path is safe and doesn't contain malicious
 * path traversal attempts.
 */
int validate_target_directory(const char *target_dir, char *sanitized_dir, size_t buffer_size) {
    if (!target_dir || !sanitized_dir || buffer_size == 0) {
        return -1;
    }

    // Initialize output buffer
    sanitized_dir[0] = '\0';

    // Check for empty target directory (current directory)
    if (strlen(target_dir) == 0) {
        return 0;
    }

    // Check for path traversal attempts
    if (strstr(target_dir, "..") != NULL) {
        fprintf(stderr, "Error: Path traversal detected in target directory\n");
        return -1;
    }

    // Check for absolute paths that could be dangerous
    if (target_dir[0] == '/') {
        fprintf(stderr, "Error: Absolute paths not allowed in target directory\n");
        return -1;
    }

    // Sanitize path: remove leading slashes, limit length
    const char *clean_path = target_dir;
    while (*clean_path == '/') clean_path++;

    // Check for very long paths
    if (strlen(clean_path) > buffer_size - 2) {
        fprintf(stderr, "Error: Target directory path too long\n");
        return -1;
    }

    // Copy sanitized path
    strncpy(sanitized_dir, clean_path, buffer_size - 1);
    sanitized_dir[strlen(sanitized_dir)] = '\0';

    return 0;
}

/**
 * @brief Send a file with target directory support
 */
void send_file_with_target_protocol(SOCKET_T s, const char *filepath, const char *target_dir) {

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Get file size
    struct stat st;
    if (stat(filepath, &st) != 0) {
        perror("stat");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    uint64_t file_size = st.st_size;

    // Extract filename
    const char *filename = strrchr(filepath, '/');
#ifdef _WIN32
    const char *filename_win = strrchr(filepath, '\\');
    if (filename_win > filename) filename = filename_win;
#endif
    if (!filename) {
        filename = filepath;
    } else {
        filename++;
    }

    // Validate and sanitize target directory
    char sanitized_target[4096] = {0};
    if (validate_target_directory(target_dir, sanitized_target, sizeof(sanitized_target)) != 0) {
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Initialize adaptive chunk sizing
    AdaptiveState adaptive;
    adaptive_init(&adaptive, file_size);

    // Prepare enhanced header
    uint64_t filename_len = strlen(filename);
    uint64_t target_dir_len = strlen(sanitized_target);
    TargetFileHeader header;
    header.file_size = htonll(file_size);
    header.filename_len = htonll(filename_len);
    header.target_dir_len = htonll(target_dir_len);

    // Send magic number
    uint32_t magic = htonl(TARGET_FILE_MAGIC);
    if (send_all(s, &magic, sizeof(magic)) != 0) {
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Send header
    if (send_all(s, &header, sizeof(header)) != 0) {
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Send filename
    if (send_all(s, filename, filename_len) != 0) {
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Send target directory (if specified)
    if (target_dir_len > 0) {
        if (send_all(s, sanitized_target, target_dir_len) != 0) {
            fclose(file);
            exit(EXIT_FAILURE);
        }
    }

    // Send file content in chunks
    char *buffer = malloc(MAX_CHUNK_BUFFER_SIZE);
    if (!buffer) {
        perror("malloc");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    size_t bytes_read;
    uint64_t total_sent = 0;
    time_t start_time = time(NULL);
    size_t chunk_size = adaptive_get_chunk_size(&adaptive);
    time_t chunk_start = start_time;

    printf("Sending file: %s", filename);
    if (target_dir_len > 0) {
        printf(" -> %s/", sanitized_target);
    }
    printf(" (%s)\n", file_size > 1024*1024 ? "large file" : "small file");

    while ((bytes_read = fread(buffer, 1, chunk_size, file)) > 0) {
        time_t chunk_end = time(NULL);
        double chunk_elapsed = difftime(chunk_end, chunk_start);
        chunk_start = chunk_end;

        if (send_all(s, buffer, bytes_read) != 0) {
            free(buffer);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        adaptive_update(&adaptive, bytes_read, chunk_elapsed);
        chunk_size = adaptive_get_chunk_size(&adaptive);
        total_sent += bytes_read;

        // Check for shutdown signal
        int shutdown = signals_should_shutdown();
        if (shutdown == 1) {
            printf("\nShutdown requested. Press Ctrl+C again to force exit...\n");
            signals_acknowledge_shutdown();
        } else if (shutdown == 2) {
            printf("\nForced exit!\n");
            free(buffer);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        // Progress display
        if (file_size > 1024*1024) { // Only show progress for large files
            double percent = (double)total_sent / file_size * 100.0;
            time_t elapsed = time(NULL) - start_time;
            double speed = elapsed > 0 ? (double)total_sent / (1024.0 * 1024.0) / elapsed : 0;

            printf("\rProgress: %.1f%%", percent);
            if (speed > 0) {
                printf(" (%.1f MB/s)", speed);
            }
            fflush(stdout);
        }
    }

    if (ferror(file)) {
        perror("fread");
        free(buffer);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    free(buffer);
    fclose(file);
    printf("\nFile sent successfully!\n");
}

/**
 * @brief Send a directory with target directory support
 */
void send_directory_with_target_protocol(SOCKET_T s, const char *dirpath, const char *target_dir) {
    // Check if path is a directory
    struct stat st;
    if (stat(dirpath, &st) != 0) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s is not a directory\n", dirpath);
        exit(EXIT_FAILURE);
    }

    // Validate and sanitize target directory
    char sanitized_target[4096] = {0};
    if (validate_target_directory(target_dir, sanitized_target, sizeof(sanitized_target)) != 0) {
        exit(EXIT_FAILURE);
    }

    // Count files and calculate total size
    uint64_t total_files = 0, total_size = 0;
    if (count_directory_files(dirpath, &total_files, &total_size) != 0) {
        fprintf(stderr, "Error: Failed to analyze directory\n");
        exit(EXIT_FAILURE);
    }

    // Extract directory name
    const char *dir_name = strrchr(dirpath, '/');
#ifdef _WIN32
    const char *dir_name_win = strrchr(dirpath, '\\');
    if (dir_name_win > dir_name) dir_name = dir_name_win;
#endif
    if (!dir_name) {
        dir_name = dirpath;
    } else {
        dir_name++;
    }

    // Prepare enhanced directory header
    uint64_t base_path_len = strlen(dir_name);
    uint64_t target_dir_len = strlen(sanitized_target);
    TargetDirectoryHeader header;
    header.total_files = htonll(total_files);
    header.total_size = htonll(total_size);
    header.base_path_len = htonll(base_path_len);
    header.target_dir_len = htonll(target_dir_len);

    // Send magic number
    uint32_t magic = htonl(TARGET_DIR_MAGIC);
    if (send_all(s, &magic, sizeof(magic)) != 0) {
        exit(EXIT_FAILURE);
    }

    // Send header
    if (send_all(s, &header, sizeof(header)) != 0) {
        exit(EXIT_FAILURE);
    }

    // Send base directory name
    if (send_all(s, dir_name, base_path_len) != 0) {
        exit(EXIT_FAILURE);
    }

    // Send target directory (if specified)
    if (target_dir_len > 0) {
        if (send_all(s, sanitized_target, target_dir_len) != 0) {
            exit(EXIT_FAILURE);
        }
    }

    printf("Sending directory: %s", dir_name);
    if (target_dir_len > 0) {
        printf(" -> %s/", sanitized_target);
    }
    printf(" (%lu files, %s)\n", (unsigned long)total_files, total_size > 1024*1024 ? "large" : "small");

    // Send all files recursively
    send_directory_recursive(s, dirpath, "");

    printf("Directory sent successfully!\n");
}

/**
 * @brief Receive a file with target directory support
 */
int recv_file_with_target_protocol(SOCKET_T s) {
    // Receive enhanced header
    TargetFileHeader header;
    if (recv_all(s, &header, sizeof(header)) != 0) {
        return -1;
    }

    // Convert from network byte order
    uint64_t file_size = ntohll(header.file_size);
    uint64_t filename_len = ntohll(header.filename_len);
    uint64_t target_dir_len = ntohll(header.target_dir_len);

    // Receive filename
    char *filename = malloc(filename_len + 1);
    if (!filename) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return -1;
    }

    if (recv_all(s, filename, filename_len) != 0) {
        free(filename);
        return -1;
    }
    filename[filename_len] = '\0';

    // Receive target directory (if specified)
    char *target_dir = NULL;
    if (target_dir_len > 0) {
        target_dir = malloc(target_dir_len + 1);
        if (!target_dir) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            free(filename);
            return -1;
        }

        if (recv_all(s, target_dir, target_dir_len) != 0) {
            free(filename);
            free(target_dir);
            return -1;
        }
        target_dir[target_dir_len] = '\0';
    }

    // Create target directory if specified
    char full_path[4096];
    if (target_dir && strlen(target_dir) > 0) {
        if (create_directory_recursive(target_dir) != 0) {
            free(filename);
            if (target_dir) free(target_dir);
            return -1;
        }
        snprintf(full_path, sizeof(full_path), "%s/%s", target_dir, filename);
    } else {
        strncpy(full_path, filename, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }

    printf("Receiving file: %s", filename);
    if (target_dir && strlen(target_dir) > 0) {
        printf(" -> %s/", target_dir);
    }
    printf(" (%s)\n", file_size > 1024*1024 ? "large file" : "small file");

    // Initialize adaptive chunk sizing
    AdaptiveState adaptive;
    adaptive_init(&adaptive, file_size);

    // Create and write file
    FILE *file = fopen(full_path, "wb");
    if (!file) {
        perror("fopen");
        free(filename);
        if (target_dir) free(target_dir);
        return -1;
    }

    // Receive file content
    char *buffer = malloc(MAX_CHUNK_BUFFER_SIZE);
    if (!buffer) {
        perror("malloc");
        fclose(file);
        free(filename);
        if (target_dir) free(target_dir);
        return -1;
    }

    uint64_t total_received = 0;
    time_t start_time = time(NULL);
    size_t chunk_size = adaptive_get_chunk_size(&adaptive);
    time_t chunk_start = start_time;

    while (total_received < file_size) {
        size_t to_receive = file_size - total_received;
        if (to_receive > chunk_size) {
            to_receive = chunk_size;
        }

        ssize_t received = recv(s, buffer, to_receive, 0);
        if (received <= 0) {
            fprintf(stderr, "Error: Connection closed while receiving file\n");
            free(buffer);
            fclose(file);
            free(filename);
            if (target_dir) free(target_dir);
            return -1;
        }

        time_t chunk_end = time(NULL);
        double chunk_elapsed = difftime(chunk_end, chunk_start);
        chunk_start = chunk_end;

        if (fwrite(buffer, 1, received, file) != received) {
            perror("fwrite");
            free(buffer);
            fclose(file);
            free(filename);
            if (target_dir) free(target_dir);
            return -1;
        }

        adaptive_update(&adaptive, received, chunk_elapsed);
        chunk_size = adaptive_get_chunk_size(&adaptive);
        total_received += received;

        // Check for shutdown signal
        int shutdown = signals_should_shutdown();
        if (shutdown == 1) {
            printf("\nShutdown requested. Press Ctrl+C again to force exit...\n");
            signals_acknowledge_shutdown();
        } else if (shutdown == 2) {
            printf("\nForced exit!\n");
            free(buffer);
            fclose(file);
            free(filename);
            if (target_dir) free(target_dir);
            exit(EXIT_FAILURE);
        }

        // Progress display for large files
        if (file_size > 1024*1024) {
            double percent = (double)total_received / file_size * 100.0;
            time_t elapsed = time(NULL) - start_time;
            double speed = elapsed > 0 ? (double)total_received / (1024.0 * 1024.0) / elapsed : 0;

            printf("\rProgress: %.1f%%", percent);
            if (speed > 0) {
                printf(" (%.1f MB/s)", speed);
            }
            fflush(stdout);
        }
    }

    free(buffer);
    fclose(file);
    printf("\nFile received successfully: %s\n", full_path);

    free(filename);
    if (target_dir) free(target_dir);
    return 0;
}

/**
 * @brief Receive a directory with target directory support
 */
int recv_directory_with_target_protocol(SOCKET_T s) {
    // Receive enhanced directory header
    TargetDirectoryHeader header;
    if (recv_all(s, &header, sizeof(header)) != 0) {
        return -1;
    }

    // Convert from network byte order
    uint64_t total_files = ntohll(header.total_files);
    uint64_t total_size = ntohll(header.total_size);
    uint64_t base_path_len = ntohll(header.base_path_len);
    uint64_t target_dir_len = ntohll(header.target_dir_len);

    // Receive base directory name
    char *base_dir = malloc(base_path_len + 1);
    if (!base_dir) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return -1;
    }

    if (recv_all(s, base_dir, base_path_len) != 0) {
        free(base_dir);
        return -1;
    }
    base_dir[base_path_len] = '\0';

    // Receive target directory (if specified)
    char *target_dir = NULL;
    if (target_dir_len > 0) {
        target_dir = malloc(target_dir_len + 1);
        if (!target_dir) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            free(base_dir);
            return -1;
        }

        if (recv_all(s, target_dir, target_dir_len) != 0) {
            free(base_dir);
            free(target_dir);
            return -1;
        }
        target_dir[target_dir_len] = '\0';
    }

    // Create full target path
    char full_target_path[4096];
    if (target_dir && strlen(target_dir) > 0) {
        if (create_directory_recursive(target_dir) != 0) {
            free(base_dir);
            if (target_dir) free(target_dir);
            return -1;
        }
        snprintf(full_target_path, sizeof(full_target_path), "%s/%s", target_dir, base_dir);
    } else {
        strncpy(full_target_path, base_dir, sizeof(full_target_path) - 1);
        full_target_path[sizeof(full_target_path) - 1] = '\0';
    }

    printf("Receiving directory: %s", base_dir);
    if (target_dir && strlen(target_dir) > 0) {
        printf(" -> %s/", target_dir);
    }
    printf(" (%lu files)\n", (unsigned long)total_files);

    // Create target directory structure
    if (create_directory_recursive(full_target_path) != 0) {
        free(base_dir);
        if (target_dir) free(target_dir);
        return -1;
    }

    // Receive all files
    uint64_t files_received = 0;
    while (files_received < total_files) {
        int result = receive_single_file_in_dir(s, full_target_path);
        if (result != 0) {
            free(base_dir);
            if (target_dir) free(target_dir);
            return -1;
        }
        files_received++;
    }

    printf("Directory received successfully: %s\n", full_target_path);

    free(base_dir);
    if (target_dir) free(target_dir);
    return 0;
}
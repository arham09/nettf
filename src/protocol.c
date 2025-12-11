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
#include <dirent.h> // For directory operations
#include <string.h> // For string manipulation functions

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
    char buffer[CHUNK_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        if (send_all(s, buffer, bytes_read) != 0) {
            fclose(file);
            exit(EXIT_FAILURE);
        }
    }

    if (ferror(file)) {
        perror("fread");
        fclose(file);
        exit(EXIT_FAILURE);
    }

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

    // Create and write file
    FILE *file = fopen(full_path, "wb");
    if (!file) {
        perror("fopen");
        free(relative_path);
        return -1;
    }

    // Receive file content
    char buffer[CHUNK_SIZE];
    uint64_t total_received = 0;

    while (total_received < file_size) {
        size_t to_receive = file_size - total_received;
        if (to_receive > CHUNK_SIZE) {
            to_receive = CHUNK_SIZE;
        }

        if (recv_all(s, buffer, to_receive) != 0) {
            fclose(file);
            free(relative_path);
            return -1;
        }

        if (fwrite(buffer, 1, to_receive, file) != to_receive) {
            perror("fwrite");
            fclose(file);
            free(relative_path);
            return -1;
        }

        total_received += to_receive;
    }

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
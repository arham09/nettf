# NETTF File Transfer Tool - Code Documentation

## Table of Contents
- [Platform Abstraction Layer](#platform-abstraction-layer)
- [Enhanced Protocol Layer](#enhanced-protocol-layer)
- [🆕 Target Directory Feature](#-target-directory-feature)
- [🆕 Logging System](#-logging-system)
- [🆕 Signal Handling](#signal-handling)
- [🆕 Adaptive Chunk Sizing](#adaptive-chunk-sizing)
- [Network Discovery System](#network-discovery-system)
- [Client Module](#client-module)
- [Server Module](#server-module)
- [Main Module](#main-module)
- [Build System](#build-system)
- [Testing System](#testing-system)
- [New Features in Latest Version](#new-features-in-latest-version)

---

## Platform Abstraction Layer

### `src/platform.h`

```c
#ifndef PLATFORM_H
#define PLATFORM_H
```
- **Purpose**: Header guards to prevent multiple inclusion
- **Why**: Avoids redefinition errors when this header is included multiple times

```c
#ifdef _WIN32
    #define IS_WINDOWS 1
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define SOCKET_T SOCKET
    #define INVALID_SOCKET_T INVALID_SOCKET
    typedef struct sockaddr_in SOCKADDR_IN_T;
```
- **Purpose**: Windows-specific socket definitions
- **`#ifdef _WIN32`**: Conditional compilation for Windows
- **`winsock2.h`**: Windows socket library
- **`SOCKET_T`**: Type alias to normalize socket types across platforms

```c
#else
    #define IS_WINDOWS 0
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int SOCKET_T;
    #define INVALID_SOCKET_T -1
    #define SOCKET_ERROR -1
    typedef struct sockaddr_in SOCKADDR_IN_T;
```
- **Purpose**: POSIX (Linux/macOS) socket definitions
- **`sys/socket.h`**: Core socket functions
- **`netinet/in.h`**: Internet address structures
- **`arpa/inet.h`**: IP address manipulation functions
- **`typedef int SOCKET_T`**: Sockets are file descriptors on POSIX systems

```c
#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
#endif
```
- **Purpose**: Auto-link Winsock library on Windows
- **`#pragma comment`**: MSVC directive to link libraries

### `src/platform.c`

```c
uint64_t htonll(uint64_t value) {
    if (htonl(1) == 1) {
        return value; // Big endian system
    } else {
        return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32) | htonl((value >> 32) & 0xFFFFFFFF);
    }
}
```
- **Purpose**: Convert 64-bit value to network byte order
- **`htonl(1) == 1`**: Check if system is already big-endian
  - If `htonl(1)` returns 1, system is big-endian (no conversion needed)
  - If `htonl(1)` returns something else (like 0x01000000 on little-endian), conversion is needed
- **`value & 0xFFFFFFFF`**: Extract lower 32 bits using bitwise AND
- **`value >> 32`**: Extract upper 32 bits using right shift
- **`htonl(value & 0xFFFFFFFF)`**: Convert lower 32 bits to network order
- **`htonl((value >> 32) & 0xFFFFFFFF)`**: Convert upper 32 bits to network order
- **`<< 32`**: Shift converted upper bits to correct position
- **`|`**: Combine both parts using bitwise OR

**Example**: Converting 0x0123456789ABCDEF on little-endian system:
1. Lower 32 bits: 0x89ABCDEF → htonl() → 0xEFCDAB89
2. Upper 32 bits: 0x01234567 → htonl() → 0x67452301
3. Shift upper bits: 0x67452301 << 32 → 0x6745230100000000
4. Combine: 0x6745230100000000 | 0xEFCDAB89 → 0x67452301EFCDAB89 (network byte order)

```c
void net_init(void) {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
```
- **Purpose**: Initialize network subsystem
- **Windows**: `WSAStartup()` initializes Winsock
- **`MAKEWORD(2, 2)`**: Request Winsock version 2.2

```c
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        fprintf(stderr, "Could not find a usable version of Winsock.dll\n");
        WSACleanup();
        exit(EXIT_FAILURE);
    }
```
- **Purpose**: Verify Winsock version
- **`LOBYTE/HIBYTE`**: Extract version numbers

---

## Enhanced Protocol Layer

### `src/protocol.h`

```c
#define HEADER_SIZE 16    // Total header size: 8 bytes (file_size) + 8 bytes (filename_len)
#define DIR_HEADER_SIZE 24 // Directory header: 8 bytes (total_files) + 8 bytes (total_size) + 8 bytes (base_path_len)
#define CHUNK_SIZE 65536  // Size of file chunks for transfer (64KB for high-speed transfers)
#define MAGIC_SIZE 4      // Size of magic number (4 bytes)

// Magic numbers to distinguish transfer types
#define FILE_MAGIC 0x46494C45  // "FILE" in hex
#define DIR_MAGIC  0x44495220  // "DIR " in hex
```
- **Purpose**: Enhanced protocol constants supporting files and directories
- **`HEADER_SIZE`**: Increased to 16 bytes for 64-bit filename length support
- **`DIR_HEADER_SIZE`**: 24 bytes for directory metadata
- **`CHUNK_SIZE`**: Optimized 64KB chunks for maximum transfer speed
- **Magic Numbers**: Enable automatic detection of transfer type (file vs directory)

```c
typedef struct {
    uint64_t file_size;    // Size of file in bytes (8 bytes, up to 16 exabytes)
    uint64_t filename_len; // Length of filename in bytes (8 bytes, up to 16 exabytes)
} FileHeader;

typedef struct {
    uint64_t total_files;   // Total number of files in the directory (8 bytes)
    uint64_t total_size;    // Total size of all files in bytes (8 bytes)
    uint64_t base_path_len; // Length of base directory path (8 bytes)
} DirectoryHeader;
```
- **Purpose**: Enhanced protocol header structures
- **`uint64_t`**: Both file size and filename length use 64-bit for maximum compatibility
- **DirectoryHeader**: Supports recursive directory transfers with metadata

### `src/protocol.c`

```c
int send_all(SOCKET_T s, const void *data, size_t len) {
    const char *ptr = (const char *)data;
    size_t total_sent = 0;

    while (total_sent < len) {
        ssize_t sent = send(s, ptr + total_sent, len - total_sent, 0);
```
- **Purpose**: Send all bytes, handling partial sends
- **`ptr + total_sent`**: Move pointer forward as bytes are sent
- **`len - total_sent`**: Remaining bytes to send
- **TCP may send partial data, so we loop until complete**

```c
        if (sent == SOCKET_ERROR) {
            perror("send");
            return -1;
        }
        if (sent == 0) {
            fprintf(stderr, "Connection closed by peer\n");
            return -1;
        }
        total_sent += sent;
    }
```
- **Purpose**: Error handling and progress tracking
- **`SOCKET_ERROR`**: Network error occurred
- **`sent == 0`**: Connection closed by remote peer

```c
void send_file_protocol(SOCKET_T s, const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
```
- **Purpose**: Open file in binary mode
- **`"rb"`**: Read binary mode (critical for file integrity)
- **`fopen()`** failure check prevents crashes

```c
    struct stat st;
    if (stat(filepath, &st) != 0) {
        perror("stat");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    uint64_t file_size = st.st_size;
```
- **Purpose**: Get file size
- **`stat()`**: System call to get file information
- **`st.st_size`**: File size in bytes

```c
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
```
- **Purpose**: Extract filename without directory path
- **`strrchr()`**: Find last occurrence of path separator
- **Security**: Prevents path traversal attacks
- **`filename++`**: Move past the separator

```c
    FileHeader header;
    header.file_size = htonll(file_size);
    header.filename_len = htonl(filename_len);
```
- **Purpose**: Prepare protocol header
- **`htonll/htonl`**: Convert to network byte order
- **Ensures compatibility between different architectures**

```c
    char buffer[CHUNK_SIZE];
    size_t bytes_read;
    uint64_t total_sent = 0;

    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        if (send_all(s, buffer, bytes_read) != 0) {
            fclose(file);
            exit(EXIT_FAILURE);
        }
        total_sent += bytes_read;
        printf("\rProgress: %.2f%%", (double)total_sent / file_size * 100);
        fflush(stdout);
    }
```
- **Purpose**: Send file in chunks with progress tracking
- **`fread()`**: Read up to CHUNK_SIZE bytes
- **`\r`**: Carriage return to overwrite progress line
- **`fflush(stdout)`**: Force immediate output

---

## 🆕 Target Directory Feature

### Overview

The target directory feature allows senders to specify exactly where files and directories should be saved on the receiver's system. This feature is implemented through enhanced protocol extensions while maintaining full backward compatibility with existing installations.

### Protocol Extensions

#### Protocol Overview

The NETTF protocol uses a binary format with magic numbers to distinguish between four transfer types. All multi-byte integer fields use network byte order (big-endian) for cross-platform compatibility.

**Protocol Flow:**
1. **Connection Establishment**: TCP three-way handshake
2. **Magic Number Transmission**: 4 bytes to identify transfer type
3. **Header Transmission**: Protocol-specific metadata (16-40 bytes)
4. **Payload Transmission**: Filename(s) and target directory (if applicable)
5. **Content Transfer**: File data in 64KB chunks
6. **Connection Termination**: TCP teardown

**All Protocol Variants:**
- Standard File: Magic (4) + Header (16) + Filename + Content
- Standard Directory: Magic (4) + Header (24) + Dirname + File Sequence
- Target File: Magic (4) + Header (32) + Filename + TargetDir + Content
- Target Directory: Magic (4) + Header (40) + Dirname + TargetDir + File Sequence

#### New Magic Numbers

```c
#define TARGET_FILE_MAGIC 0x54415247  // "TARG" in hex - File with target directory
#define TARGET_DIR_MAGIC  0x54444952  // "TDIR" in hex - Directory with target directory
```

#### Enhanced Header Structures

```c
/**
 * @brief Standard file header structure (16 bytes total)
 */
typedef struct {
    uint64_t file_size;      // Size of file in bytes (8 bytes)
    uint64_t filename_len;   // Length of filename in bytes (8 bytes)
} FileHeader;

/**
 * @brief Standard directory header structure (24 bytes total)
 */
typedef struct {
    uint64_t total_files;     // Total number of files in the directory (8 bytes)
    uint64_t total_size;      // Total size of all files in bytes (8 bytes)
    uint64_t base_path_len;   // Length of base directory path (8 bytes)
} DirectoryHeader;

/**
 * @brief Enhanced file header structure with target directory support (32 bytes total)
 */
typedef struct {
    uint64_t file_size;      // Size of file in bytes (8 bytes)
    uint64_t filename_len;   // Length of filename in bytes (8 bytes)
    uint64_t target_dir_len; // Length of target directory path in bytes (8 bytes, 0 for current directory)
} TargetFileHeader;

/**
 * @brief Enhanced directory header structure with target directory support (40 bytes total)
 */
typedef struct {
    uint64_t total_files;     // Total number of files in the directory (8 bytes)
    uint64_t total_size;      // Total size of all files in bytes (8 bytes)
    uint64_t base_path_len;   // Length of base directory path (8 bytes)
    uint64_t target_dir_len;  // Length of target directory path in bytes (8 bytes, 0 for current directory)
} TargetDirectoryHeader;
```

### Target Directory Validation

#### Security Validation Function

```c
/**
 * @brief Sanitize and validate target directory path
 */
int validate_target_directory(const char *target_dir, char *sanitized_dir, size_t buffer_size);
```

**Security Checks Implemented:**

1. **Path Traversal Prevention**: Blocks `..` sequences
   ```c
   if (strstr(target_dir, "..") != NULL) {
       fprintf(stderr, "Error: Path traversal detected in target directory\n");
       return -1;
   }
   ```

2. **Absolute Path Prevention**: Blocks paths starting with `/`
   ```c
   if (target_dir[0] == '/') {
       fprintf(stderr, "Error: Absolute paths not allowed in target directory\n");
       return -1;
   }
   ```

3. **Length Validation**: Prevents buffer overflows
   ```c
   if (strlen(clean_path) > buffer_size - 2) {
       fprintf(stderr, "Error: Target directory path too long\n");
       return -1;
   }
   ```

4. **Path Sanitization**: Removes leading slashes and dangerous characters
   ```c
   const char *clean_path = target_dir;
   while (*clean_path == '/') clean_path++;
   ```

### Enhanced Protocol Functions

#### Target File Sending

```c
/**
 * @brief Send a file with target directory support
 */
void send_file_with_target_protocol(SOCKET_T s, const char *filepath, const char *target_dir);
```

**Implementation Flow:**

1. **File Validation**: Open and validate the source file
2. **Target Validation**: Sanitize and validate the target directory path
3. **Header Preparation**: Create `TargetFileHeader` with metadata
4. **Magic Number**: Send `TARGET_FILE_MAGIC` for protocol identification
5. **Metadata Transmission**: Send header, filename, and target directory
6. **File Content**: Transfer file content in 64KB chunks

**Key Security Features:**
- **Path Stripping**: Extract only filename (no directory components)
- **Target Sanitization**: Comprehensive validation before transmission
- **Memory Management**: Proper cleanup on errors

#### Target Directory Sending

```c
/**
 * @brief Send a directory with target directory support
 */
void send_directory_with_target_protocol(SOCKET_T s, const char *dirpath, const char *target_dir);
```

**Implementation Flow:**

1. **Directory Validation**: Verify path is a valid directory
2. **Content Analysis**: Count files and calculate total size
3. **Header Preparation**: Create `TargetDirectoryHeader` with metadata
4. **Magic Number**: Send `TARGET_DIR_MAGIC` for protocol identification
5. **Directory Metadata**: Send header, directory name, and target directory
6. **Recursive Transfer**: Send all files using existing recursive mechanisms

#### Target File Receiving

```c
/**
 * @brief Receive a file with target directory support
 */
int recv_file_with_target_protocol(SOCKET_T s);
```

**Implementation Flow:**

1. **Header Reception**: Receive and parse `TargetFileHeader`
2. **Metadata Extraction**: Extract filename and target directory path
3. **Directory Creation**: Create target directory if specified and doesn't exist
4. **File Creation**: Create file in the correct target location
5. **Content Reception**: Receive file content with progress tracking

**Key Features:**
- **Automatic Directory Creation**: Creates parent directories as needed
- **Path Construction**: Safely constructs full file paths
- **Memory Management**: Proper allocation and cleanup
- **Progress Display**: Enhanced progress with target directory information

#### Target Directory Receiving

```c
/**
 * @brief Receive a directory with target directory support
 */
int recv_directory_with_target_protocol(SOCKET_T s);
```

**Implementation Flow:**

1. **Header Reception**: Receive and parse `TargetDirectoryHeader`
2. **Metadata Extraction**: Extract directory name and target path
3. **Path Construction**: Build full target directory structure
4. **Directory Creation**: Create complete directory tree
5. **File Reception**: Receive all files using existing mechanisms

### Enhanced Transfer Type Detection

```c
/**
 * @brief Detect transfer type by examining first bytes
 */
int detect_transfer_type(SOCKET_T s);
```

**Enhanced Return Values:**
- `0`: Standard file transfer (`FILE_MAGIC`)
- `1`: Standard directory transfer (`DIR_MAGIC`)
- `2`: File transfer with target directory (`TARGET_FILE_MAGIC`)
- `3`: Directory transfer with target directory (`TARGET_DIR_MAGIC`)
- `-1`: Error or unknown protocol

### Client Module Integration

#### Updated Send Function Signature

```c
void send_file(const char *target_ip, int port, const char *filepath, const char *target_dir);
```

**Protocol Selection Logic:**
```c
if (target_dir && strlen(target_dir) > 0) {
    printf("Target directory: %s\n", target_dir);
    if (is_dir) {
        send_directory_with_target_protocol(client_socket, filepath, target_dir);
    } else {
        send_file_with_target_protocol(client_socket, filepath, target_dir);
    }
} else {
    // Use standard protocols for backward compatibility
    if (is_dir) {
        send_directory_protocol(client_socket, filepath);
    } else {
        send_file_protocol(client_socket, filepath);
    }
}
```

### Server Module Integration

#### Enhanced Transfer Type Handling

```c
int transfer_type = detect_transfer_type(client_socket);
switch (transfer_type) {
    case 0:  // Standard file
        recv_file_protocol(client_socket);
        break;
    case 1:  // Standard directory
        recv_directory_protocol(client_socket);
        break;
    case 2:  // Target file
        recv_file_with_target_protocol(client_socket);
        break;
    case 3:  // Target directory
        recv_directory_with_target_protocol(client_socket);
        break;
    default:
        fprintf(stderr, "Error: Unknown transfer type %d\n", transfer_type);
        break;
}
```

### CLI Integration

#### Updated Command Syntax

```bash
./nettf send <TARGET_IP> <FILE_OR_DIR_PATH> [TARGET_DIR]
```

**Argument Parsing:**
```c
const char *target_dir = NULL;  // Optional parameter
if (argc == 5) {
    target_dir = argv[4];
}
```

### Backward Compatibility

The target directory feature maintains 100% backward compatibility:

1. **Protocol Detection**: Magic numbers distinguish between old and new protocols
2. **Fallback Mechanism**: Old receivers gracefully reject unknown protocols
3. **Optional Parameters**: Target directory is completely optional
4. **Standard Behavior**: Existing scripts work without modification

### Security Architecture

#### Defense in Depth

1. **Input Validation**: Comprehensive validation at sender and receiver
2. **Path Sanitization**: Remove dangerous characters and sequences
3. **Access Control**: Operate within receiver's working directory
4. **Memory Safety**: Proper bounds checking and buffer management
5. **Error Handling**: Graceful failure with security-first approach

#### Threat Mitigation

- **Directory Traversal**: Blocked by `..` sequence detection
- **Absolute Path Attacks**: Blocked by leading `/` detection
- **Buffer Overflows**: Prevented by length validation
- **Privilege Escalation**: Restricted to receiver's directory context

### Performance Considerations

#### Optimized for Large Transfers

1. **Chunked Transfer**: 64KB chunks for optimal network utilization
2. **Progress Tracking**: Minimal overhead progress display
3. **Memory Efficiency**: Constant memory footprint regardless of file size
4. **Directory Metadata**: Efficient pre-scanning for large directories

#### Network Efficiency

- **Protocol Overhead**: Additional header bytes (16 bytes) negligible for large files
- **Backward Compatibility**: No additional network traffic for standard transfers
- **Compression Ready**: Protocol structure allows future compression integration

---

## 🆕 Logging System

### `src/logging.h`

The logging system provides comprehensive file-based logging for debugging and monitoring.

```c
typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

// Core functions
int log_init(void);
void log_cleanup(void);
void log_message(LogLevel level, const char *format, ...);

// Convenience macros
#define LOG_DEBUG(fmt, ...) log_message(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_message(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_message(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_message(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
```

**Design Features:**
- **Append-only mode**: Preserves log history across sessions
- **Thread-safe**: Uses file locking for concurrent access
- **Safe no-op**: Logging without `log_init()` is safe (silently fails)
- **Timestamp format**: `[YYYY-MM-DD HH:MM:SS] [LEVEL] message`

### `src/logging.c`

#### Initialization

```c
int log_init(void) {
    if (g_log_file != NULL) {
        return 0;  // Already initialized
    }
    g_log_file = fopen(LOG_FILE_PATH, "a");
    if (g_log_file == NULL) {
        return -1;
    }
    return 0;
}
```

- **Purpose**: Opens log file in append mode
- **Idempotent**: Multiple calls are safe (checks if already open)
- **`"a"` mode**: Creates file if doesn't exist, appends if exists

#### Log Message Formatting

```c
void log_message(LogLevel level, const char *format, ...) {
    if (g_log_file == NULL) {
        return;  // Safe no-op if not initialized
    }

    // Get timestamp
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    // Get level string
    const char *level_str = level_to_string(level);

    // Format: [TIMESTAMP] [LEVEL] message\n
    fprintf(g_log_file, "[%s] [%s] ", timestamp, level_str);
    vfprintf(g_log_file, format, args);
    fprintf(g_log_file, "\n");
    fflush(g_log_file);  // Ensure immediate write
}
```

**Key Implementation Details:**
- **`time()`**: Gets current Unix timestamp
- **`localtime()`**: Converts to local timezone
- **`strftime()`**: Formats timestamp as `YYYY-MM-DD HH:MM:SS`
- **`vfprintf()`**: Prints va_list to file (supports variable arguments)
- **`fflush()`**: Forces immediate write to disk

#### Level Mapping

```c
static const char* level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
```

### Usage Examples

```c
// In main.c
int main(int argc, char *argv[]) {
    log_init();
    LOG_INFO("NETTF started with %d arguments", argc);

    // In client.c
    LOG_INFO("Connecting to %s:%d", ip, port);
    LOG_ERROR("Connection failed: %s", strerror(errno));

    // In server.c
    LOG_INFO("Connection established from %s:%d", client_ip, client_port);
    LOG_WARN("Shutdown requested, waiting for transfer completion");

    // At cleanup
    log_cleanup();
}
```

### Log File Example

```
[2024-12-29 14:32:15] [INFO] NETTF started with 3 arguments
[2024-12-29 14:32:15] [INFO] Starting receive mode on port 9876
[2024-12-29 14:32:20] [INFO] Connection established from 192.168.1.10:54321
[2024-12-29 14:32:20] [INFO] Receiving file: document.pdf (2.4 MB)
[2024-12-29 14:32:20] [DEBUG] Adaptive chunk size: 64 KB
[2024-12-29 14:32:21] [INFO] File received successfully
[2024-12-29 14:32:25] [WARN] Shutdown requested, waiting for transfer completion
[2024-12-29 14:32:30] [INFO] NETTF shutting down
```

---

## 🆕 Signal Handling

### `src/signals.h`

POSIX signal handling for graceful shutdown on Ctrl+C (SIGINT).

**Note**: Windows uses stub implementations only (no Ctrl+C handling).

```c
// Initialize signal handling
int signals_init(void);

// Cleanup signal handling
void signals_cleanup(void);

// Check shutdown status
// Returns: 0 = continue, 1 = prompt user, 2 = force exit
int signals_should_shutdown(void);

// Keep shutdown flag at 1 (after prompt)
void signals_acknowledge_shutdown(void);

// Get last signal name (for debugging)
const char* signals_get_last_signal_name(void);
```

**Design Philosophy:**
- **First Ctrl+C**: Soft shutdown (prompt user, continue transfer)
- **Second Ctrl+C**: Hard shutdown (force exit with cleanup)
- **Atomic counter**: Thread-safe signal count using `stdatomic.h`

### `src/signals.c`

#### Signal Handler Implementation

```c
static volatile sig_atomic_t signal_count = 0;

static void signal_handler(int signum) {
    signal_count++;
}
```

**Key Points:**
- **`volatile sig_atomic_t`**: C standard type for signal handler safety
  - `volatile`: Prevents compiler optimization
  - `sig_atomic_t`: Guaranteed atomic read/write (no partial values)
- **Handler simplicity**: Only increments counter (no complex logic)
  - POSIX only allows async-signal-safe functions in handlers

#### Signal Registration

```c
int signals_init(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // Restart interrupted system calls

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        return -1;
    }
    return 0;
}
```

**`SA_RESTART` Flag:**
- Automatically restarts interrupted system calls
- Prevents `read()`, `write()`, `accept()` from returning `EINTR`
- Critical for maintaining transfer progress during signal

#### Shutdown Status Logic

```c
int signals_should_shutdown(void) {
    if (signal_count == 0) {
        return 0;  // Continue normal operation
    } else if (signal_count == 1) {
        return 1;  // First Ctrl+C - prompt user
    } else {
        return 2;  // Second+ Ctrl+C - force exit
    }
}

void signals_acknowledge_shutdown(void) {
    // Keep signal_count at 1 to prevent returning to 0
    // Allows second Ctrl+C to trigger force exit
    if (signal_count > 0) {
        signal_count = 1;
    }
}
```

### Server Integration Example

```c
// In server.c accept loop
while (1) {
    int shutdown = signals_should_shutdown();

    if (shutdown == 1) {
        // First Ctrl+C - prompt user
        printf("\nShutdown requested. Press Ctrl+C again to force exit...\n");
        signals_acknowledge_shutdown();
        LOG_WARN("Shutdown requested, waiting for transfer completion");
        // Continue to accept() to finish current transfer
    } else if (shutdown == 2) {
        // Second Ctrl+C - force exit
        LOG_WARN("Forced exit - closing server immediately");
        close_socket(server_socket);
        net_cleanup();
        exit(EXIT_SUCCESS);
    }

    // Accept new connections
    SOCKET_T client_socket = accept(server_socket, ...);
    // ... handle connection
}
```

### Protocol Integration Example

```c
// In protocol.c transfer loop
while (bytes_sent < file_size) {
    // Check for shutdown signal
    int shutdown = signals_should_shutdown();
    if (shutdown == 2) {
        LOG_WARN("Forced exit - cancelling transfer");
        fclose(file);
        return -1;
    }

    // Send chunk
    size_t chunk_size = adaptive_get_chunk_size(&adaptive);
    bytes_read = fread(buffer, 1, chunk_size, file);
    send_all(socket, buffer, bytes_read);

    // Update adaptive state
    adaptive_update(&adaptive, bytes_read, elapsed_time);
}
```

### Cross-Platform Considerations

| Platform | Implementation | Notes |
|----------|----------------|-------|
| Linux | Full POSIX `sigaction` | Full signal handling |
| macOS | Full POSIX `sigaction` | Full signal handling |
| Windows | Stub only | No Ctrl+C handling (returns 0) |

---

## 🆕 Adaptive Chunk Sizing

### `src/adaptive.h`

Dynamic chunk size adjustment based on network conditions for optimal throughput.

```c
// Chunk size limits
#define MIN_CHUNK_SIZE    (8 * 1024)       // 8 KB
#define MAX_CHUNK_SIZE    (2 * 1024 * 1024)  // 2 MB
#define INITIAL_CHUNK_SIZE (64 * 1024)     // 64 KB

// Adjustment parameters
#define ADJUSTMENT_INTERVAL 2  // Seconds between adjustments
#define SPEED_SAMPLES      5   // Rolling window size

typedef struct {
    size_t current_chunk_size;      // Current chunk size in bytes
    time_t last_adjustment_time;    // Last adjustment timestamp
    uint64_t bytes_transferred;     // Total bytes transferred
    double speed_samples[SPEED_SAMPLES];  // Speed history (bytes/sec)
    int sample_count;               // Number of samples collected
    uint64_t total_bytes;           // Total file size (0 if unknown)
    uint64_t bytes_sent_or_received; // Progress tracking
} AdaptiveState;

// Core functions
void adaptive_init(AdaptiveState *state, uint64_t total_bytes);
size_t adaptive_get_chunk_size(AdaptiveState *state);
void adaptive_update(AdaptiveState *state, size_t bytes_transferred, double elapsed_time);
void adaptive_reset(AdaptiveState *state);
double adaptive_get_current_speed(AdaptiveState *state);
void adaptive_format_chunk_size(size_t bytes, char *buffer, size_t buffer_size);
```

### `src/adaptive.c`

#### Initialization

```c
void adaptive_init(AdaptiveState *state, uint64_t total_bytes) {
    state->current_chunk_size = INITIAL_CHUNK_SIZE;
    state->last_adjustment_time = time(NULL);
    state->bytes_transferred = 0;
    state->sample_count = 0;
    state->total_bytes = total_bytes;
    state->bytes_sent_or_received = 0;

    for (int i = 0; i < SPEED_SAMPLES; i++) {
        state->speed_samples[i] = 0.0;
    }
}
```

**Design Rationale:**
- **Initial 64KB**: Balanced starting point (not too small, not too large)
- **Zero samples**: No speed data yet, starts with default
- **Time zero**: Resets adjustment timer

#### Update Cycle

```c
void adaptive_update(AdaptiveState *state, size_t bytes_transferred, double elapsed_time) {
    state->bytes_transferred += bytes_transferred;
    state->bytes_sent_or_received += bytes_transferred;

    // Calculate speed for this chunk
    double speed = bytes_transferred / elapsed_time;

    // Add to rolling window
    state->speed_samples[state->sample_count % SPEED_SAMPLES] = speed;
    state->sample_count++;

    // Check if it's time to adjust
    time_t now = time(NULL);
    if (now - state->last_adjustment_time >= ADJUSTMENT_INTERVAL) {
        adjust_chunk_size(state);
        state->last_adjustment_time = now;
    }
}
```

**Rolling Average Calculation:**
```c
static double get_average_speed(AdaptiveState *state) {
    if (state->sample_count == 0) {
        return 0.0;
    }

    int count = (state->sample_count < SPEED_SAMPLES) ?
                 state->sample_count : SPEED_SAMPLES;

    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += state->speed_samples[i];
    }
    return sum / count;
}
```

#### Adaptation Algorithm

```c
static void adjust_chunk_size(AdaptiveState *state) {
    double avg_speed = get_average_speed(state);

    // Convert to MB/s for decision making
    double speed_mb_per_sec = avg_speed / (1024.0 * 1024.0);

    // Aggressive adaptation based on speed tiers
    if (speed_mb_per_sec < 1.0) {
        state->current_chunk_size = MIN_CHUNK_SIZE;  // 8 KB
    } else if (speed_mb_per_sec < 10.0) {
        state->current_chunk_size = 64 * 1024;      // 64 KB
    } else if (speed_mb_per_sec < 50.0) {
        state->current_chunk_size = 256 * 1024;     // 256 KB
    } else if (speed_mb_per_sec < 100.0) {
        state->current_chunk_size = 1024 * 1024;    // 1 MB
    } else {
        state->current_chunk_size = MAX_CHUNK_SIZE; // 2 MB
    }
}
```

**Adaptation Tiers (Aggressive):**

| Network Speed | Chunk Size | Rationale |
|---------------|------------|-----------|
| < 1 MB/s | 8 KB | Minimize latency on slow connections |
| < 10 MB/s | 64 KB | Balanced for moderate speeds |
| < 50 MB/s | 256 KB | Higher throughput for fast networks |
| < 100 MB/s | 1 MB | Near-optimal for very fast networks |
| ≥ 100 MB/s | 2 MB | Maximum chunk for gigabit+ networks |

#### Utility Functions

```c
void adaptive_format_chunk_size(size_t bytes, char *buffer, size_t buffer_size) {
    if (bytes < 1024) {
        snprintf(buffer, buffer_size, "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, buffer_size, "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
}
```

### Protocol Integration

```c
// In send_file_protocol()
AdaptiveState adaptive;
adaptive_init(&adaptive, file_size);

char *buffer = malloc(MAX_CHUNK_BUFFER_SIZE);

while ((bytes_read = fread(buffer, 1, adaptive_get_chunk_size(&adaptive), file)) > 0) {
    clock_t start = clock();
    send_all(socket, buffer, bytes_read);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    adaptive_update(&adaptive, bytes_read, elapsed);

    // Display progress with chunk size
    char chunk_str[32];
    adaptive_format_chunk_size(adaptive.current_chunk_size, chunk_str, sizeof(chunk_str));
    printf("\rProgress: %.2f%% [%s chunk]", progress, chunk_str);
}

free(buffer);
```

### Performance Characteristics

**Benefits:**
- **Latency optimization**: Smaller chunks on slow networks reduce perceived lag
- **Throughput optimization**: Larger chunks on fast networks maximize bandwidth
- **Memory efficiency**: Dynamically allocates only needed buffer size
- **Smooth adaptation**: Rolling average prevents oscillation

**Trade-offs:**
- **Adjustment delay**: 2-second interval means response lag to speed changes
- **Memory overhead**: Maximum 2MB buffer allocation required
- **Complexity**: More state management than fixed chunk size

---

## Network Discovery System

### `src/discovery.h`

```c
#define DEFAULT_NETTF_PORT 9876

typedef struct {
    char ip_address[16];        // IPv4 address string (xxx.xxx.xxx.xxx)
    char mac_address[18];       // MAC address string (xx:xx:xx:xx:xx:xx)
    char hostname[256];         // Device hostname if available
    int is_active;             // 1 if device responded to ping, 0 otherwise
    int has_nettf_service;     // 1 if device has NETTF service running, 0 otherwise
    double response_time;      // Ping response time in milliseconds
} NetworkDevice;
```
- **Purpose**: Device information structure for network discovery
- **`ip_address`**: IPv4 address in dotted decimal notation
- **`mac_address`**: Hardware MAC address for device identification
- **`hostname`**: Resolved hostname if available via reverse DNS
- **`is_active`**: Device responded to network ping
- **`has_nettf_service`**: Device has NETTF service listening on port 9876
- **`response_time`**: Network latency in milliseconds

### `src/discovery.c`

```c
int discover_network_devices(NetworkDevice *devices, int max_devices, int check_services, int timeout_ms)
```
- **Purpose**: Main discovery function combining multiple scanning methods
- **ARP Table Scanning**: Reads system ARP table for known devices
- **Active Ping Sweep**: Scans network range for active devices
- **Service Detection**: Connects to port 9876 to check for NETTF service
- **Cross-platform**: Uses system-specific APIs for optimal performance

```c
int ping_sweep(const char *network, const char *netmask, NetworkDevice *devices, int max_devices, int timeout_ms)
```
- **Purpose**: Actively scan network range for responsive devices
- **Network Calculation**: Computes network range from IP and netmask
- **Parallel Pinging**: Uses non-blocking sockets for concurrent pings
- **Timeout Control**: Configurable timeout for responsiveness

---

## Client Module

### `src/client.c`

```c
void send_file(const char *target_ip, int port, const char *filepath) {
    net_init();

    SOCKET_T client_socket = socket(AF_INET, SOCK_STREAM, 0);
```
- **Purpose**: Create TCP socket
- **`AF_INET`**: IPv4 address family
- **`SOCK_STREAM`**: TCP (reliable, connection-oriented)
- **`0`**: Default protocol (TCP for SOCK_STREAM)

```c
    SOCKADDR_IN_T server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
```
- **Purpose**: Setup server address structure
- **`memset()`**: Zero-initialize structure
- **`AF_INET`**: IPv4
- **`htons()`**: Convert port to network byte order

```c
    if (inet_pton(AF_INET, target_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", target_ip);
        close_socket(client_socket);
        net_cleanup();
        exit(EXIT_FAILURE);
    }
```
- **Purpose**: Convert IP string to binary format
- **`inet_pton()`**: "Presentation to Network" - converts text IP to binary
- **`<= 0`**: Invalid IP address or error

```c
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        perror("connect");
        close_socket(client_socket);
        net_cleanup();
        exit(EXIT_FAILURE);
    }
```
- **Purpose**: Establish TCP connection
- **`connect()`**: Three-way handshake with server
- **Casts**: Convert from platform-specific to generic socket address

---

## Server Module

### `src/server.c`

```c
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == SOCKET_ERROR) {
        perror("setsockopt");
        close_socket(server_socket);
        net_cleanup();
        exit(EXIT_FAILURE);
    }
```
- **Purpose**: Allow immediate address reuse
- **`SO_REUSEADDR`**: Prevents "address already in use" errors
- **Useful when restarting server quickly**

```c
    server_addr.sin_addr.s_addr = INADDR_ANY;
```
- **Purpose**: Accept connections on any network interface
- **`INADDR_ANY`**: Listen on all available interfaces

```c
    if (listen(server_socket, 1) == SOCKET_ERROR) {
        perror("listen");
        close_socket(server_socket);
        net_cleanup();
        exit(EXIT_FAILURE);
    }
```
- **Purpose**: Start listening for connections
- **`1`**: Maximum pending connections in queue

```c
#ifdef _WIN32
    int client_addr_len = sizeof(client_addr);
#else
    socklen_t client_addr_len = sizeof(client_addr);
#endif
```
- **Purpose**: Handle type differences between platforms
- **Windows**: `int` for address length
- **POSIX**: `socklen_t` type

---

## Main Module

### `src/main.c`

```c
    if (strcmp(argv[1], "receive") == 0) {
        if (argc != 3) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
```
- **Purpose**: Command-line argument parsing
- **`strcmp()`**: String comparison for command matching
- **`argc`**: Argument count validation

```c
        int port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Error: Port must be between 1 and 65535\n");
            return EXIT_FAILURE;
        }
```
- **Purpose**: Port validation
- **`atoi()`**: Convert string to integer
- **Port range**: Valid TCP/UDP ports are 1-65535

---

## Build System

### `Makefile`

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = nettf
SRCDIR = src
OBJDIR = obj
```
- **Purpose**: Build configuration variables
- **`-Wall -Wextra`**: Enable all warnings
- **`-std=c99`**: Use C99 standard
- **`-O2`**: Optimization level 2

```makefile
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
```
- **Purpose**: Automatic file discovery
- **`$(wildcard)`**: Find all .c files
- **Pattern substitution**: Convert src/file.c to obj/file.o

```makefile
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
```
- **Purpose**: Compilation rule pattern
- **`$<`**: First prerequisite (source file)
- **`$@`**: Target name (object file)
- **`| $(OBJDIR)`**: Order-only prerequisite (create directory first)

---

## 🆕 Testing System

### `test/unity.h` and `test/unity.c`

NETTF uses the Unity Test Framework for unit testing. Unity is a lightweight, header-only testing framework designed for embedded systems and C projects.

**Unity Features:**
- **Header-only**: Minimal integration overhead
- **No external dependencies**: Self-contained testing
- **Cross-platform**: Works on any C99 compiler
- **Small footprint**: ~500 lines of code

### `test/Makefile`

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I.. -I../src

# Source files for tests
TEST_SOURCES = $(TESTDIR)/run_tests.c \
               $(TESTDIR)/test_logging.c \
               $(TESTDIR)/test_adaptive.c

# Unity framework
UNITY_SOURCES = $(TESTDIR)/unity.c

# Module source files (needed for testing)
MODULE_SOURCES = $(SRCDIR)/logging.c \
                 $(SRCDIR)/adaptive.c

# Build and run tests
test: $(TEST_TARGET)
	./$(TEST_TARGET)
```

### Test Structure

Each test file follows the Unity pattern:

```c
// test/test_adaptive.c
#include "unity.h"
#include "../src/adaptive.h"

void test_adaptive_init_sets_defaults(void) {
    AdaptiveState state;
    adaptive_init(&state, 1024 * 1024);

    TEST_ASSERT_EQUAL_size_t(INITIAL_CHUNK_SIZE, state.current_chunk_size);
    TEST_ASSERT_EQUAL(0, state.sample_count);
}

void test_adaptive_slow_network_reduces_chunk_size(void) {
    AdaptiveState state;
    adaptive_init(&state, 10 * 1024 * 1024);

    // Simulate slow network: ~100 KB/s
    for (int i = 0; i < SPEED_SAMPLES; i++) {
        adaptive_update(&state, 64 * 1024, 0.64);
    }

    // Force adjustment
    state.last_adjustment_time = 0;
    adaptive_update(&state, 64 * 1024, 0.64);

    TEST_ASSERT_EQUAL_size_t(MIN_CHUNK_SIZE, state.current_chunk_size);
}

int test_adaptive_runner(void) {
    UNITY_BEGIN();
    RUN_TEST(test_adaptive_init_sets_defaults);
    RUN_TEST(test_adaptive_slow_network_reduces_chunk_size);
    return UNITY_END();
}
```

### Unity Assertions

| Assertion | Purpose |
|-----------|---------|
| `TEST_ASSERT_EQUAL(expected, actual)` | Integer equality |
| `TEST_ASSERT_EQUAL_size_t(expected, actual)` | size_t equality |
| `TEST_ASSERT_TRUE(condition)` | Boolean true |
| `TEST_ASSERT_FALSE(condition)` | Boolean false |
| `TEST_ASSERT_NOT_NULL(pointer)` | Pointer non-null |
| `TEST_ASSERT_NULL(pointer)` | Pointer null |

### Test Runners

`test/run_tests.c` is the main test entry point:

```c
void setUp(void) {}
void tearDown(void) {}

int main(void) {
    printf("======================================\n");
    printf("NETTF Unit Test Suite\n");
    printf("======================================\n\n");

    int failures = 0;

    // Run logging tests
    printf("Running Logging Tests...\n");
    failures += test_logging_runner();

    // Run adaptive tests
    printf("Running Adaptive Chunk Sizing Tests...\n");
    failures += test_adaptive_runner();

    // Print summary
    if (failures == 0) {
        printf("All tests passed!\n");
    } else {
        printf("%d test(s) failed!\n", failures);
    }

    return failures > 0 ? 1 : 0;
}
```

### Running Tests

```bash
# Run unit tests via build script
./build.sh unit

# Or directly via Makefile
cd test && make test

# Clean test artifacts
cd test && make clean
```

### Current Test Coverage

**Logging Tests (`test_logging.c`):**
- `test_log_init_creates_file` - Verifies log file creation
- `test_log_message_writes_to_file` - Tests message writing
- `test_log_levels` - Verifies all log levels work
- `test_timestamp_format` - Checks timestamp format
- `test_log_multiple_init` - Tests idempotent init
- `test_log_without_init` - Tests safe no-op behavior

**Adaptive Tests (`test_adaptive.c`):**
- `test_adaptive_init_sets_defaults` - Initialization values
- `test_adaptive_get_chunk_size_in_range` - Chunk size bounds
- `test_adaptive_slow_network_reduces_chunk_size` - Slow network adaptation
- `test_adaptive_fast_network_increases_chunk_size` - Fast network adaptation
- `test_adaptive_moderate_speed` - Moderate speed handling
- `test_adaptive_reset_preserves_chunk_size` - Reset behavior
- `test_adaptive_format_chunk_size_kb` - KB formatting
- `test_adaptive_format_chunk_size_mb` - MB formatting
- `test_adaptive_get_current_speed_no_samples` - No samples case
- `test_adaptive_get_current_speed_with_samples` - With samples case
- `test_adaptive_update_increases_samples` - Sample increment
- `test_adaptive_get_chunk_size_returns_current` - Current size getter
- `test_adaptive_init_zero_size` - Unknown file size handling

### Test Results Example

```
======================================
NETTF Unit Test Suite
======================================

Running Logging Tests...
--------------------------------------
test_log_init_creates_file ... OK
test_log_message_writes_to_file ... OK
test_log_levels ... OK
test_timestamp_format ... OK
test_log_multiple_init ... OK
test_log_without_init ... OK

Running Adaptive Chunk Sizing Tests...
--------------------------------------
test_adaptive_init_sets_defaults ... OK
test_adaptive_get_chunk_size_in_range ... OK
test_adaptive_slow_network_reduces_chunk_size ... OK
test_adaptive_fast_network_increases_chunk_size ... OK
...

======================================
All tests passed!
======================================
```

---

## Key Concepts Explained

### Architectural Design Patterns

#### 1. Platform Abstraction Pattern
The codebase uses conditional compilation to abstract platform differences:
- **Header Files**: `platform.h` defines unified types and macros
- **Implementation**: `platform.c` provides platform-specific implementations
- **Benefits**: Single codebase supports Windows, Linux, and macOS

```c
#ifdef _WIN32
    // Windows-specific code
    #define SOCKET_T SOCKET
#else
    // POSIX-specific code (Linux/macOS)
    typedef int SOCKET_T;
#endif
```

#### 2. Protocol Versioning with Magic Numbers
Magic numbers enable backward-compatible protocol evolution:
- **Type Safety**: First 4 bytes identify transfer type
- **Extensibility**: New protocols can be added without breaking old clients
- **Detection**: Receivers automatically detect protocol capabilities

```c
uint32_t magic;
recv_all(s, &magic, sizeof(magic));
switch (ntohl(magic)) {
    case FILE_MAGIC: /* Standard file */
    case DIR_MAGIC:  /* Standard directory */
    case TARGET_FILE_MAGIC: /* New target file protocol */
    case TARGET_DIR_MAGIC:  /* New target directory protocol */
}
```

#### 3. Chunked Data Transfer
Large files are transferred in fixed-size chunks:
- **Memory Efficiency**: Constant 64KB buffer regardless of file size
- **Network Optimization**: Balments throughput and memory usage
- **Progress Tracking**: Easy to calculate and display transfer progress
- **Error Recovery**: Failed transfers can be resumed from last chunk

#### 4. Security-First Validation
All user inputs are validated before use:
- **Path Sanitization**: Strip directory components from filenames
- **Traversal Prevention**: Block `..` sequences in target directories
- **Length Validation**: Prevent buffer overflows with bounds checking
- **Type Safety**: Use `size_t` for sizes, `uint64_t` for protocol fields

#### 5. Resource Acquisition Is Initialization (RAII)
Resources are properly managed in error cases:
- **Socket Cleanup**: Always close sockets before exit
- **File Handles**: Always `fclose()` even on errors
- **Memory Management**: All `malloc()` calls have corresponding `free()`
- **Network Cleanup**: `WSACleanup()` on Windows in all code paths

### Network Byte Order
- **Big Endian**: Most significant byte first (network standard)
- **Little Endian**: Most significant byte last (x86 processors)
- **`htonl/htons`**: Host to Network conversion
- **`ntohl/ntohs`**: Network to Host conversion

### Socket Programming Flow
1. **Server**: socket() → bind() → listen() → accept() → recv/send() → close()
2. **Client**: socket() → connect() → send/recv() → close()

### Error Handling Patterns

The codebase follows a consistent error handling strategy that prioritizes security and reliability.

#### 1. System Call Return Value Checking
All system calls are checked for errors immediately after invocation:

```c
// Socket operations
if (connect(client_socket, ...) == SOCKET_ERROR) {
    perror("connect");  // Print system error message
    close_socket(client_socket);
    net_cleanup();
    exit(EXIT_FAILURE);
}

// File operations
if ((file = fopen(filepath, "rb")) == NULL) {
    perror("fopen");
    exit(EXIT_FAILURE);
}

// Memory allocation
if ((buffer = malloc(size)) == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(EXIT_FAILURE);
}
```

#### 2. Resource Cleanup on Error
Always clean up resources before exiting, even in error cases:

```c
// Pattern: Initialize to NULL/invalid value
FILE *file = NULL;
SOCKET_T sock = INVALID_SOCKET_T;
char *buffer = NULL;

// Allocate resources
file = fopen(...);
sock = socket(...);
buffer = malloc(...);

// On error: clean up everything
if (error) {
    if (file) fclose(file);
    if (sock != INVALID_SOCKET_T) close_socket(sock);
    if (buffer) free(buffer);
    exit(EXIT_FAILURE);
}
```

#### 3. Network Error Recovery
Handle partial sends and connection failures:

```c
// Handle partial sends
int send_all(SOCKET_T s, const void *data, size_t len) {
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = send(s, ptr + total_sent, len - total_sent, 0);
        if (sent == SOCKET_ERROR) {
            return -1;  // Error
        }
        if (sent == 0) {
            return -1;  // Connection closed
        }
        total_sent += sent;
    }
    return 0;
}
```

#### 4. Input Validation
Validate all user inputs before processing:

```c
// Port validation
int port = atoi(argv[2]);
if (port <= 0 || port > 65535) {
    fprintf(stderr, "Error: Port must be between 1 and 65535\n");
    return EXIT_FAILURE;
}

// IP address validation
if (inet_pton(AF_INET, target_ip, &server_addr.sin_addr) <= 0) {
    fprintf(stderr, "Invalid IP address: %s\n", target_ip);
    return EXIT_FAILURE;
}

// File existence validation
if (stat(filepath, &st) != 0) {
    perror("stat");
    return EXIT_FAILURE;
}
```

#### 5. Security-Focused Error Messages
Error messages provide debugging information without exposing security details:

```c
// Good: Descriptive but safe
fprintf(stderr, "Error: Path traversal detected in target directory\n");

// Bad: Could leak information
fprintf(stderr, "Error: /etc/passwd not accessible\n");

// Good: Generic security error
fprintf(stderr, "Error: Invalid target directory path\n");

// Good: Explains the restriction
fprintf(stderr, "Error: Absolute paths not allowed in target directory\n");
```

### Memory Management
- `malloc()` for dynamic allocation
- Always check for allocation failure
- `free()` when done
- `fclose()` for file handles

### Data Flow and Transfer Lifecycle

#### Standard File Transfer Flow

**Sender Side (Client):**
```
1. User Input: ./nettf send 192.168.1.100 file.txt
2. Argument Parsing: Extract IP, filepath, target_dir (optional)
3. File Validation: Check file exists, get size via stat()
4. Network Initialization: net_init() -> WSAStartup() on Windows
5. Socket Creation: socket(AF_INET, SOCK_STREAM, 0)
6. Server Connection: connect() to target:9876
7. Protocol Selection: Choose FILE_MAGIC or TARGET_FILE_MAGIC
8. Send Magic Number: 4 bytes (0x46494C45 or 0x54415247)
9. Prepare Header: Pack file_size, filename_len, target_dir_len
10. Send Header: 16 or 32 bytes in network byte order
11. Send Filename: Only basename (no directory path)
12. Send Target Dir: If specified (for TARGET_FILE_MAGIC)
13. Transfer Content: Loop with 64KB chunks
    - fread(buffer, 1, CHUNK_SIZE, file)
    - send_all(client_socket, buffer, bytes_read)
    - Update progress display
14. Cleanup: fclose(), close_socket(), net_cleanup()
```

**Receiver Side (Server):**
```
1. User Input: ./nettf receive
2. Network Initialization: net_init()
3. Socket Creation: socket(AF_INET, SOCK_STREAM, 0)
4. Socket Options: setsockopt(SO_REUSEADDR)
5. Bind Address: bind() to 0.0.0.0:9876
6. Start Listening: listen() with backlog of 1
7. Accept Connection: accept() blocks until client connects
8. Detect Transfer Type: recv() 4 bytes magic number
9. Parse Header: Based on magic number type
    - FILE_MAGIC: 16 bytes header
    - TARGET_FILE_MAGIC: 32 bytes header
10. Receive Filename: Allocate buffer, recv() filename
11. Receive Target Dir: If applicable (for TARGET_FILE_MAGIC)
12. Create Target Directory: mkdir() if needed (for TARGET_FILE_MAGIC)
13. Create File: fopen() in write binary mode
14. Receive Content: Loop with 64KB chunks
    - recv() chunk into buffer
    - fwrite(buffer, 1, bytes_received, file)
    - Update progress display
15. Finalize: fclose(), verify transfer complete
16. Cleanup: close_socket(), net_cleanup()
```

#### Directory Transfer Flow

**Sender Side (Client):**
```
1. User Input: ./nettf send 192.168.1.100 directory/
2. Directory Validation: Check path is directory via stat()
3. Directory Scan: count_directory_files()
    - Recursive traversal with opendir()/readdir()
    - Skip "." and ".." entries
    - Count files and calculate total size
4. Network Initialization: net_init()
5. Socket Creation and Connection: Same as file transfer
6. Send Magic: DIR_MAGIC (0x44495220) or TARGET_DIR_MAGIC (0x54444952)
7. Send Directory Header: total_files, total_size, base_path_len, target_dir_len
8. Send Directory Name: Only basename of directory
9. Send Target Directory: If applicable (for TARGET_DIR_MAGIC)
10. Transfer Files: For each file in directory
    - Build relative path from base directory
    - Send file header with relative path
    - Send file content in chunks
    - Update per-file and overall progress
11. Cleanup: Close directory, close socket, net_cleanup()
```

**Receiver Side (Server):**
```
1. User Input: ./nettf receive
2. Server Setup: Same as file transfer (bind/listen/accept)
3. Detect Transfer Type: recv() magic -> DIR_MAGIC or TARGET_DIR_MAGIC
4. Parse Directory Header: Extract metadata
5. Receive Directory Name: Base name of source directory
6. Receive Target Directory: If applicable (for TARGET_DIR_MAGIC)
7. Create Directory Structure: create_directory_recursive()
    - Create base directory
    - Create target directory if specified
    - Build full path: target_dir/base_dir
8. Receive Files: Loop for total_files
    - Receive file header with relative path
    - Create subdirectories as needed
    - Create file and receive content
    - Update progress display
9. Finalize: Verify all files received
10. Cleanup: Close directory, close socket, net_cleanup()
```

#### Network Discovery Flow

```
1. User Input: ./nettf discover [--timeout <ms>]
2. Network Interface Enumeration:
    - Get all network interfaces (Windows: GetAdaptersInfo, POSIX: ioctl)
    - Extract IP addresses and netmasks
    - Determine network class (10.x, 172.16-31, 192.168, or public)
3. ARP Table Scanning:
    - Read system ARP table
    - Parse IP/MAC mappings
    - Store known devices
4. Ping Sweep (if needed):
    - Calculate network range from IP/netmask
    - Send ICMP echo requests to all hosts
    - Wait for responses with timeout
5. Service Detection:
    - For each active device, try connect() to port 9876
    - If connection succeeds, device has NETTF service
    - Close connection immediately
6. Hostname Resolution:
    - Use reverse DNS (getnameinfo/gethostbyaddr)
    - Resolve IP to hostname
7. Display Results:
    - Sort devices by IP address
    - Format table with IP, MAC, hostname, active status, service status
    - Show summary statistics
```

---

## New Features in Latest Version

### 1. Network Discovery System

**Purpose**: Automatically discover NETTF-enabled devices on the local network

**Key Components**:
- `src/discovery.h/c`: Complete network discovery implementation
- Cross-platform network interface enumeration
- ARP table scanning for known devices
- Active ping sweeps for device discovery
- Service detection on default port 9876

**Usage**:
```bash
./nettf discover [--timeout <ms>]
```

**Features**:
- Automatic network range detection
- Configurable timeout for slow networks
- Hostname resolution via reverse DNS
- Service availability checking
- Responsive table-based output

### 2. Enhanced Protocol with Magic Numbers

**Purpose**: Support both file and directory transfers with automatic type detection

**Protocol Enhancements**:
- Magic numbers (`0x46494C45` for files, `0x44495220` for directories)
- 64-bit filename length support (eliminates 4GB limitation)
- Directory metadata with file count and total size
- Improved chunk size (64KB for optimal performance)

**File Transfer Flow**:
1. Send magic number (4 bytes)
2. Send file header (16 bytes)
3. Send filename (variable length)
4. Send file content (64KB chunks)

**Directory Transfer Flow**:
1. Send magic number (4 bytes)
2. Send directory header (24 bytes)
3. Send base directory name (variable length)
4. Send each file using file transfer protocol with relative paths

### 3. Directory Transfer Support

**Purpose**: Transfer entire directories with preserved structure

**Implementation Details**:
- Recursive directory traversal
- Relative path preservation
- Automatic directory creation on receiver
- Progress tracking per-file and overall

**Key Functions**:
- `send_directory_protocol()`: Sends directory with metadata
- `recv_directory_protocol()`: Receives and reconstructs directory
- `count_directory_files()`: Pre-scan for metadata
- `create_directory_recursive()`: Creates directory structure

### 4. Improved Progress Tracking

**Enhanced Display Features**:
- Real-time transfer speed calculation
- Time remaining estimation
- Progress bar visualization
- File-by-file progress for directories
- Formatted byte display (B, KB, MB, GB, TB)

**Helper Functions**:
- `format_bytes()`: Human-readable byte formatting
- `format_speed()`: Transfer speed with appropriate units
- `format_time()`: Time duration formatting

### 5. Cross-Platform Build Enhancements

**Linux/macOS Build Script** (`build.sh`):
- Enhanced dependency checking
- Automated testing integration
- System detection and validation
- Color output and error logging
- Installation and uninstallation support

**Build Features**:
- Dependency verification before compilation
- Cross-platform compatibility checks
- Automated cleanup and error handling
- Build information display

### 6. Enhanced Error Handling and Security

**Security Improvements**:
- Enhanced path stripping for directory traversal prevention
- Input validation for all user-provided parameters
- Memory safety improvements with proper bounds checking
- Network timeout configuration

**Error Handling**:
- Comprehensive error messages
- Graceful failure recovery
- Resource cleanup on errors
- Network connection timeout handling

### 7. Protocol Backwards Compatibility

**Version Support**:
- Magic number detection for protocol versioning
- Graceful fallback to older protocol versions
- Forward compatibility planning

**Implementation**:
- `detect_transfer_type()`: Automatic protocol detection
- Backward compatibility headers
- Version negotiation support

### 8. Performance Optimizations

**Transfer Speed Improvements**:
- 64KB chunk size (increased from 4KB)
- Reduced system call overhead
- Better memory management
- Optimized TCP socket settings

**Memory Efficiency**:
- Constant memory footprint regardless of file size
- Efficient directory metadata handling
- Minimal memory allocations during transfers

### 9. 🆕 Target Directory Support

**Purpose**: Allow senders to specify exactly where files and directories should be saved on the receiver

**Key Features**:
- **Optional Target Directory**: Fourth parameter allows specifying destination directory
- **Automatic Directory Creation**: Target directories are created if they don't exist
- **Enhanced Security**: Comprehensive path validation prevents security issues
- **Backward Compatible**: Existing commands continue to work unchanged

**Security Enhancements**:
- **Path Sanitization**: Blocks directory traversal attempts (`..` sequences)
- **Absolute Path Prevention**: Disallows absolute paths starting with `/`
- **Input Validation**: Comprehensive validation of all target directory inputs
- **Isolation**: All operations restricted to receiver's working directory

**Protocol Extensions**:
- **New Magic Numbers**: `0x54415247` (TARG) and `0x54444952` (TDIR)
- **Enhanced Headers**: Additional `target_dir_len` field in protocol headers
- **Four Transfer Types**: Standard file/directory + target file/directory
- **Protocol Detection**: Enhanced detection function supports all 4 types

**Implementation Details**:
- `send_file_with_target_protocol()`: Enhanced file sending with target directory
- `send_directory_with_target_protocol()`: Enhanced directory sending with target directory
- `recv_file_with_target_protocol()`: Enhanced file receiving with directory creation
- `recv_directory_with_target_protocol()`: Enhanced directory receiving with target structure
- `validate_target_directory()`: Security validation function for target paths

**Usage Examples**:
```bash
# Standard file transfer (backward compatible)
./nettf send 192.168.1.100 file.txt

# File with target directory
./nettf send 192.168.1.100 file.txt downloads/

# Directory with target directory
./nettf send 192.168.1.100 project/ backups/

# Nested target directory
./nettf send 192.168.1.100 video.mp4 media/movies/
```

### 10. Enhanced CLI Interface

**Command Line Improvements**:
- Three-mode operation: discover, send, receive
- **Optional Target Directory**: Fourth parameter for destination specification
- Optional timeout parameter for discovery
- Simplified syntax with default port (9876)
- Better error messages and usage information

**Command Examples**:
```bash
./nettf discover                    # Network discovery
./nettf discover --timeout 2000    # Custom timeout
./nettf receive                     # Start receiver
./nettf send 192.168.1.100 file.txt # Send file
./nettf send 192.168.1.100 dir/     # Send directory
./nettf send 192.168.1.100 file.txt downloads/  # Send to target directory
./nettf send 192.168.1.100 project/ backup/      # Send directory to target
```

### 11. 🆕 Adaptive Chunk Sizing

**Purpose**: Optimize transfer performance by dynamically adjusting chunk size based on network conditions

**Key Features**:
- **Dynamic Range**: 8KB to 2MB chunk sizes
- **Speed-Based Adaptation**: 5 speed tiers from <1 MB/s to ≥100 MB/s
- **Rolling Average**: 5-sample window prevents oscillation
- **2-Second Adjustment**: Aggressive response to changing conditions

**Speed Tiers**:
- < 1 MB/s → 8 KB (minimize latency on slow networks)
- < 10 MB/s → 64 KB (balanced for moderate speeds)
- < 50 MB/s → 256 KB (higher throughput for fast networks)
- < 100 MB/s → 1 MB (near-optimal for very fast networks)
- ≥ 100 MB/s → 2 MB (maximum for gigabit+ networks)

**Benefits**:
- Reduced latency on slow connections
- Maximum throughput on fast connections
- Memory-efficient (dynamically allocated)
- Smooth adaptation without oscillation

### 12. 🆕 Comprehensive Logging System

**Purpose**: Provide file-based logging for debugging, monitoring, and audit trails

**Key Features**:
- **File-Based**: Append-only logging to `./nettf.log`
- **Log Levels**: DEBUG, INFO, WARN, ERROR
- **Timestamp Format**: `[YYYY-MM-DD HH:MM:SS] [LEVEL] message`
- **Safe No-Op**: Logging without initialization is safe (silently fails)
- **Idempotent Init**: Multiple `log_init()` calls are safe

**Usage Throughout Codebase**:
- **main.c**: Program start/shutdown, command parsing
- **client.c**: Connection attempts, transfer progress
- **server.c**: Connection acceptance, transfer completion
- **protocol.c**: File operations, transfer milestones

**Example Log Output**:
```
[2024-12-29 14:32:15] [INFO] NETTF started with 3 arguments
[2024-12-29 14:32:20] [INFO] Connection established from 192.168.1.10:54321
[2024-12-29 14:32:20] [INFO] Receiving file: document.pdf (2.4 MB)
[2024-12-29 14:32:25] [WARN] Shutdown requested, waiting for transfer completion
[2024-12-29 14:32:30] [INFO] NETTF shutting down
```

### 13. 🆕 Graceful Signal Handling

**Purpose**: Enable clean shutdown on Ctrl+C (SIGINT) with user prompts

**Key Features**:
- **First Ctrl+C**: Soft shutdown (display prompt, continue transfer)
- **Second Ctrl+C**: Hard shutdown (force exit with cleanup)
- **SA_RESTART Flag**: Automatically restarts interrupted system calls
- **Atomic Counter**: Thread-safe signal count using `sig_atomic_t`
- **POSIX Only**: Full support on Linux/macOS, stub on Windows

**Implementation Details**:
- **Handler Simplicity**: Only increments counter (POSIX requirement)
- **Server Integration**: Check signals in accept loop
- **Protocol Integration**: Check signals after each chunk
- **Clean Cleanup**: Properly closes sockets, files, and resources

**User Experience**:
```bash
$ ./nettf receive
Listening on port 9876...
Connection established from 192.168.1.10:54321
Receiving file: large_file.bin (5.2 GB)
^C
Shutdown requested. Press Ctrl+C again to force exit...
# Transfer continues to completion
File received successfully!
```

### 14. 🆕 Unit Testing Framework

**Purpose**: Ensure code quality and prevent regressions through automated testing

**Framework Choice**: Unity Test Framework
- **Lightweight**: Header-only, ~500 lines of code
- **No Dependencies**: Self-contained testing
- **Cross-Platform**: Works with any C99 compiler
- **Simple API**: Easy-to-write tests with clear assertions

**Test Coverage**:
- **Logging Tests** (6 tests): File creation, message writing, log levels, timestamps
- **Adaptive Tests** (13 tests): Initialization, speed tiers, chunk size bounds, formatting

**Running Tests**:
```bash
./build.sh unit           # Run via build script
cd test && make test      # Run directly
```

**Example Test**:
```c
void test_adaptive_slow_network_reduces_chunk_size(void) {
    AdaptiveState state;
    adaptive_init(&state, 10 * 1024 * 1024);

    // Simulate slow network: ~100 KB/s
    for (int i = 0; i < SPEED_SAMPLES; i++) {
        adaptive_update(&state, 64 * 1024, 0.64);
    }

    // Force adjustment
    state.last_adjustment_time = 0;
    adaptive_update(&state, 64 * 1024, 0.64);

    TEST_ASSERT_EQUAL_size_t(MIN_CHUNK_SIZE, state.current_chunk_size);
}
```

This documentation explains each line's purpose and the underlying concepts. The code demonstrates professional C programming practices including error handling, cross-platform compatibility, and network programming fundamentals.
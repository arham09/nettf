# NETTF File Transfer Tool - Code Documentation

## Table of Contents
- [Platform Abstraction Layer](#platform-abstraction-layer)
- [Protocol Layer](#protocol-layer)
- [Client Module](#client-module)
- [Server Module](#server-module)
- [Main Module](#main-module)
- [Build System](#build-system)

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
- **`value & 0xFFFFFFFF`**: Extract lower 32 bits
- **`value >> 32`**: Extract upper 32 bits
- **`<< 32`**: Shift upper bits to correct position

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

## Protocol Layer

### `src/protocol.h`

```c
#define HEADER_SIZE 12
#define CHUNK_SIZE 4096
```
- **Purpose**: Protocol constants
- **`HEADER_SIZE`**: 8 bytes (file_size) + 4 bytes (filename_len)
- **`CHUNK_SIZE`**: Optimal size for file transfer chunks

```c
typedef struct {
    uint64_t file_size;
    uint32_t filename_len;
} FileHeader;
```
- **Purpose**: Protocol header structure
- **`uint64_t`**: 8-byte file size (supports files up to 16 exabytes)
- **`uint32_t`**: 4-byte filename length (supports up to 4GB filenames)

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

## Key Concepts Explained

### Network Byte Order
- **Big Endian**: Most significant byte first (network standard)
- **Little Endian**: Most significant byte last (x86 processors)
- **`htonl/htons`**: Host to Network conversion
- **`ntohl/ntohs`**: Network to Host conversion

### Socket Programming Flow
1. **Server**: socket() → bind() → listen() → accept() → recv/send() → close()
2. **Client**: socket() → connect() → send/recv() → close()

### Error Handling Patterns
- Check return values of all system calls
- Use `perror()` for system errors
- Clean up resources before exit
- Provide meaningful error messages

### Memory Management
- `malloc()` for dynamic allocation
- Always check for allocation failure
- `free()` when done
- `fclose()` for file handles

This documentation explains each line's purpose and the underlying concepts. The code demonstrates professional C programming practices including error handling, cross-platform compatibility, and network programming fundamentals.
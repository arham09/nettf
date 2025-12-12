# NETTF File Transfer Tool

A lightweight, peer-to-peer file transfer CLI tool written in C that works across Windows, Linux, and macOS.

## Features

- **Cross-platform**: Works on Windows (Winsock2), Linux, and macOS (POSIX sockets)
- **Simple CLI**: Easy-to-use command-line interface
- **LAN transfers**: Optimized for same-network transfers
- **Progress tracking**: Shows transfer progress in real-time
- **Error handling**: Comprehensive error checking and reporting
- **Protocol**: Strict 16-byte header with network byte order for cross-architecture compatibility
- **Large file support**: Can transfer files up to 16 exabytes (theoretical limit)

## File Size Support

The tool can now transfer extremely large files thanks to the updated protocol:

- **File size limit**: Up to 16 exabytes (2^64 - 1 bytes)
- **Filename length limit**: Up to 16 exabytes (though practical limits are much lower)
- **Memory efficient**: Uses 4KB chunks for transfer, keeping memory usage low
- **Progress tracking**: Shows transfer progress as percentage completed

**Real-world considerations:**
- 20GB+ files transfer reliably with stable network connections
- Transfer speed depends on network bandwidth and latency
- Progress tracking helps monitor large file transfers
- Automatic retry on network interruptions (implementation can be added)

## Building

This project supports multiple platforms with dedicated build scripts for each operating system.

### Linux (Recommended) 🐧

#### Method 1: Enhanced Build Script (Recommended)
```bash
# Full build process (clean, build, test)
./build.sh

# Individual commands
./build.sh build      # Build only
./build.sh clean      # Clean artifacts
./build.sh test       # Run tests
./build.sh install    # Install to /usr/local/bin
./build.sh uninstall  # Remove from system
./build.sh info       # Build information
./build.sh check      # Check dependencies
./build.sh help       # Show help
```

#### Method 2: Traditional Makefile
```bash
# Build the project
make

# Clean build files
make clean

# Install to system (requires sudo)
make install

# Remove from system
make uninstall
```

#### Dependencies
```bash
# Ubuntu/Debian
sudo apt-get update && sudo apt-get install build-essential

# CentOS/RHEL
sudo yum groupinstall 'Development Tools'

# Fedora
sudo dnf groupinstall 'Development Tools'

# Arch Linux
sudo pacman -S base-devel
```

### macOS 🍎

#### Method 1: Enhanced Build Script (Recommended)
```bash
# Full build process (clean, build, test)
./build.sh

# Individual commands (same as Linux)
./build.sh build      # Build only
./build.sh clean      # Clean artifacts
./build.sh test       # Run tests
./build.sh install    # Install to /usr/local/bin
./build.sh info       # Build information
```

#### Method 2: Traditional Makefile
```bash
# Build the project
make

# Clean build files
make clean

# Install to system (requires sudo)
make install

# Remove from system
make uninstall
```

#### Prerequisites
```bash
# Install Xcode Command Line Tools (includes Clang)
xcode-select --install

# Or using Homebrew
brew install gcc
```

### Windows 🪟

#### Method 1: Batch Script (Recommended)
```cmd
# Build the project
build.bat

# The batch script handles:
# - Creating object directory
# - Compiling source files with MinGW/GCC
# - Linking with Winsock library
# - Error checking and cleanup
```

#### Method 2: Manual Compilation (with MinGW)
```cmd
# Create object directory
mkdir obj

# Compile source files
gcc -Wall -Wextra -std=c99 -O2 -c src/platform.c -o obj/platform.o
gcc -Wall -Wextra -std=c99 -O2 -c src/protocol.c -o obj/protocol.o
gcc -Wall -Wextra -std=c99 -O2 -c src/client.c -o obj/client.o
gcc -Wall -Wextra -std=c99 -O2 -c src/server.c -o obj/server.o
gcc -Wall -Wextra -std=c99 -O2 -c src/main.c -o obj/main.o

# Link executable
gcc obj/*.o -o nettf.exe -lws2_32
```

#### Prerequisites
```cmd
# Option 1: Install MinGW-w64
# Download from: https://www.mingw-w64.org/

# Option 2: Use MSYS2
# Download from: https://www.msys2.org/
# Then install:
pacman -S mingw-w64-x86_64-gcc

# Option 3: Use WSL (Windows Subsystem for Linux)
wsl --install
# Follow Linux instructions inside WSL
```

### Build Verification

After building on any platform, verify the executable:

```bash
# Linux/macOS
./nettf
# Should show usage information

# Windows
nettf.exe
# Should show usage information
```

### Cross-Platform Build Matrix

| Platform | Build Method | Executable Name | Dependencies |
|----------|--------------|-----------------|--------------|
| Linux | `./build.sh` (recommended) or `make` | `nettf` | GCC, make, libc-dev |
| macOS | `./build.sh` (recommended) or `make` | `nettf` | Xcode CLI or GCC |
| Windows | `build.bat` (recommended) | `nettf.exe` | MinGW-w64 or MSYS2 |

### Build Features Comparison

| Feature | Linux build.sh | macOS build.sh | Windows build.bat | Makefile |
|---------|----------------|---------------|-------------------|----------|
| Dependency checking | ✅ | ✅ | ❌ | ❌ |
| System detection | ✅ | ✅ | ❌ | ❌ |
| Automated testing | ✅ | ✅ | ❌ | ❌ |
| Installation support | ✅ | ✅ | ❌ | ✅ |
| Color output | ✅ | ✅ | ❌ | ❌ |
| Error logging | ✅ | ✅ | ❌ | ❌ |
| Progress tracking | ✅ | ✅ | ✅ | ✅ |

## Usage

### Receive a file (Server)
```bash
./nettf receive
```

Example:
```bash
./nettf receive
```

### Send a file (Client)
```bash
./nettf send <TARGET_IP> <FILE_PATH>
```

Example:
```bash
./nettf send 192.168.1.100 /path/to/file.txt
```

## Protocol Specification

The tool uses a TCP-based protocol with the following byte-stream format:

1. **Header (16 bytes)**
   - `uint64_t file_size` (8 bytes, Network Byte Order/Big Endian)
   - `uint64_t filename_len` (8 bytes, Network Byte Order/Big Endian)

2. **Filename Payload**
   - Variable length character array (`filename_len` bytes)
   - Directory paths are stripped for security

3. **File Content**
   - Raw bytes of the file (`file_size` bytes)
   - Transferred in 4KB chunks

## Architecture

- **platform.h/c**: Cross-platform socket abstraction
- **protocol.h/c**: Data serialization and transfer functions
- **client.c**: Sender logic (connects to target)
- **server.c**: Receiver logic (listens for connections)
- **main.c**: CLI argument parsing and program coordination

## Security Features

- Filename path stripping to prevent arbitrary file writes
- Binary mode file operations to prevent data corruption
- Comprehensive error handling on all network/file operations
- Network byte order conversion for cross-architecture compatibility

## Example Session

**Receiver:**
```bash
$ ./nettf receive 8080
Listening on port 8080...
Waiting for incoming connection...
Connection established from 192.168.1.20:54321
Receiving file: test.txt (47 bytes)
Progress: 100.00%
File received successfully!
```

**Sender:**
```bash
$ ./nettf send 192.168.1.10 8080 test.txt
Connecting to 192.168.1.10:8080...
Connected! Sending file: test.txt
Progress: 100.00%
File sent successfully!
```

## Cleaning Build Files

```bash
make clean
```
# NETTF File Transfer Tool

A high-performance, peer-to-peer file transfer CLI tool written in C. Cross-platform support for Windows, Linux, and macOS with network device discovery and adaptive chunk sizing.

## Features

- **Cross-platform**: Windows (Winsock2), Linux, and macOS (POSIX sockets)
- **Directory Transfer**: Send entire directories with preserved structure
- **Target Directory Support**: Send files/directories to specific receiver directories
- **Large File Support**: Transfer files up to 16 exabytes
- **Progress Tracking**: Real-time progress with speed and chunk size display

## Building

### Linux / macOS

```bash
# Quick build
./build.sh

# Or using make
make

# Install to /usr/local/bin
make install
```

**Dependencies:**
- Linux: `sudo apt-get install build-essential`
- macOS: `xcode-select --install`

### Windows

```cmd
# Using build script
build.bat

# Or manually with MinGW
mkdir obj
gcc -Wall -Wextra -std=c99 -O2 -c src/*.c -o obj/
gcc obj/*.o -o nettf.exe -lws2_32
```

**Requirements:** MinGW-w64 or MSYS2

## Usage

### Discover Devices

```bash
./nettf discover [--timeout <ms>]
```

### Receive Files (Server)

```bash
./nettf receive
# Listens on port 9876
```

### Send Files/Directories (Client)

```bash
# Send file
./nettf send <TARGET_IP> <FILE_PATH>

# Send to specific directory
./nettf send <TARGET_IP> <FILE_PATH> <TARGET_DIR>

# Send directory
./nettf send <TARGET_IP> <DIRECTORY_PATH>

# Send directory to specific target
./nettf send <TARGET_IP> <DIRECTORY_PATH> <TARGET_DIR>
```

### Examples

```bash
# Terminal 1: Start receiver
./nettf receive

# Terminal 2: Send file
./nettf send 192.168.1.100 document.pdf

# Send to downloads folder
./nettf send 192.168.1.100 video.mp4 downloads/

# Send entire directory
./nettf send 192.168.1.100 photos/

# Backup to specific folder
./nettf send 192.168.1.100 project/ backups/
```

## Architecture

```
src/
├── platform.h/c    # Cross-platform socket abstraction
├── protocol.h/c    # File transfer protocol with magic numbers
├── adaptive.h/c    # Adaptive chunk sizing (8KB-2MB)
├── signals.h/c     # POSIX signal handling
├── discovery.h/c   # Network device discovery
├── client.c        # Sender implementation
├── server.c        # Receiver implementation
└── main.c          # CLI entry point
```

## License

Provided as-is for educational and practical use.

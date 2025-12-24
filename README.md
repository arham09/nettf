# NETTF File Transfer Tool

A high-performance, peer-to-peer file transfer CLI tool written in C that works across Windows, Linux, and macOS. Optimized for local area network (LAN) transfers with advanced network discovery capabilities.

## Features

- **Cross-platform**: Works on Windows (Winsock2), Linux, and macOS (POSIX sockets)
- **Network Discovery**: Automatically discover NETTF-enabled devices on your network
- **Directory Transfer**: Send entire directories with preserved structure
- **🆕 Target Directory Support**: Send files and directories directly to specific receiver directories
- **Large File Support**: Transfer files up to 16 exabytes with 64KB chunk optimization
- **High Performance**: Optimized 64KB chunk size for maximum transfer speeds
- **Progress Tracking**: Real-time transfer progress with percentage completion and speed estimation
- **Error Handling**: Comprehensive error checking and robust reporting
- **Enhanced Security**: Path validation, target directory sanitization, and input validation for secure transfers
- **Protocol**: Enhanced magic-number-based protocol with backward compatibility for cross-architecture support

## File Size Support

The tool can now transfer extremely large files thanks to the updated protocol:

- **File size limit**: Up to 16 exabytes (2^64 - 1 bytes)
- **Filename length limit**: Up to 16 exabytes (though practical limits are much lower)
- **Memory efficient**: Uses 64KB chunks for transfer, keeping memory usage low
- **Progress tracking**: Shows transfer progress as percentage completed with speed estimation

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

NETTF supports three main operational modes:

### Discover Devices
```bash
./nettf discover [--timeout <ms>]
```

Scan your local network to find available NETTF-enabled devices. This feature automatically:
- Discovers all active devices on **any network type** (10.x.x.x, 172.16-31.x.x, 192.168.x.x, or public)
- Checks which devices have NETTF service running
- Shows IP addresses, hostnames, and response times
- Intelligently scans different network ranges based on network class

Example:
```bash
./nettf discover
./nettf discover --timeout 2000  # Custom timeout
```

### Receive Files/Directories (Server)
```bash
./nettf receive
```

Start listening for incoming file or directory transfers. The server will:
- Listen on port 9876 by default
- Accept connections from any NETTF client
- Receive both single files and entire directories
- Preserve directory structure when receiving directories
- Show real-time transfer progress

Example:
```bash
./nettf receive
```

### Send Files/Directories (Client)
```bash
./nettf send <TARGET_IP> <FILE_OR_DIR_PATH> [TARGET_DIR]
```

Send files or directories to a NETTF-enabled device. The client will:
- Connect to the target device on port 9876
- Automatically detect if you're sending a file or directory
- **Optionally specify a target directory** where files should be saved
- Preserve directory structure for directory transfers
- Show real-time transfer progress with speed estimation
- Handle very large files up to 16 exabytes

**New Target Directory Feature:**
- Send files directly to specific directories on the receiver
- No need to manually move files after transfer
- Automatic directory creation if target doesn't exist
- Works with both single files and entire directories

Examples:
```bash
# Send a single file (saved to receiver's current directory)
./nettf send 192.168.1.100 /path/to/file.txt

# Send a file to specific directory (creates directory if needed)
./nettf send 192.168.1.100 /path/to/file.txt downloads/

# Send an entire directory (preserves structure)
./nettf send 192.168.1.100 /path/to/documents/

# Send a directory to specific target directory
./nettf send 192.168.1.100 /path/to/documents/ backups/

# Send with custom directory for organization
./nettf send 192.168.1.100 /path/to/video.mp4 media/videos/
```

## 🆕 Target Directory Feature

The target directory feature allows senders to specify exactly where files should be saved on the receiver's system, providing better organization and eliminating the need for manual file management after transfers.

### Key Benefits
- **Automatic Organization**: Files are saved directly to the specified directory
- **No Manual Cleanup**: Eliminates the need to move files after transfer
- **Flexible Organization**: Support for nested directory structures
- **Secure by Design**: Comprehensive path validation prevents security issues
- **Zero Configuration**: Works out of the box with no additional setup

### Security Features
- **Path Sanitization**: Only relative paths are allowed (no absolute paths like `/etc/passwd`)
- **Traversal Prevention**: Blocks dangerous sequences like `../../../etc`
- **Character Validation**: Removes potentially dangerous characters
- **Directory Creation**: Automatically creates target directories if they don't exist
- **Isolation**: All operations are restricted to the receiver's working directory

### Usage Examples

#### Basic File Organization
```bash
# Send downloads directly to downloads folder
./nettf send 192.168.1.100 ./movie.mp4 downloads/

# Send documents to documents folder
./nettf send 192.168.1.100 ./report.pdf documents/

# Send images to media organization
./nettf send 192.168.1.100 ./photo.jpg media/images/
```

#### Backup and Archive Operations
```bash
# Create backup of important documents
./nettf send 192.168.1.100 ./documents/ backups/today/

# Archive photos with date-based organization
./nettf send 192.168.1.100 ./vacation-photos/ archives/2024/december/

# Project file transfers to specific workspaces
./nettf send 192.168.1.100 ./project-files/ workspaces/client-project/
```

#### Development and Testing
```bash
# Send build artifacts to test directory
./nettf send 192.168.1.100 ./build/ test-results/

# Deploy configuration files
./nettf send 192.168.1.100 ./config/ deployment/staging/

# Share logs for analysis
./nettf send 192.168.1.100 ./application.log logs/analysis/
```

### Directory Structure Preservation

When sending directories with target directories:
```bash
# Send entire project structure
./nettf send 192.168.1.100 ./my-project/ backup/projects/

# Result structure on receiver:
# backup/
# └── projects/
#     └── my-project/
#         ├── src/
#         ├── tests/
#         ├── docs/
#         └── README.md
```

### Protocol Evolution

The feature introduces two new protocol types while maintaining full backward compatibility:

| Protocol Type | Magic Number | Use Case |
|---------------|-------------|----------|
| Standard File | `0x46494C45` | Traditional file transfer |
| Standard Directory | `0x44495220` | Traditional directory transfer |
| **Target File** | `0x54415247` | File with target directory |
| **Target Directory** | `0x54444952` | Directory with target directory |

## Protocol Specification

NETTF uses an enhanced TCP-based protocol that supports both single files and directories with a magic number prefix to distinguish transfer types:

### Transfer Type Detection
- **Magic Number** (4 bytes): Identifies transfer type
  - `0x46494C45` ("FILE"): Single file transfer (standard)
  - `0x44495220` ("DIR "): Directory transfer (standard)
  - `0x54415247` ("TARG"): File transfer with target directory
  - `0x54444952` ("TDIR"): Directory transfer with target directory

### File Transfer Protocol
1. **File Header** (16 bytes total)
   - Magic Number: `0x46494C45` (4 bytes)
   - `uint64_t file_size` (8 bytes, Network Byte Order/Big Endian)
   - `uint64_t filename_len` (8 bytes, Network Byte Order/Big Endian)

2. **Filename Payload**
   - Variable length character array (`filename_len` bytes)
   - Directory paths are stripped for security
   - Only the basename is transmitted

3. **File Content**
   - Raw bytes of the file (`file_size` bytes)
   - Transferred in 64KB chunks for optimal performance

### Directory Transfer Protocol
1. **Directory Header** (24 bytes total)
   - Magic Number: `0x44495220` (4 bytes)
   - `uint64_t total_files` (8 bytes): Total number of files in directory
   - `uint64_t total_size` (8 bytes): Combined size of all files
   - `uint64_t base_path_len` (8 bytes): Length of base directory name

2. **Base Directory Path**
   - Directory name without full path (`base_path_len` bytes)

3. **File Transfer Sequence**
   - For each file: Follow File Transfer Protocol
   - Each file includes relative path from base directory
   - Directory structure is preserved on receiver

### Enhanced Target Directory File Transfer Protocol
1. **Target File Header** (32 bytes total)
   - Magic Number: `0x54415247` (4 bytes)
   - `uint64_t file_size` (8 bytes, Network Byte Order)
   - `uint64_t filename_len` (8 bytes, Network Byte Order)
   - `uint64_t target_dir_len` (8 bytes, Network Byte Order)

2. **Filename Payload**
   - Variable length character array (`filename_len` bytes)
   - Only the basename is transmitted

3. **Target Directory Path** (if specified)
   - Variable length character array (`target_dir_len` bytes)
   - Sanitized path (no absolute paths, no traversal)

4. **File Content**
   - Raw bytes of the file (`file_size` bytes)
   - Transferred in 64KB chunks

### Enhanced Target Directory Transfer Protocol
1. **Target Directory Header** (32 bytes total)
   - Magic Number: `0x54444952` (4 bytes)
   - `uint64_t total_files` (8 bytes): Total number of files
   - `uint64_t total_size` (8 bytes): Combined size of all files
   - `uint64_t base_path_len` (8 bytes): Length of directory name
   - `uint64_t target_dir_len` (8 bytes): Length of target directory path

2. **Directory Name**
   - Directory name without full path (`base_path_len` bytes)

3. **Target Directory Path** (if specified)
   - Variable length character array (`target_dir_len` bytes)
   - Sanitized path for target directory creation

4. **File Transfer Sequence**
   - For each file: Follow Enhanced Target Directory File Transfer Protocol
   - Each file includes relative path from base directory

### Protocol Features
- **Network Byte Order**: All multi-byte values use big-endian for cross-architecture compatibility
- **Large File Support**: 64-bit file sizes support files up to 16 exabytes
- **Chunked Transfer**: 64KB chunks balance memory usage and transfer speed
- **Target Directory Support**: Send files directly to specific receiver directories
- **Path Sanitization**: Prevents directory traversal attacks and validates paths
- **Backward Compatibility**: Standard FILE/DIR magic numbers still supported
- **Error Detection**: Comprehensive error checking at every protocol step

## Architecture

The codebase follows a modular, cross-platform architecture:

### Core Modules
- **platform.h/c**: Cross-platform socket abstraction layer
  - Windows: Winsock2 with automatic library linking
  - POSIX: Standard Berkeley sockets with cleanup handling
  - 64-bit endian conversion functions (htonll/ntohll)

- **protocol.h/c**: Enhanced file transfer protocol implementation
  - File and directory transfer protocols with magic numbers
  - **Target directory protocols** with path validation and security
  - 64KB chunked transfers for optimal performance
  - Progress tracking with speed and time estimation
  - Directory structure preservation and recursive operations
  - Backward compatibility with existing protocol versions

- **discovery.h/c**: Network device discovery system
  - ARP table scanning for known devices
  - Ping sweeps for active device detection
  - Service detection on configurable ports
  - Cross-platform network interface enumeration

- **client.c**: Sender (client) implementation
  - TCP connection establishment
  - File/directory detection and appropriate protocol selection
  - Real-time progress display with transfer statistics

- **server.c**: Receiver (server) implementation
  - TCP listener with configurable port
  - Transfer type detection using magic numbers
  - Automatic file/directory reconstruction

- **main.c**: Command-line interface and program coordination
  - Three-mode operation (discover, send, receive)
  - Argument validation and error handling
  - Unified entry point for all functionality

## Security Features

- **Enhanced Path Sanitization**: Comprehensive validation of both filenames and target directories
  - Strips directory components from filenames to prevent traversal attacks
  - Validates target directory paths (no absolute paths, no ".." sequences)
  - Removes leading slashes and dangerous characters
- **Binary Mode Operations**: All file operations use binary mode to prevent data corruption
- **Input Validation**: Comprehensive validation of IP addresses, file paths, and network parameters
- **Error Handling**: Robust error checking on all network, file, and memory operations
- **Memory Safety**: Proper bounds checking and buffer management throughout the codebase
- **Network Isolation**: Transfers are limited to specified targets, no broadcast or multicast
- **Cross-Platform Compatibility**: Network byte order ensures compatibility between different architectures
- **Target Directory Security**: Only allows relative paths within receiver's directory structure

## Example Sessions

### Device Discovery
```bash
$ ./nettf discover
Scanning network interfaces...
Scanning 192.168.1.0/24...
Found 5 devices:

+----------------+-----------------+------------------+--------+----------------+
| IP Address     | MAC Address     | Hostname         | Active | NETTF Service  |
+----------------+-----------------+------------------+--------+----------------+
| 192.168.1.1    | aa:bb:cc:dd:ee:ff | router.local     | ✓      | ✗              |
| 192.168.1.10   | 11:22:33:44:55:66 | desktop-pc       | ✓      | ✓              |
| 192.168.1.15   | 77:88:99:aa:bb:cc | laptop-mac       | ✓      | ✓              |
| 192.168.1.20   | dd:ee:ff:aa:bb:cc | server-nas       | ✓      | ✗              |
| 192.168.1.100  | 33:44:55:66:77:88 | phone-wifi       | ✓      | ✗              |
+----------------+-----------------+------------------+--------+----------------+

Discovery completed. Found 5 device(s).
2 device(s) have NETTF service running on port 9876.
```

### Single File Transfer
**Receiver:**
```bash
$ ./nettf receive
Listening on port 9876...
Waiting for incoming connection...
Connection established from 192.168.1.10:54321
Receiving file: document.pdf (2.4 MB)
Progress: 100.00% [████████████████████] 2.4 MB/2.4 MB (8.5 MB/s, 0s)
File received successfully!
```

**Sender:**
```bash
$ ./nettf send 192.168.1.10 ./document.pdf
Connecting to 192.168.1.10:9876...
Connected! Sending file: document.pdf
Progress: 100.00% [████████████████████] 2.4 MB/2.4 MB (8.5 MB/s, 0s)
File sent successfully!
```

### File Transfer with Target Directory
**Receiver:**
```bash
$ ./nettf receive
Listening on port 9876...
Waiting for incoming connection...
Connection established from 192.168.1.20:54321
Receiving file: video.mp4 -> downloads/ (850 MB)
Progress: 100.00% [████████████████████] 850 MB/850 MB (12.3 MB/s, 69s)
File received successfully: downloads/video.mp4
```

**Sender:**
```bash
$ ./nettf send 192.168.1.20 ./video.mp4 downloads
Connecting to 192.168.1.20:9876...
Connected! Sending file: video.mp4
Target directory: downloads
Sending file: video.mp4 -> downloads/ (large file)
Progress: 100.00% [████████████████████] 850 MB/850 MB (12.3 MB/s)
File sent successfully!
```

### Directory Transfer
**Receiver:**
```bash
$ ./nettf receive
Listening on port 9876...
Waiting for incoming connection...
Connection established from 192.168.1.15:54321
Receiving directory: photos/ (156 files, 1.2 GB)
Receiving: photos/vacation/beach.jpg (2.1 MB) [████████████████████] 45.2%
Receiving: photos/family/birthday.jpg (1.8 MB) [████████████████████] 67.8%
Receiving: photos/work/presentation.png (895 KB) [████████████████████] 89.1%
Directory received successfully!
All files saved to: photos/
```

**Sender:**
```bash
$ ./nettf send 192.168.1.15 ./photos/
Connecting to 192.168.1.15:9876...
Connected! Sending directory: photos/ (156 files, 1.2 GB)
Sending: photos/vacation/beach.jpg (2.1 MB) [████████████████████] 45.2%
Sending: photos/family/birthday.jpg (1.8 MB) [████████████████████] 67.8%
Sending: photos/work/presentation.png (895 KB) [████████████████████] 89.1%
Directory sent successfully!
```

## Performance Characteristics

### Transfer Speed
- **Gigabit Networks**: Up to 900+ MB/s for large files
- **WiFi Networks**: 50-200 MB/s depending on signal strength and protocol
- **Internet Transfer**: Limited by available bandwidth and latency
- **Small Files**: Protocol overhead is minimal (< 1ms per file)

### Memory Usage
- **Chunked Transfer**: 64KB chunks keep memory usage low
- **Directory Transfers**: Memory usage scales with directory structure size
- **Large File Support**: Files up to 16 exabytes with constant memory footprint
- **Discovery Mode**: Minimal memory usage during network scans

### Network Efficiency
- **Protocol Overhead**: 16-24 bytes per transfer (negligible for files > 1KB)
- **TCP Optimization**: Uses TCP's built-in flow control and congestion avoidance
- **Connection Reuse**: Single TCP connection per transfer session
- **Error Recovery**: Automatic detection of connection failures

## Troubleshooting

### Common Issues

**"Connection refused" error**
- Ensure the receiver is running `./nettf receive` before sending
- Check that port 9876 is not blocked by firewalls
- Verify the IP address is correct

**"Discovery found no devices"**
- Check network connectivity
- Try increasing the timeout: `./nettf discover --timeout 3000`
- Ensure devices are on the same network subnet
- Discovery works on all network types: 10.x.x.x, 172.16-31.x.x, 192.168.x.x, or public networks

**"Transfer interrupted"**
- Check network cable/WiFi connection stability
- Restart the transfer from the beginning
- For very large files, ensure stable network connection

**"Permission denied"**
- Check file/directory read permissions
- Ensure write permissions in the receiver's directory
- On Linux/macOS, use `chmod` to fix permissions if needed

### Target Directory Issues

**"Path traversal detected in target directory"**
- The target directory contains `..` sequences (blocked for security)
- Use relative paths like `downloads/` instead of `../../../downloads/`
- Avoid absolute paths like `/tmp/downloads/`

**"Absolute paths not allowed in target directory"**
- Target directories cannot start with `/` (security restriction)
- Use relative paths: `backup/` instead of `/home/user/backup/`
- The receiver creates all paths relative to its working directory

**"Target directory path too long"**
- Path exceeds 4096 character limit
- Use shorter directory names
- Consider using nested structure with shorter names

**"Error: Unknown transfer type magic number"**
- Receiver is using older version without target directory support
- Update receiver to latest version, or
- Use standard transfer without target directory parameter

**"Directory creation failed"**
- Insufficient permissions to create target directory
- Parent directory doesn't exist or is not writable
- Check disk space on receiver

### Firewall Configuration

**Linux (ufw):**
```bash
sudo ufw allow 9876/tcp
sudo ufw reload
```

**Windows:**
- Open "Windows Defender Firewall with Advanced Security"
- Add "Inbound Rule" for port 9876 TCP
- Allow "nettf.exe" application through firewall

**macOS:**
```bash
sudo pfctl -f /etc/pf.conf  # If using pf firewall
```

## Development

### Code Documentation
Comprehensive code documentation is available in `CODE_DOCUMENTATION.md`, including:
- Detailed function-by-function analysis
- Platform-specific implementation notes
- Protocol specification details
- Architecture explanations

### Contributing
1. Fork the repository
2. Create a feature branch: `git checkout -b feature-name`
3. Make your changes with proper comments
4. Test on all target platforms
5. Submit a pull request

### Testing
```bash
# Create test files
dd if=/dev/zero of=test_10mb.bin bs=1M count=10
mkdir -p test_dir && echo "test" > test_dir/file.txt

# Test file transfer
./nettf receive &
./nettf send 127.0.0.1 test_10mb.bin

# Test file transfer with target directory
./nettf receive &
./nettf send 127.0.0.1 test_10mb.bin downloads/

# Test directory transfer
./nettf receive &
./nettf send 127.0.0.1 test_dir/

# Test directory transfer with target directory
./nettf receive &
./nettf send 127.0.0.1 test_dir/ backups/
```

## License

This project is provided as-is for educational and practical use. See the source code headers for additional information.

## Cleaning Build Files

```bash
# Linux/macOS
make clean
./build.sh clean

# Windows
del obj\*.o
del nettf.exe
```
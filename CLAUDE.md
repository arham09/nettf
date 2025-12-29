# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

NETTF (Network Efficient Transfer Tool) is a cross-platform peer-to-peer file transfer CLI tool written in C99. It supports Windows (Winsock2), Linux, and macOS (POSIX sockets) for LAN-based file transfers with network device discovery.

## Build Commands

### Linux/macOS (Primary)
```bash
./build.sh              # Full build (clean, build, test)
./build.sh build        # Build only
./build.sh clean        # Clean artifacts
./build.sh test         # Run functional tests
./build.sh unit         # Run unit tests (Unity framework)
./build.sh install      # Install to /usr/local/bin
./build.sh uninstall    # Remove from system
make                    # Traditional Makefile build
make clean              # Clean via Makefile
cd test && make test    # Run unit tests directly
```

### Windows
```cmd
build.bat               # Build with MinGW/GCC
```

### Build Verification
```bash
./nettf                 # Should show usage information
```

## Code Architecture

### Source Files Structure
```
src/
├── platform.h/c    # Cross-platform socket abstraction (Winsock2 vs POSIX)
├── protocol.h/c    # File transfer protocol with magic numbers
├── adaptive.h/c    # Adaptive chunk sizing (8KB-2MB based on network speed)
├── logging.h/c     # Comprehensive logging system (./nettf.log)
├── signals.h/c     # POSIX signal handling (Ctrl+C graceful shutdown)
├── discovery.h/c   # Network device discovery system
├── client.c        # Sender implementation
├── server.c        # Receiver implementation
└── main.c          # CLI entry point (discover/send/receive modes)

test/
├── unity.h/c       # Unity test framework
├── run_tests.c     # Main test runner
├── test_logging.c  # Logging module tests
└── test_adaptive.c # Adaptive chunk sizing tests
```

### Core Modules

**Platform Layer (`src/platform.h/c`)**
- Abstracts Windows (Winsock2) vs POSIX (Berkeley sockets) differences
- Provides `SOCKET_T`, `INVALID_SOCKET_T` type aliases
- Implements 64-bit endian conversion: `htonll()`, `ntohll()`
- Network init/cleanup: `net_init()`, `net_cleanup()`, `close_socket()`

**Protocol Layer (`src/protocol.h/c`)**
- Magic-number-based protocol for transfer type detection
- Four transfer types (magic numbers are 4-byte hex values):
  - `FILE_MAGIC` (0x46494C45): Standard file transfer
  - `DIR_MAGIC` (0x44495220): Standard directory transfer
  - `TARGET_FILE_MAGIC` (0x54415247): File with target directory
  - `TARGET_DIR_MAGIC` (0x54444952): Directory with target directory
- **Adaptive chunk sizing** (8KB-2MB) for optimal throughput
- 64-bit file size support (up to 16 exabytes)
- Functions: `send_file_protocol()`, `recv_file_protocol()`, `send_directory_protocol()`, `recv_directory_protocol()`, plus target directory variants
- Integrated with adaptive sizing, logging, and signal handling

**Adaptive Module (`src/adaptive.h/c`)**
- Dynamic chunk size adjustment based on network speed
- Range: 8KB to 2MB with initial size of 64KB
- Rolling average of 5 speed samples
- Adjustment interval: 2 seconds
- Functions: `adaptive_init()`, `adaptive_get_chunk_size()`, `adaptive_update()`, `adaptive_reset()`
- Speed tiers: <1 MB/s → 8KB, <10 MB/s → 64KB, <50 MB/s → 256KB, <100 MB/s → 1MB, ≥100 MB/s → 2MB

**Logging Module (`src/logging.h/c`)**
- File-based append-only logging to `./nettf.log`
- Log levels: DEBUG, INFO, WARN, ERROR
- Timestamp format: `[YYYY-MM-DD HH:MM:SS] [LEVEL] message`
- Safe no-op if not initialized
- Macros: `LOG_DEBUG()`, `LOG_INFO()`, `LOG_WARN()`, `LOG_ERROR()`
- Functions: `log_init()`, `log_cleanup()`, `log_message()`

**Signals Module (`src/signals.h/c`)**
- POSIX signal handling for graceful shutdown (Linux/macOS only)
- First Ctrl+C: prompt user, continue transfer
- Second Ctrl+C: force exit with cleanup
- Uses `sig_atomic_t` for thread-safe counter
- Functions: `signals_init()`, `signals_cleanup()`, `signals_should_shutdown()`, `signals_acknowledge_shutdown()`
- Note: Windows uses stub implementations (no Ctrl+C handling)

**Discovery Module (`src/discovery.h/c`)**
- ARP table scanning and ping sweeps for device discovery
- Service detection on port 9876 (default NETTF port)
- Returns `NetworkDevice` structs with IP, MAC, hostname, and service status

**Client (`src/client.c`)**
- TCP connection to target IP:port
- Protocol selection based on file/directory and target directory args
- Progress display with speed/time estimation
- Integrated logging and signal handling

**Server (`src/server.c`)**
- TCP listener with `SO_REUSEADDR` for quick restarts
- Transfer type detection via magic number
- Automatic file/directory reconstruction with target directory support
- Signal-aware accept loop for graceful shutdown

**Main (`src/main.c`)**
- Three modes: `discover`, `send <IP> <PATH> [TARGET_DIR]`, `receive [PORT]`
- Default port: 9876
- Initializes logging and signals on startup
- Cleanup on exit

## Key Design Patterns

**Platform Abstraction via Conditional Compilation**
- `#ifdef _WIN32` for Windows-specific code
- Type aliases normalize socket types across platforms
- Example: `typedef int SOCKET_T` on POSIX vs `#define SOCKET_T SOCKET` on Windows

**Protocol Versioning with Magic Numbers**
- First 4 bytes of any transfer identify the protocol type
- Enables backward compatibility - old receivers gracefully reject unknown protocols
- `detect_transfer_type()` reads magic number and returns 0-3 (or -1 for unknown)

**Chunked Data Transfer**
- Files transferred in adaptive chunks (8KB-2MB) based on network speed
- Dynamic chunk size adjusts every 2 seconds using rolling average of 5 speed samples
- Speed tiers: <1 MB/s → 8KB, <10 MB/s → 64KB, <50 MB/s → 256KB, <100 MB/s → 1MB, ≥100 MB/s → 2MB
- Memory footprint up to 2MB for transfers on fast networks
- `send_all()` / `recv_all()` handle partial sends/receives

**Security-First Validation**
- Path sanitization: strip directory components from filenames
- Target directory validation: blocks `..` sequences, absolute paths
- All operations restricted to receiver's working directory
- See `validate_target_directory()` in protocol.c

## Important Constants

```c
// Default port
#define DEFAULT_NETTF_PORT 9876

// Adaptive chunk sizing constants
#define MIN_CHUNK_SIZE    (8 * 1024)       // 8 KB
#define MAX_CHUNK_SIZE    (2 * 1024 * 1024)  // 2 MB
#define INITIAL_CHUNK_SIZE (64 * 1024)     // 64 KB
#define ADJUSTMENT_INTERVAL 2              // Seconds between adjustments
#define SPEED_SAMPLES      5               // Rolling window size

// Protocol constants
#define HEADER_SIZE 16                // File header (file_size + filename_len)
#define DIR_HEADER_SIZE 24            // Directory header (total_files + total_size + base_path_len)
#define MAGIC_SIZE 4                  // Magic number size

// Logging
#define LOG_FILE_PATH "./nettf.log"   // Default log file location
```

## Protocol Details

All multi-byte integers use **network byte order** (big-endian) for cross-architecture compatibility:
- `htonll()` / `ntohll()` for 64-bit values
- `htonl()` / `ntohl()` for 32-bit values

**File Header (16 bytes):** `file_size` (8) + `filename_len` (8)
**Directory Header (24 bytes):** `total_files` (8) + `total_size` (8) + `base_path_len` (8)
**Target File Header (32 bytes):** File header + `target_dir_len` (8)
**Target Directory Header (40 bytes):** Directory header + `target_dir_len` (8)

## Common Development Tasks

**Adding a new protocol type:**
1. Define new magic number in `protocol.h`
2. Add header struct (if needed)
3. Implement `send_*_protocol()` and `recv_*_protocol()` in `protocol.c`
4. Update `detect_transfer_type()` switch statement
5. Wire up in `client.c` and `server.c`

**Writing unit tests:**
1. Create test function in `test/test_*.c` following Unity pattern
2. Use Unity assertions: `TEST_ASSERT_EQUAL`, `TEST_ASSERT_TRUE`, etc.
3. Add test runner function: `int test_*_runner(void)`
4. Register runner in `test/run_tests.c` main function
5. Run tests: `./build.sh unit` or `cd test && make test`

**Adding logging to new code:**
1. Call `log_init()` in `main.c` before other operations
2. Use macros: `LOG_INFO()`, `LOG_ERROR()`, `LOG_WARN()`, `LOG_DEBUG()`
3. Call `log_cleanup()` before program exit
4. Log file is written to `./nettf.log` in append mode

**Adding signal handling:**
1. Call `signals_init()` in `main.c`
2. Check `signals_should_shutdown()` in loops (returns 0=continue, 1=prompt, 2=force exit)
3. On first signal (1): print prompt, call `signals_acknowledge_shutdown()`, continue work
4. On second signal (2): clean up and exit
5. Call `signals_cleanup()` before exit

**Cross-platform considerations:**
- Always use `SOCKET_T`, `INVALID_SOCKET_T` types
- Use `close_socket()` instead of `close()` or `closesocket()`
- Call `net_init()` before socket operations, `net_cleanup()` after
- Address length type: `int` on Windows, `socklen_t` on POSIX
- Signal handling only works on POSIX (Linux/macOS), Windows uses stubs

**Testing file transfers locally:**
```bash
# Terminal 1: Start receiver
./nettf receive

# Terminal 2: Send file
./nettf send 127.0.0.1 test_file.txt

# Run unit tests
./build.sh unit

# Check logs
cat ./nettf.log
```

**When making new changes:**
- Update CODE_DOCUMENTATION.md with detailed implementation notes
- Update CLAUDE.md with architecture changes
- Add unit tests for new functionality
- Run tests: `./build.sh` (runs all tests)
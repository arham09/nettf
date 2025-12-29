#!/bin/bash

# Build Script for NETTF File Transfer Tool
# Compatible with Linux distributions (Ubuntu, Debian, CentOS, RHEL, etc.)

# Color codes for output formatting
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Project configuration
PROJECT_NAME="nettf"
SRCDIR="src"
OBJDIR="obj"
TARGET_NAME="nettf"

# Compiler settings
CC="${CC:-gcc}"
CFLAGS="-Wall -Wextra -std=c99 -O2"
LDFLAGS=""

# Detect system information
detect_system() {
    echo -e "${BLUE}🔍 Detecting system information...${NC}"

    # Detect Linux distribution
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$NAME
        VERSION=$VERSION_ID
    elif type lsb_release >/dev/null 2>&1; then
        DISTRO=$(lsb_release -si)
        VERSION=$(lsb_release -sr)
    else
        DISTRO="Unknown"
        VERSION="Unknown"
    fi

    # Detect architecture
    ARCH=$(uname -m)

    # Check compiler availability
    if command -v $CC >/dev/null 2>&1; then
        COMPILER_VERSION=$($CC --version | head -n1)
    else
        COMPILER_VERSION="Not found"
    fi

    echo -e "${CYAN}  Distribution: $DISTRO $VERSION${NC}"
    echo -e "${CYAN}  Architecture: $ARCH${NC}"
    echo -e "${CYAN}  Compiler: $COMPILER_VERSION${NC}"
    echo ""
}

# Check dependencies
check_dependencies() {
    echo -e "${BLUE}📦 Checking dependencies...${NC}"

    local missing_deps=()

    # Check for GCC compiler
    if ! command -v gcc >/dev/null 2>&1; then
        missing_deps+=("gcc")
    fi

    # Check for make
    if ! command -v make >/dev/null 2>&1; then
        missing_deps+=("make")
    fi

    # Check for development headers
    if [ ! -f /usr/include/sys/socket.h ]; then
        missing_deps+=("libc6-dev")
    fi

    if [ ${#missing_deps[@]} -eq 0 ]; then
        echo -e "${GREEN}✅ All dependencies are satisfied${NC}"
        return 0
    else
        echo -e "${RED}❌ Missing dependencies: ${missing_deps[*]}${NC}"
        echo -e "${YELLOW}Install them using:${NC}"

        if command -v apt-get >/dev/null 2>&1; then
            echo -e "${YELLOW}  sudo apt-get update && sudo apt-get install build-essential${NC}"
        elif command -v yum >/dev/null 2>&1; then
            echo -e "${YELLOW}  sudo yum groupinstall 'Development Tools'${NC}"
        elif command -v dnf >/dev/null 2>&1; then
            echo -e "${YELLOW}  sudo dnf groupinstall 'Development Tools'${NC}"
        elif command -v pacman >/dev/null 2>&1; then
            echo -e "${YELLOW}  sudo pacman -S base-devel${NC}"
        else
            echo -e "${YELLOW}  Please install GCC compiler and make manually${NC}"
        fi
        return 1
    fi
}

# Clean build artifacts
clean_build() {
    echo -e "${BLUE}🧹 Cleaning build artifacts...${NC}"

    if [ -d "$OBJDIR" ]; then
        rm -rf "$OBJDIR"
        echo -e "${GREEN}  Removed object directory${NC}"
    fi

    if [ -f "$TARGET_NAME" ]; then
        rm -f "$TARGET_NAME"
        echo -e "${GREEN}  Removed executable${NC}"
    fi

    # Remove common build artifacts
    rm -f *.o *.a *.so *.out *.log

    echo -e "${GREEN}✅ Clean completed${NC}"
}

# Build the project
build_project() {
    echo -e "${BLUE}🔨 Building $PROJECT_NAME...${NC}"

    # Create object directory
    mkdir -p "$OBJDIR"

    # Find all source files
    SOURCES=$(find "$SRCDIR" -name "*.c" 2>/dev/null)
    if [ -z "$SOURCES" ]; then
        echo -e "${RED}❌ No source files found in $SRCDIR${NC}"
        return 1
    fi

    # Compile each source file
    for source in $SOURCES; do
        source_name=$(basename "$source" .c)
        object_file="$OBJDIR/${source_name}.o"

        echo -e "${CYAN}  Compiling $source...${NC}"

        if ! $CC $CFLAGS -c "$source" -o "$object_file" 2>"${source_name}.log"; then
            echo -e "${RED}❌ Failed to compile $source${NC}"
            if [ -f "${source_name}.log" ]; then
                echo -e "${RED}Compiler errors:${NC}"
                cat "${source_name}.log"
            fi
            return 1
        fi

        # Remove compilation log on success
        rm -f "${source_name}.log"
    done

    # Link the executable
    OBJECT_FILES=$(find "$OBJDIR" -name "*.o" | tr '\n' ' ')
    echo -e "${CYAN}  Linking $TARGET_NAME...${NC}"

    if ! $CC $OBJECT_FILES $LDFLAGS -o "$TARGET_NAME" 2>link.log; then
        echo -e "${RED}❌ Failed to link $TARGET_NAME${NC}"
        if [ -f "link.log" ]; then
            echo -e "${RED}Linker errors:${NC}"
            cat "link.log"
        fi
        return 1
    fi

    # Remove link log on success
    rm -f link.log

    echo -e "${GREEN}✅ Build completed successfully${NC}"
    return 0
}

# Run tests
run_tests() {
    echo -e "${BLUE}🧪 Running functional tests...${NC}"

    if [ ! -f "$TARGET_NAME" ]; then
        echo -e "${RED}❌ Executable not found. Build first.${NC}"
        return 1
    fi

    # Test basic functionality
    echo -e "${CYAN}  Testing help output...${NC}"
    if ./$TARGET_NAME >/dev/null 2>&1; then
        echo -e "${RED}❌ Should have exited with error for no arguments${NC}"
        return 1
    fi

    echo -e "${CYAN}  Testing argument parsing...${NC}"
    if ./$TARGET_NAME receive >/dev/null 2>&1; then
        echo -e "${RED}❌ Should have exited with error for missing port${NC}"
        return 1
    fi

    if ./$TARGET_NAME send >/dev/null 2>&1; then
        echo -e "${RED}❌ Should have exited with error for missing arguments${NC}"
        return 1
    fi

    if ./$TARGET_NAME invalid-command 2>&1 | grep -q "Invalid command"; then
        echo -e "${GREEN}  ✅ Invalid command handling works${NC}"
    else
        echo -e "${RED}❌ Invalid command handling failed${NC}"
        return 1
    fi

    echo -e "${GREEN}✅ All functional tests passed${NC}"
    return 0
}

# Run unit tests
run_unit_tests() {
    echo -e "${BLUE}🧪 Running unit tests...${NC}"

    # Check if test directory exists
    if [ ! -d "test" ]; then
        echo -e "${YELLOW}⚠️  No test directory found. Skipping unit tests.${NC}"
        return 0
    fi

    # Check if test Makefile exists
    if [ ! -f "test/Makefile" ]; then
        echo -e "${YELLOW}⚠️  No test Makefile found. Skipping unit tests.${NC}"
        return 0
    fi

    # Build and run tests
    cd test
    if make clean 2>&1 | grep -q error; then
        echo -e "${RED}❌ Unit test clean failed${NC}"
        cd ..
        return 1
    fi

    if make build 2>&1 | grep -q error; then
        echo -e "${RED}❌ Unit test build failed${NC}"
        cd ..
        return 1
    fi

    if ./run_tests; then
        echo -e "${GREEN}✅ Unit tests passed${NC}"
        cd ..
        return 0
    else
        echo -e "${RED}❌ Unit tests failed${NC}"
        cd ..
        return 1
    fi
}

# Install the executable
install_executable() {
    echo -e "${BLUE}📦 Installing $TARGET_NAME...${NC}"

    if [ ! -f "$TARGET_NAME" ]; then
        echo -e "${RED}❌ Executable not found. Build first.${NC}"
        return 1
    fi

    # Check installation directory
    INSTALL_DIR="/usr/local/bin"
    if [ ! -w "$INSTALL_DIR" ]; then
        echo -e "${YELLOW}⚠️  Need sudo privileges to install to $INSTALL_DIR${NC}"
        if sudo cp "$TARGET_NAME" "$INSTALL_DIR/"; then
            echo -e "${GREEN}✅ Installed to $INSTALL_DIR${NC}"
        else
            echo -e "${RED}❌ Installation failed${NC}"
            return 1
        fi
    else
        if cp "$TARGET_NAME" "$INSTALL_DIR/"; then
            echo -e "${GREEN}✅ Installed to $INSTALL_DIR${NC}"
        else
            echo -e "${RED}❌ Installation failed${NC}"
            return 1
        fi
    fi
}

# Uninstall the executable
uninstall_executable() {
    echo -e "${BLUE}🗑️  Uninstalling $TARGET_NAME...${NC}"

    INSTALL_DIR="/usr/local/bin"
    if [ -f "$INSTALL_DIR/$TARGET_NAME" ]; then
        if [ ! -w "$INSTALL_DIR" ]; then
            echo -e "${YELLOW}⚠️  Need sudo privileges to uninstall from $INSTALL_DIR${NC}"
            if sudo rm -f "$INSTALL_DIR/$TARGET_NAME"; then
                echo -e "${GREEN}✅ Uninstalled from $INSTALL_DIR${NC}"
            else
                echo -e "${RED}❌ Uninstallation failed${NC}"
                return 1
            fi
        else
            if rm -f "$INSTALL_DIR/$TARGET_NAME"; then
                echo -e "${GREEN}✅ Uninstalled from $INSTALL_DIR${NC}"
            else
                echo -e "${RED}❌ Uninstallation failed${NC}"
                return 1
            fi
        fi
    else
        echo -e "${YELLOW}⚠️  $TARGET_NAME not found in $INSTALL_DIR${NC}"
    fi
}

# Display build information
build_info() {
    echo -e "${BLUE}ℹ️  Build Information:${NC}"
    echo -e "${CYAN}  Project: $PROJECT_NAME${NC}"
    echo -e "${CYAN}  Compiler: $CC${NC}"
    echo -e "${CYAN}  Flags: $CFLAGS${NC}"

    if [ -f "$TARGET_NAME" ]; then
        SIZE=$(stat -f%z "$TARGET_NAME" 2>/dev/null || stat -c%s "$TARGET_NAME" 2>/dev/null)
        echo -e "${CYAN}  Executable Size: $((SIZE / 1024)) KB${NC}"
        echo -e "${CYAN}  Location: $(pwd)/$TARGET_NAME${NC}"
    else
        echo -e "${YELLOW}  Executable not built yet${NC}"
    fi

    # Show version info if available
    if command -v ldd >/dev/null 2>&1 && [ -f "$TARGET_NAME" ]; then
        echo -e "${CYAN}  Linked libraries:${NC}"
        ldd "$TARGET_NAME" 2>/dev/null | head -3
    fi
}

# Show usage information
show_usage() {
    echo -e "${CYAN}NETTF File Transfer Tool - Build Script${NC}"
    echo ""
    echo -e "${YELLOW}Usage: $0 [COMMAND]${NC}"
    echo ""
    echo -e "${GREEN}Commands:${NC}"
    echo -e "  ${CYAN}all${NC}         Clean, build, and test (default)"
    echo -e "  ${CYAN}build${NC}       Build the project"
    echo -e "  ${CYAN}clean${NC}       Clean build artifacts"
    echo -e "  ${CYAN}test${NC}        Run functional tests"
    echo -e "  ${CYAN}unit${NC}        Run unit tests"
    echo -e "  ${CYAN}install${NC}     Install to system path"
    echo -e "  ${CYAN}uninstall${NC}   Remove from system path"
    echo -e "  ${CYAN}info${NC}        Show build information"
    echo -e "  ${CYAN}check${NC}       Check dependencies"
    echo -e "  ${CYAN}help${NC}        Show this help"
    echo ""
    echo -e "${YELLOW}Examples:${NC}"
    echo -e "  $0              # Full build process"
    echo -e "  $0 build        # Build only"
    echo -e "  $0 clean build  # Clean then build"
    echo -e "  $0 unit         # Run unit tests"
    echo -e "  $0 install      # Install to system"
    echo ""
}

# Main script logic
main() {
    # Set script directory
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    cd "$SCRIPT_DIR"

    # Parse command line arguments
    COMMAND="${1:-all}"

    case "$COMMAND" in
        "all")
            detect_system
            check_dependencies || exit 1
            clean_build
            build_project || exit 1
            run_tests || exit 1
            run_unit_tests || exit 1
            echo -e "${GREEN}🎉 Full build process completed successfully!${NC}"
            ;;
        "build")
            detect_system
            build_project || exit 1
            ;;
        "clean")
            clean_build
            ;;
        "test")
            run_tests || exit 1
            ;;
        "unit")
            run_unit_tests || exit 1
            ;;
        "install")
            install_executable || exit 1
            ;;
        "uninstall")
            uninstall_executable || exit 1
            ;;
        "info")
            build_info
            ;;
        "check")
            detect_system
            check_dependencies
            ;;
        "help"|"-h"|"--help")
            show_usage
            ;;
        *)
            echo -e "${RED}❌ Unknown command: $COMMAND${NC}"
            echo ""
            show_usage
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"
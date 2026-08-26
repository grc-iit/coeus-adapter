#!/bin/bash
# Quick build script for coeus-adapter with Chimaera Runtime and CTE support

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
INSTALL_PREFIX="/usr/local"
ENABLE_META=OFF
ENABLE_DEBUG=OFF
NUM_JOBS=$(nproc)

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        --meta)
            ENABLE_META=ON
            shift
            ;;
        --debug)
            ENABLE_DEBUG=ON
            shift
            ;;
        --jobs)
            NUM_JOBS="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --build-type TYPE    Build type (Debug/Release) [default: Release]"
            echo "  --prefix PATH        Installation prefix [default: /usr/local]"
            echo "  --meta               Enable metadata features"
            echo "  --debug              Enable debug mode"
            echo "  --jobs N             Number of parallel jobs [default: nproc]"
            echo "  --help               Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}=== Coeus-Adapter Build Script ===${NC}"
echo "Build Type: $BUILD_TYPE"
echo "Install Prefix: $INSTALL_PREFIX"
echo "Metadata: $ENABLE_META"
echo "Debug: $ENABLE_DEBUG"
echo "Jobs: $NUM_JOBS"
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: CMakeLists.txt not found. Please run this script from the coeus-adapter root directory.${NC}"
    exit 1
fi

# Check for required dependencies
echo -e "${YELLOW}Checking dependencies...${NC}"

check_dependency() {
    if pkg-config --exists "$1" 2>/dev/null; then
        echo -e "${GREEN}✓${NC} $1 found"
        return 0
    else
        echo -e "${RED}✗${NC} $1 not found"
        return 1
    fi
}

MISSING_DEPS=0

# Check CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}✗${NC} CMake not found"
    MISSING_DEPS=1
else
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    echo -e "${GREEN}✓${NC} CMake $CMAKE_VERSION found"
fi

# Check for Chimaera
if [ -z "$CMAKE_PREFIX_PATH" ] && [ ! -d "/usr/local/lib/cmake/chimaera-core" ]; then
    echo -e "${YELLOW}⚠${NC} Chimaera not found in standard locations. Set CMAKE_PREFIX_PATH if installed elsewhere."
fi

# Check for CTE
if [ -z "$CMAKE_PREFIX_PATH" ] && [ ! -d "/usr/local/lib/cmake/wrp_cte_core" ]; then
    echo -e "${YELLOW}⚠${NC} CTE not found in standard locations. Set CMAKE_PREFIX_PATH if installed elsewhere."
fi

if [ $MISSING_DEPS -eq 1 ]; then
    echo -e "${RED}Missing required dependencies. Please install them before building.${NC}"
    exit 1
fi

# Create build directory
echo -e "${YELLOW}Creating build directory...${NC}"
mkdir -p build
cd build

# Configure CMake
echo -e "${YELLOW}Configuring CMake...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -Dmeta_enabled="$ENABLE_META" \
    -Ddebug_mode="$ENABLE_DEBUG"

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    exit 1
fi

# Build
echo -e "${YELLOW}Building...${NC}"
cmake --build . --parallel "$NUM_JOBS"

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo -e "${GREEN}=== Build Complete ===${NC}"
echo ""
echo "Libraries built in: $(pwd)/bin"
echo ""
echo "To install, run:"
echo "  sudo cmake --install . --prefix $INSTALL_PREFIX"
echo ""
echo "To run tests, run:"
echo "  ctest -V"
echo ""


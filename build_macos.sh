#!/bin/bash
#
# build_macos.sh - Build CACHEKIT on macOS with Open Watcom
#
# Part of the Abacus FPGA Project
# Last Updated: 2026-01-23 17:45:00 CET
#
# Usage:
#   ./build_macos.sh          Build all (HAL + CLI tools)
#   ./build_macos.sh hal      Build HAL-based CACHEKIT only
#   ./build_macos.sh cli      Build CLI tools only
#   ./build_macos.sh clean    Remove build artifacts
#

# Configuration
WATCOM="${WATCOM:-/Users/johnfabienke/Development/macos-open-watcom/open-watcom-v2/rel}"
CC="$WATCOM/armo64/wcc"
LINK="$WATCOM/armo64/wlink"
export INCLUDE="$WATCOM/h"
export LIB="$WATCOM/lib286/dos:$WATCOM/lib286"

# Compiler flags
# Use medium memory model (-mm) for larger code (64K data, multiple 64K code segments)
CFLAGS="-0 -mm -s -ox -w4 -zq"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check for Open Watcom installation
if [ ! -x "$CC" ]; then
    echo -e "${RED}Error: Open Watcom not found at $WATCOM${NC}"
    echo "Set WATCOM environment variable or edit this script"
    exit 1
fi

# Compile a single C file to OBJ
compile() {
    local src=$1
    # Get base name without extension
    local base=$(basename "$src" .C)
    base=$(basename "$base" .c)
    local obj="${base}.OBJ"

    echo -n "  Compiling $src... "
    if $CC $CFLAGS -fo="$obj" "$src" 2>/dev/null; then
        echo -e "${GREEN}OK${NC}"
        return 0
    else
        echo -e "${RED}FAILED${NC}"
        $CC $CFLAGS -fo="$obj" "$src"
        return 1
    fi
}

# Link object files to EXE
link_exe() {
    local name=$1
    shift
    local objs="$@"

    echo -n "  Linking $name.EXE... "
    # Use explicit library path since wlink on macOS needs it
    if $LINK option quiet option stack=8192 \
        libpath "$WATCOM/lib286/dos" libpath "$WATCOM/lib286" \
        library clibm.lib library mathm.lib \
        format dos name "$name.EXE" file { $objs } 2>/dev/null; then
        echo -e "${GREEN}OK${NC}"
        return 0
    else
        echo -e "${RED}FAILED${NC}"
        $LINK option quiet option stack=8192 \
            libpath "$WATCOM/lib286/dos" libpath "$WATCOM/lib286" \
            library clibm.lib library mathm.lib \
            format dos name "$name.EXE" file { $objs }
        return 1
    fi
}

# Build CLI tools
build_cli() {
    echo -e "${YELLOW}Building CLI tools...${NC}"

    local success=0
    local failed=0

    # CHIPSET - 16-bit, works on any x86
    if [ -f "CHIPSET.C" ]; then
        if compile "CHIPSET.C"; then
            if link_exe "CHIPSET" "CHIPSET.OBJ"; then
                ((success++))
            else
                ((failed++))
            fi
        else
            ((failed++))
        fi
    fi

    # NCCONFIG - 16-bit, works on any x86
    if [ -f "NCCONFIG.C" ]; then
        if compile "NCCONFIG.C"; then
            if link_exe "NCCONFIG" "NCCONFIG.OBJ"; then
                ((success++))
            else
                ((failed++))
            fi
        else
            ((failed++))
        fi
    fi

    # CACHETST - SKIPPED: requires 486+ instructions (WBINVD) in inline assembly
    # This tool has mixed 16/32-bit assembly that needs source changes for macOS build
    # The cache flush testing is also available in the main CACHEKIT TUI
    if [ -f "CACHETST.C" ]; then
        echo -e "  ${YELLOW}Skipping CACHETST.C (mixed 16/32-bit asm needs DOS-hosted build)${NC}"
    fi

    echo ""
    echo -e "CLI tools: ${GREEN}$success succeeded${NC}, ${RED}$failed failed${NC}"
}

# Build HAL-based CACHEKIT
build_hal() {
    echo -e "${YELLOW}Building HAL-based CACHEKIT...${NC}"

    local hal_sources="CACHEKIT CK_HAL CK_IO CK_XMS CK_ENUM CK_BCFG CK_INTEL CK_SIS CK_OPTI CK_VIA CK_ALI CK_LEGAC CK_OTHER CK_VIDEO CK_UI"
    local objs=""
    local all_ok=1

    for src in $hal_sources; do
        if [ -f "${src}.C" ]; then
            if compile "${src}.C"; then
                objs="$objs ${src}.OBJ"
            else
                all_ok=0
            fi
        fi
    done

    if [ $all_ok -eq 1 ] && [ -n "$objs" ]; then
        link_exe "CACHEKIT" $objs
    else
        echo -e "${RED}HAL build failed due to compilation errors${NC}"
        return 1
    fi
}

# Clean build artifacts
clean() {
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    rm -f *.OBJ *.EXE *.MAP *.ERR 2>/dev/null
    echo -e "${GREEN}Clean complete${NC}"
}

# Main
case "${1:-all}" in
    hal)
        build_hal
        ;;
    cli)
        build_cli
        ;;
    clean)
        clean
        ;;
    all)
        build_hal
        echo ""
        build_cli
        ;;
    *)
        echo "Usage: $0 [hal|cli|clean|all]"
        exit 1
        ;;
esac

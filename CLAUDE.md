# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Last Updated: 2026-01-06 15:26:51 EST

## Project Overview

CACHEKIT is a DOS-based utility suite for 386/486/Pentium ISA systems that provides chipset detection, cache management, and non-cacheable (NC) region configuration. Part of the Abacus FPGA Project. Supports 62+ chipsets from 286-era through PCI systems.

## Build Commands

Requires Open Watcom C/C++ with `WATCOM` environment variable set.

```bash
wmake              # Build CACHEKIT.EXE (HAL-based v3.0, default)
wmake legacy       # Build single-file v2.x version
wmake cli          # Build standalone CLI tools (CHIPSET.EXE, NCCONFIG.EXE, CACHETST.EXE)
wmake clean        # Remove build artifacts
wmake rebuild      # Clean and rebuild
```

Compiler flags: `-0` (8086 compatible), `-ms` (small memory model), `-ox` (max optimization), `-w4` (strict warnings)

## Architecture

HAL-based modular design with per-chipset operations structure (`chipset_ops_t`):

```
CACHEKIT.C (Main TUI: screens F1-F7, state machine, main loop)
    │
    ├── CK_UI.C/H      (dialogs, menus, tables)
    ├── CK_VIDEO.C/H   (VGA text mode B800h, keyboard)
    ├── CK_HAL.C/H     (detection registry, fallback ops)
    └── CK_IO.C        (PCI config space, legacy port I/O)
            │
            └── Chipset Implementations:
                ├── CK_INTEL.C  (430FX/HX/VX/TX/MX, EISA PCIsets)
                ├── CK_SIS.C    (460, Rabbit, 496, 5598, 530, 5591)
                ├── CK_OPTI.C   (391, 381, Viper, EISA variants)
                ├── CK_VIA.C    (VT82C495, VP1, VP3, MVP3)
                ├── CK_ALI.C    (Aladdin IV/V)
                ├── CK_LEGACY.C (NEAT, Headland, VLSI, 286/386-era)
                └── CK_OTHER.C  (UMC, Eteq, Faraday, MIC, Contaq)

CK_ENUM.C/H  (PCI/MCA/EISA/ISA PnP enumeration with embedded ID databases)
```

## Key Abstractions

### chipset_ops_t (CK_HAL.H)
Every chipset implements this structure with function pointers:
- `probe()` - Detect chipset presence (PCI ID or port-based)
- `cache_get/set/flush()` - Cache control
- `nc_read/write/clear()` - NC region management
- `shadow_get/set()` - Shadow RAM control
- `a20_get/set()` - A20 gate control
- `reg_read/write()` - Raw register access

### Detection Flow
MCA Bus → PCI Chipsets → EISA Chipsets → Legacy ISA → `ops_unknown` fallback

### I/O Access Patterns
- **PCI**: `pci_read_config_*()` via 0xCF8/0xCFC mechanism
- **Legacy ports**: Standard 0x22/0x23, OPTi quirk 0x22/0x24, VIA 0xA8/0xA9

## Adding a New Chipset

1. Create implementation in `CK_VENDOR.C` (or add to existing file):
   - Implement `probe()` function
   - Implement cache/NC/shadow/A20 ops as applicable
   - Create `const chipset_ops_t ops_my_chipset` struct

2. Register in HAL:
   - Add `extern` in `CK_HAL.H`
   - Add pointer to `g_chipset_registry[]` in `CK_HAL.C`

3. Update MAKEFILE:
   - Add `.OBJ` to `HAL_OBJS`
   - Add dependency line

## Critical Domain Knowledge

### Write-Back vs Write-Through
- **WB Flush Order**: FLUSH first, THEN DISABLE (data loss if reversed!)
- **WT Flush Order**: DISABLE first, then FLUSH

### NC Region Encodings (chipset-specific)
- **OPTi**: Base (A23:A16 in 64KB units) + size code nibble
- **SiS**: 16-bit packed (bits 15:13=size, bits 12:0=base)
- **Intel/VIA**: Separate PAM registers per region

### Port Conflicts
Ports 0x22/0x23/0x24 may conflict with LPT2 or sound cards. Detection checks for floating bus conditions.

## Constraints

- 16-bit real mode DOS (8086+ compatible binary)
- No malloc, limited stack, segment:offset memory model
- Direct port I/O and inline assembly via `#pragma aux`
- ~65KB binary size (HAL-based)

## Testing

- **Emulators**: PCem, 86Box (support 286 through Pentium 4 era)
- **Key chipsets to verify**: Intel 430FX (PCI), OPTi 82C391 (port quirk), SiS 496 (encoding differences)

# CACHEKIT v3.1

**DOS Chipset Detection, Cache Management & Bus Configuration Utility**

Part of the Abacus FPGA Project | For 386/486/Pentium ISA Systems

*Last Updated: 2026-01-23 18:17:36 CET*

## Download

| File | Size | Description |
|------|------|-------------|
| [**CACHEKIT.EXE**](https://github.com/jfabienke/cache-kit/releases/latest/download/CACHEKIT.EXE) | 127 KB | Main TUI utility (requires 386+) |
| [CHIPSET.EXE](https://github.com/jfabienke/cache-kit/releases/latest/download/CHIPSET.EXE) | ~8 KB | CLI chipset detection |
| [NCCONFIG.EXE](https://github.com/jfabienke/cache-kit/releases/latest/download/NCCONFIG.EXE) | ~6 KB | CLI NC region config |

➡️ **[All Downloads on Releases Page](https://github.com/jfabienke/cache-kit/releases)**

## Overview

CACHEKIT is a DOS-based utility suite that provides comprehensive chipset detection, cache management, non-cacheable (NC) region configuration, and bus configuration for vintage x86 systems. It supports 62+ chipsets from 286-era through PCI/PCIe systems.

## Features

- **Chipset Detection** - Identifies 62+ chipsets from Intel, SiS, OPTi, VIA, ALi, UMC, and more
- **Cache Control** - Enable/disable L1/L2 cache with proper write-back flush handling
- **NC Regions** - Configure non-cacheable memory regions for DMA device buffers
- **Bus Enumeration** - Full PCI, PCIe, MCA, EISA, and ISA Plug-and-Play device scanning
- **Bus Configuration** - EISA/MCA slot configuration with enable/disable and resource editing
- **Register Dump** - Browse and edit chipset configuration registers
- **Memory Benchmarks** - Cache-on vs cache-off bandwidth comparison

## Screenshots

```
+------------------------------------------------------------------------------+
| CACHEKIT v3.1  [F1 Info] F2 NC  F3 Test  F4 Reg  F5 Bench  F6 Prof     Alt-X |
+------------------------------------------------------------------------------+
|  CHIPSET DETECTED                          SYSTEM STATUS                     |
|  -----------------                         -------------                     |
|  Name:    OPTi 82C391 (SYSC)               CPU:     386DX                    |
|  Vendor:  OPTi Inc.                        Memory:  16384 KB                 |
|  Ports:   22h / 24h                        A20:     Enabled                  |
|                                                                              |
|  CACHE CONTROLLER                          SCORE                             |
|  ----------------                          -----                             |
|  Type:       Write-Back                    9.2/10  S-TIER: Driver's Dream    |
|  Size:       256 KB                                                          |
+------------------------------------------------------------------------------+
```

## Quick Start

```bash
# Build (requires Open Watcom C/C++)
wmake

# Run
CACHEKIT.EXE
```

**Navigation:**
| Key | Screen |
|-----|--------|
| F1 | System/Chipset Information |
| F2 | NC Region Configuration |
| F3 | Cache Flush Testing |
| F4 | Register Dump |
| F5 | Memory Benchmarks |
| F6 | Configuration Profiles |
| F7 | Expansion Card Inventory |
| F8 | EISA/MCA/ISA PnP Bus Configuration |
| Alt-X | Exit |

## Building

**Requirements:**
- Open Watcom C/C++ v2 compiler
- `WATCOM` environment variable set

### DOS Native Build
```bash
wmake              # Build CACHEKIT.EXE (HAL-based v3.x)
wmake legacy       # Build single-file v2.x version
wmake cli          # Build standalone CLI tools
wmake clean        # Remove build artifacts
wmake rebuild      # Clean and rebuild
```

### macOS Cross-Compilation
Build from macOS (including Apple Silicon) using Open Watcom v2:
```bash
./build_macos.sh hal      # Build CACHEKIT.EXE
./build_macos.sh cli      # Build CLI tools
./build_macos.sh clean    # Clean build artifacts
./build_macos.sh          # Build all
```

See [WATCOM_COMPILER_NOTES.md](WATCOM_COMPILER_NOTES.md) for detailed cross-compilation setup and C89 compliance notes.

## Supported Chipsets

### S-Tier (Score 9.0+) - Full Cache Control
- Intel 430FX/HX/VX/TX/MX, 440FX, 450GX
- SiS 85C460, 85C496, 85C310 (Rabbit)
- OPTi 82C391 (SYSC)
- VIA Apollo MVP3

### A-Tier (Score 8.0-8.9) - High Performance
- Intel 420TX/ZX, 430LX/NX (EISA)
- SiS 5591, 5598, 530
- OPTi 82C596/597 (Viper)
- VIA Apollo VP3
- ALi M1541 (Aladdin V)

### B-Tier (Score 7.0-7.9) - Solid Support
- Intel 82350/82350DT (EISA)
- VIA VT82C570 (VP1), VT82C310
- OPTi 82C381 (Symphony)

### C/D-Tier - Limited Support
- C&T PEAK/SCAT, Contaq 82C596, Forex, Suntac

### I-Tier (Info-Only) - 286/386SX Era
- C&T CS8221 (NEAT), Headland HT101/102, VLSI VL82C100/101/102
- IBM PS/2 MCA systems (hardware-enforced cache coherency)

## Architecture

```
CACHEKIT.C (Main TUI)
    │
    ├── CK_UI.C/H      (dialogs, menus)
    ├── CK_VIDEO.C/H   (VGA text mode)
    ├── CK_HAL.C/H     (detection registry)
    ├── CK_IO.C        (PCI/legacy port I/O)
    ├── CK_ENUM.C/H    (bus enumeration)
    └── CK_BCFG.C/H  (EISA/MCA/ISA PnP config)
            │
            └── Chipset Implementations:
                ├── CK_INTEL.C   (430FX/HX/VX/TX/MX, EISA)
                ├── CK_SIS.C     (460, Rabbit, 496, 5598)
                ├── CK_OPTI.C    (391, 381, Viper)
                ├── CK_VIA.C     (VP1, VP3, MVP3)
                ├── CK_ALI.C     (Aladdin IV/V)
                ├── CK_LEGAC.C  (NEAT, Headland, VLSI)
                └── CK_OTHER.C   (UMC, Eteq, Faraday)
```

## Critical Notes

### Write-Back Cache Warning

**CRITICAL:** Write-Back caches hold dirty data not yet written to RAM.

| Cache Type | Correct Order |
|------------|---------------|
| Write-Back | FLUSH first, then DISABLE |
| Write-Through | DISABLE first, then FLUSH |

Disabling a WB cache BEFORE flushing causes **DATA CORRUPTION!**

### Port Conflicts

Ports 0x22/0x23/0x24 may conflict with LPT2 or sound cards. Detection checks for floating bus conditions before probing.

## CLI Utilities

Standalone command-line tools (built with `wmake cli`):

- **CHIPSET.EXE** - Chipset detection and info
- **NCCONFIG.EXE** - NC region configuration
- **CACHETST.EXE** - Cache flush testing

## Testing

- **Emulators:** PCem, 86Box (support 286 through Pentium 4 era)
- **Key chipsets:** Intel 430FX (PCI), OPTi 82C391 (port quirk), SiS 496

## Version History

| Version | Highlights |
|---------|------------|
| v3.1 | F8 EISA/MCA/ISA PnP bus configuration screen |
| v3.0 | HAL architecture refactor, modular chipset files |
| v2.8 | SMBIOS/ACPI system information |
| v2.7 | F7 expansion card inventory (PCI/MCA/EISA/ISA PnP) |
| v2.4 | Per-feature access descriptor architecture |
| v2.0 | TUI with F-key navigation, benchmarks |

## License

Part of the Abacus FPGA Project. See project root for license information.

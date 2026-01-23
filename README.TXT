ABACUS DOS UTILITIES
====================

Part of the Abacus FPGA Project
For 386/486 ISA Systems

Last Updated: 2026-01-06 11:25:00 EST


OVERVIEW
--------

These DOS utilities support the Abacus FPGA coprocessor by providing
chipset detection, non-cacheable region configuration, cache flush
testing, and memory benchmarking on 386DX and compatible 486/Pentium
ISA systems.

Two versions are available:
  - CACHEKIT.EXE  Combined TUI with F-key tabs (RECOMMENDED)
  - CLI utilities  Standalone command-line tools


================================================================================
CACHEKIT.EXE v3.0 - Combined TUI Utility (RECOMMENDED)
================================================================================

Full-screen text user interface combining all functionality into one tool.
Navigate between screens using F1-F8 keys or Tab.

Usage: CACHEKIT

Keys:
  F1        System/Chipset Information + Cache Toggle
  F2        NC Region Configuration
  F3        Cache Flush Testing
  F4        Register Dump
  F5        Memory Bandwidth Benchmarks
  F6        Configuration Profiles
  F7        Expansion Card Inventory
  F8        EISA/MCA Bus Configuration (NEW in v3.1)
  Tab       Next screen
  Alt-X     Exit

NEW IN v2.0:
  - Safe cache toggle (T key) with auto-flush for Write-Back chipsets
  - F5 Benchmark screen with cache on/off comparison
  - F6 Configuration profiles (placeholder)
  - Report generator (S key saves REPORT.TXT)
  - Expanded chipset support (Intel 430FX/HX/VX, ALi Aladdin, SiS 496, etc.)
  - PCI configuration space detection for Pentium-era chipsets

NEW IN v2.1:
  - Late Socket 7 / Super7 chipsets (1997-1999):
    Intel 430TX, VIA Apollo VP3/MVP3, SiS 5591/5598/530, ALi M1541, Intel 430MX
  - Full PCI config space I/O for Pentium-era chipsets (read AND write)
  - Cache toggle, NC regions, and register dump all work via PCI for PCI chipsets
  - Now supports 34 chipsets total (386 through Super7 era)

NEW IN v2.2:
  - 286/386SX chipset inventory support (INFO-ONLY mode):
    C&T CS8221 (NEAT), C&T CS8230 (NEAT-386), Headland HT101/102/18,
    VLSI VL82C100/101/102/320, ALi M1217, OPTi 82C212
  - 286 inventory screen showing Shadow RAM map, A20 status, DRAM config
  - A20 gate toggle with confirmation (for chipsets with direct A20 control)
  - Now supports 45 chipsets total (286 through Super7 era)

NEW IN v2.2.1:
  - Fixed Contaq 82C596 I/O routing (uses legacy ports 0x22/0x23, not PCI)
  - Corrected Contaq 82C596 cache register (Index 0x11 bit 0 per 86Box source)
  - Contaq 82C596 reclassified: 486 VLB-era C-tier, shadow RAM only (no NC regions)
  - Added Port 0x92 Fast A20 support for 286/386SX chipsets:
    Headland HT101/102/18, VLSI VL82C100/101/102/320, ALi M1217
  - A20 toggle now works on more chipsets via universal Fast A20 mechanism

NEW IN v2.3:
  - Intel 82385-style external cache detection via timing analysis
  - Works on 386 systems with "Unknown" chipset where no registers are available
  - Automatically detects 32KB or 64KB external cache by measuring memory access
    timing at different working set sizes (8/32/64/128 KB sweep)
  - Quick hint check on startup (~100ms), full probe only if cache likely present
  - Press 'P' on F1 screen to manually re-probe external cache
  - Shows cache size, line size (16 bytes), and speed boost ratio
  - Also detects similar cache controllers: Intel 82395, C&T CS8231 (82C307)
  - Tested on both real 386 hardware and 86Box emulator

NEW IN v2.3.1:
  - CRITICAL FIX: SiS Super7 chipset register correctness (per datasheets)
    * SiS 5598: Cache control now at PCI 0x51 bit 6 (was 0x10 bit 0)
    * SiS 530:  Cache control now at PCI 0x51 bit 7 (was 0x10 bit 0)
    * SiS 496:  Cache control now at PCI 0x42 bit 0 (was 0x10 bit 0)
  - CRITICAL FIX: SiS 5598/530 NC region registers corrected
    * Now uses 0x77 (enable) + 0x78-0x7B (2 regions, 16-bit encoding)
    * Was incorrectly using 0x50-0x57 (4 regions)
    * Size encoding: bits 15:13 = size code (64K-8M), bits 12:0 = base A28:A16
  - SiS 5598/530 now correctly show 2 NC regions (was incorrectly 4)
  - Added TODO markers for SiS 496/5591 NC regions (need datasheet verification)

NEW IN v2.3.2:
  - SiS 5591 register correctness (verified via 86Box emulator source)
    * Cache control: 0x51 bit 7 (was assumed bit 6)
    * NC regions: 0x77 + 0x78-0x7B (same as 5598/530, was using 0x50-0x57)
    * 2 NC regions (was incorrectly 4)
  - Open source verification performed against:
    * 86Box emulator: sis_5591_h2p.c, sis_85c496.c, sis_5571_h2p.c
    * Linux kernel: PCI quirks (no cache control, but confirms PCI IDs)
    * BSD sources: AGP/IDE drivers (confirms register patterns)
  - All SiS Super7 chipsets (5591/5598/530) now use consistent NC model

NEW IN v2.4 (ARCHITECTURAL REFACTOR):
  - Per-feature access descriptor architecture eliminates "global routing problem"
    * Each feature (cache, NC, shadow, A20) has its own access method in the table
    * No more is_pci_chipset() binary routing for all features
    * Chipsets like Contaq 82C596 correctly use legacy ports for NC while
      other Pentium-era chipsets use PCI - without special-case code
  - Unified feature_descriptor_t structure with 21 fields:
    * Identification: chipset_type, feature_type
    * Access routing: access_method, index_port, data_port, pci_bus/dev/func
    * Register layout: base_reg, ctrl_reg, reg_count
    * Encoding parameters: encoding, bit_position, enable_high, base_shift, etc.
    * Function pointers: read_fn, write_fn, clear_fn for complex encodings
  - Six access methods: PCI_CONFIG, LEGACY_22_23, LEGACY_22_24, EISA, SPECIAL_FN, NONE
  - Seven encoding types: BIT_TOGGLE, BASE_SIZE_PAIR, 16BIT_PACKED, BOUNDARY,
    STEERING, PAM (info-only), NONE
  - 60+ feature descriptors covering all 4 feature types across all chipsets:
    * 28 FEAT_CACHE entries (legacy + PCI chipsets)
    * 18 FEAT_NC entries (OPTi, SiS, Intel, VIA, ALi families)
    * 8 FEAT_SHADOW entries (NEAT, Headland, Intel Triton)
    * 3 FEAT_A20 entries (chipsets with dedicated A20 register control)
  - Generic encode/decode functions for NC regions:
    * decode_nc_region_base_size_pair() / encode_nc_region_base_size_pair()
    * Handles both OPTi (8KB min) and SiS/VIA (64KB min) styles automatically
  - SiS 5598/530 NC regions still use specialized handlers via function pointers
  - v2 API functions run parallel to v1 for gradual migration:
    * is_cache_enabled_v2(), set_cache_enabled_v2()
    * read_nc_regions_v2(), write_nc_region_v2(), clear_nc_region_v2()
    * toggle_a20_v2()
  - Adding a new chipset now requires only table entries, no switch modification

NEW IN v2.5 (EISA CHIPSET EXPANSION):
  - Expanded EISA chipset support (16 new chipsets, 17 EISA total):
    * Intel 82350DT (Mongoose) - Full cache control via 82359 DRAM controller
    * Intel EISA PCIsets (7 chipsets with 82375EB/SB PCI-EISA bridge):
      420TX Saturn, 420ZX Saturn II, 430LX Mercury, 430NX Neptune,
      430HX Triton II, 440FX Natoma, 450GX Orion Server
    * OPTi EISA (4 chipsets): 82C682, 82C683, 82C691/696 Hunter,
      82C693/6/7 Pentium WB (info-only, pending datasheets)
    * Other vendors (4 chipsets): SiS 85C411/406, VIA VT82C495,
      Atmel AT40957/58/59, TI TACT84500 (info-only, pending datasheets)
  - Multi-stage EISA detection with PCMC differentiation:
    * Checks for PCI-EISA bridge first (8086:0482)
    * Identifies PCMC to determine specific Intel PCIset variant:
      82424 (420TX/ZX), 82434 (430LX/NX), 82439HX, 82441FX, 82454GX
    * Probes 82359 chip ID for 82350DT vs original 82350
    * Probes OPTi, SiS, VIA vendor-specific patterns
    * Falls back to generic 82350 if no specific chipset detected
  - Detection functions: detect_82350dt(), detect_eisa_pciset(),
    detect_opti_eisa(), detect_sis_eisa(), detect_via_eisa(),
    detect_atmel_eisa(), detect_ti_eisa()
  - Feature descriptors for all Intel EISA PCIsets (cache via PCI 0x52)
  - Now supports 61 chipsets total (286 through EISA+PCI bridge era)

NEW IN v2.6 (MCA BUS SUPPORT):
  - Added IBM PS/2 MCA (Micro Channel Architecture) bus detection
  - MCA treated as observer-only: hardware-enforced cache coherency via
    bus snooping makes software cache management unnecessary
  - Same architectural model as PCI/PCIe: no software intervention needed
  - Detection via INT 15h AH=C0h BIOS configuration table feature byte 1
    (bit 1 = MCA bus present)
  - Reports PS/2 model/submodel from BIOS configuration table
  - No cache control or NC region management (not needed on MCA)
  - Coverage now spans all major PC bus architectures:
    ISA (active controller), EISA (inspector), MCA (observer), PCI (observer)
  - Now supports 62 chipsets total (286 through MCA/PCI era)

NEW IN v2.7 (F7 EXPANSION CARD INVENTORY):
  - New F7 screen for PCI device enumeration
  - Full bus scan: all 256 buses, 32 devices, 8 functions per device
  - Displays: Bus type, B:D.F address, Vendor:Device IDs, Class/Subclass, IRQ
  - Embedded PCI ID database with ~600 entries for device name lookup:
    Intel (chipsets, NICs, ICH), AMD/ATI (GPUs, chipsets, PCnet), NVIDIA (all gens),
    3dfx (Voodoo), S3 (Trio/ViRGE/Savage), VIA (chipsets, audio, USB), SiS,
    ALi/Acer Labs, Matrox, Cirrus Logic, Creative/Ensoniq, Realtek, Broadcom,
    LSI/Symbios SCSI, Adaptec SCSI, Promise IDE/SATA, Trident, OPTi, UMC, Tseng Labs
  - Scrolling list with keyboard navigation (Up/Down/PgUp/PgDn/Home/End)
  - R key to rescan devices
  - Auto-scan on F7 entry for immediate results
  - Foundation for Session 2 (MCA/EISA enumeration) and Session 3 (ISA PnP/PCIe)

NEW IN v2.7.1 (MCA + EISA ENUMERATION):
  - MCA (Micro Channel Architecture) device enumeration:
    * Scans all 8 MCA slots via POS (Programmable Option Select) registers
    * Reads adapter ID from POS bytes 0-1 at ports 0x100-0x101
    * Embedded MCA ID database with ~50 entries for IBM adapter names:
      XGA/XGA-2 displays, SCSI adapters, Token Ring, Ethernet, memory, etc.
    * Test on: IBM PS/2 Model 50/60/70/80, PCem PS/2 emulation
  - EISA (Extended ISA) device enumeration:
    * Scans all 15 EISA slots via configuration space (port 0xzC80)
    * Reads compressed vendor ID and product ID (EISA 3-char vendor encoding)
    * Decodes vendor ID for display (e.g., "CPQ" = Compaq, "IBM" = IBM)
    * Test on: 486 EISA systems, 86Box EISA emulation
  - F7 summary line now shows: [PCI:N MCA:N EISA:N]
  - ISA Plug-and-Play enumeration planned for Session 3

NEW IN v2.7.2 (ISA PnP + PCIe DETECTION):
  - ISA Plug-and-Play device enumeration:
    * Full isolation protocol implementation (PnP spec 1.0a compliant)
    * 32-byte LFSR initiation key sequence
    * Automatic read data port discovery (0x203-0x3F3)
    * CSN (Card Select Number) assignment for all detected cards
    * Resource descriptor parsing: IRQ, I/O base addresses
    * Embedded ISA PnP ID database with ~45 entries:
      Creative Sound Blaster (SB16/AWE32/AWE64/Vibra), ESS AudioDrive,
      Yamaha OPL3-SA series, Crystal CS4232/CS4236, OPTi audio,
      3Com EtherLink III, Intel EtherExpress, SMC/WD, Realtek NE2000,
      US Robotics modems, Adaptec SCSI, serial/parallel/IDE controllers
    * Test on: Any system with ISA PnP cards (SB16 PnP, NE2000 PnP, etc.)
  - PCIe device detection:
    * PCI Express Capability structure detection (Cap ID 0x10)
    * Devices with PCIe capability now shown as "PCIe" instead of "PCI"
    * ACPI MCFG table search in BIOS area (foundation for MMIO access)
    * Works on Pentium 4+ systems with PCIe-capable chipsets
  - F7 summary line now shows all 5 bus types:
    [PCI:N PCIe:N MCA:N EISA:N PnP:N]
  - Complete expansion card inventory across all PC architectures:
    From 386 ISA systems to modern PCIe machines

NEW IN v2.8 (SMBIOS/ACPI SYSTEM INFORMATION):
  - F1 screen now has Tab-switchable views:
    * Chipset View: Original chipset and cache information
    * SMBIOS/ACPI View: Detailed system information from firmware tables
  - SMBIOS/DMI table parsing (Pentium+ systems):
    * Scans BIOS area F0000-FFFFF for "_SM_" signature
    * Parses Type 0 (BIOS), Type 1 (System), Type 2 (Baseboard), Type 3 (Chassis)
    * Parses Type 4 (Processor), Type 16 (Memory Array), Type 17 (Memory Device)
    * Displays: BIOS vendor/version/date, system manufacturer/product,
      board manufacturer/product, chassis type, memory slot details
    * Version detection (SMBIOS 2.x/3.x)
  - ACPI table parsing:
    * Scans BIOS area E0000-FFFFF for "RSD PTR " signature
    * Parses RSDP (Root System Description Pointer)
    * Walks RSDT (Root System Description Table) to enumerate all tables
    * Detects: FACP/FADT, MCFG (PCIe), APIC/MADT, HPET
    * Displays: ACPI revision, OEM ID, PM profile, table presence checkboxes
  - Enhanced chipset detection via SMBIOS:
    * For unknown chipsets, cross-references board product name
    * Identifies Intel 430FX/VX/HX/TX, 440BX/LX, 810/815 from SMBIOS
    * Also detects VIA Apollo, SiS 530, ALi Aladdin from board names
    * Displayed as "Intel 440BX (SMBIOS)" etc. when identified
  - Works on systems from late Pentium era (1996+) through modern PCs
  - Graceful fallback: Shows "not found" on 386/486 without SMBIOS

NEW IN v3.1 (EISA/MCA BUS CONFIGURATION):
  - New F8 screen for EISA and MCA slot/device configuration:
    * Auto-detects EISA or MCA bus (mutually exclusive architectures)
    * Lists all populated slots with adapter ID, enable status, resources
    * Enable/disable individual adapter cards
    * View and edit resource assignments (IRQ, DMA, I/O, Memory)
  - EISA configuration support:
    * Scans slots 0-15 for installed adapters
    * Reads compressed EISA vendor ID and product ID
    * Embedded database of ~150 common EISA cards:
      - SCSI: Compaq Smart Array, Adaptec AHA-1740/2740, BusLogic BT-742/747/757,
        DPT PM series, NCR 53C700/710/810/825, Future Domain TMC, UltraStor,
        Mylex DAC960, ICP GDT3000
      - Network: 3Com 3C503/507/509/579/592/597, Intel EtherExpress PRO/100,
        DEC DE422/425/450/500, HP EtherTwist, SMC Elite32, Madge Token Ring
      - Graphics: ATI Mach32 EISA, Matrox MGA/Millennium, S3 Vision 864/964,
        Tseng Labs ET4000, Cirrus Logic GD543x, Number Nine GXE, Weitek Power 9000
      - Sound: Creative Sound Blaster, Media Vision Pro AudioSpectrum, Gravis UltraSound
      - Serial/Modem: US Robotics, Hayes, DigiBoard, Equinox, Comtrol, Specialix
    * Card enable/disable via control register
    * NVM save to Extended CMOS (preserves config across reboots)
    * BIOS vendor detection for NVM protocol compatibility:
      - Compaq, Dell, Micronics, Intel: Ports 0x800-0x802, sum checksum
      - HP, Olivetti: Ports 0xC00-0xC02, XOR checksum
      - DEC, NCR: INT 15h AH=D8h BIOS services (most portable)
      - AMI, Award, Phoenix: Compaq-compatible
    * Auto-detects INT 15h AH=D8h EISA CMOS services when available
    * Hardcoded EISA system board detection via slot 0 ID (0x0C80-0x0C83):
      - Complete database of 60+ system boards from major vendors
      - Compaq: SystemPro, Deskpro, ProSignia, ProLiant series
      - HP: Vectra RS/486/XU/VL, NetServer LE/LM/LF/LH/LC/LS/LH Pro/LX Pro
      - DEC: DECpc 433/450, DECstation, DECpc AXP 150 (Alpha Jensen)
      - Dell: PowerEdge 2100/2200/4100/4200
      - NCR: System 3230/3340/3410/3430/3450/3550
      - Intel: EISA 486 Reference, Alder, Xpress Server
      - Olivetti: CP486, M486, NetStrada
      - Micronics: EISA-3, EISA-4, Gemini
      - AST: Premium 386/486, Manhattan P
      - ALR: Business VEISA, PowerPro, Revolution
      - Acer: AcerPower 4e, AcerAltos 7000
      - Apricot: LS Pro, XEN-S
      - NEC: PowerMate 486, Express Server
      - Automatic slot count detection based on system board identification
  - MCA (Micro Channel) configuration support:
    * Scans slots 0-7 for PS/2 adapter cards
    * Reads all 8 POS (Programmable Option Select) registers
    * Embedded database of ~50 common MCA cards (IBM, 3Com, Adaptec, etc.)
    * Expanded POS decode table with 30+ adapter-specific parsers:
      - IBM Token Ring, SCSI, XGA, ESDI, IDE, Ethernet adapters
      - 3Com EtherLink/MC, EtherLink III/MC, EtherLink/MC 32
      - Adaptec AHA-1640, BusLogic BT-640A, NCR 53C700, Future Domain
      - Sound Blaster MCV/Pro MCV, Pro AudioSpectrum 16
      - ATI 8514/Ultra, Graphics Ultra, Matrox MG-1024
      - Intel and US Robotics modems
    * Card enable/disable via POS register 2 bit 0
    * Direct POS register editing for advanced users
    * NVRAM save support
    * Hardcoded PS/2 model detection via INT 15h AH=C0h:
      - Complete database of 50+ IBM PS/2 models with slot counts
      - Model 25/25SX (0 slots), Model 30 (0 slots), Model 50/50Z (4 slots)
      - Model 55SX (3 slots), Model 60 (8 slots), Model 70 (3 slots)
      - Model 80 (8 slots), Model 90/95 (8 slots), Model 35/40 (3 slots)
      - Rare third-party MCA: NCR System 3000, Tandy 5000, Reply 32, Apricot Qi
      - Automatic slot count detection based on model identification
      - IBM PS/2 vs third-party clone detection
    * NEW: Bus width and speed detection by PS/2 model:
      - 16-bit MCA @ 10 MHz: Model 50, 50Z, 55SX, 55LS, 60, 65SX, 35/40 (286/386SX)
      - 32-bit MCA @ 16-25 MHz: Model 70, 80 (386DX at 16/20/25 MHz)
      - 32-bit MCA @ 25-33 MHz: Model 90, 95, Servers (486/Pentium)
      - Bus width/speed displayed in F8 screen title bar
    * NEW: MCA adapter capability detection:
      - Bus master detection for SCSI, Token Ring, XGA, and other adapters
      - Arbitration level display (0-15)
      - Fairness mode detection (cooperative vs burst mode)
      - Streaming data mode detection for high-performance adapters
      - Capabilities displayed in slot details panel
  - Configuration export (W key):
    * Exports detailed configuration report to BUSCFG.TXT
    * Includes system info, slot details, resources, and conflicts
    * Useful for documentation and troubleshooting
  - ISA Plug and Play configuration support (NEW):
    * Full ISA PnP 1.0a protocol implementation
    * 32-byte LFSR initiation key sequence for card wake-up
    * Card isolation and automatic CSN (Card Select Number) assignment
    * Resource data parsing: IRQ, DMA, I/O ports, memory windows
    * Embedded database of 150+ common ISA PnP cards:
      - Sound (100+ entries): Creative SB16/AWE32/AWE64/Vibra16/Live,
        ESS ES688/1688/1868/1869/1878/1898, Yamaha OPL3-SA/SA2/SA3,
        Crystal CS4231/4232/4235/4236/4237, OPTi 82C924/925/929/930/931,
        Gravis UltraSound/MAX PnP, Aztech PRO16V/Galaxy/Nova,
        Analog Devices AD1816, C-Media CMI8330/8338, Ensoniq AudioPCI,
        Media Vision Pro AudioSpectrum, TerraTec EWS64, Turtle Beach Tropez/Pinnacle
      - Network (17 entries): 3Com 3C509B/3C515/3C574/3C589,
        Intel EtherExpress PRO/100, Realtek RTL8019/8029,
        SMC EtherEZ/Elite16 Ultra, DEC DE200/201, Accton EN1660/2212
      - Modems (28 entries): US Robotics Sportster/Courier 14.4-56K,
        Hayes Accura/Optima, Practical Peripherals, Zoom 14.4-56K,
        Motorola ModemSurfr/VoiceSurfr, Rockwell chipset modems
      - System/Storage (15 entries): National Semi PC87306/307/308 Super I/O,
        Adaptec AHA-1510/1520/1542 SCSI, Future Domain TMC-1610/1800,
        UMC UM8663/8672 IDE, PnP system devices (PIC/Timer/DMA/RTC)
    * Activate/deactivate logical devices (A/D keys)
    * Works standalone (ISA/VLB systems) or combined with EISA
    * Mixed EISA + ISA PnP mode:
      - EISA slots shown first, then ISA PnP cards appended
      - Title shows "EISA + ISA PnP CONFIGURATION (+N PnP)"
      - Individual card actions based on bus type (A=Activate for PnP, A=AutoCfg for EISA)
    * MCA excluded: PS/2 systems use separate slot architecture (no ISA bus)
  - New source files:
    * CK_BCFG.C/H - Bus configuration API and implementation
  - Test on 86Box: EISA 486 systems, IBM PS/2 Model 80, ISA systems with PnP cards

NEW IN v3.0 (HAL ARCHITECTURE REFACTOR):
  - Complete codebase restructure using Hardware Abstraction Layer (HAL)
  - Per-chipset operations structure (chipset_ops_t) replaces switch statements
  - All chipset-specific code now in dedicated source files:
    * CK_HAL.C/H   - HAL core infrastructure and detection registry
    * CK_IO.C      - PCI config space and legacy port I/O
    * CK_INTEL.C   - Intel chipsets (430FX/HX/VX/TX/MX, EISA PCIsets)
    * CK_SIS.C     - SiS chipsets (460, Rabbit, 496, 5598, 530, 5591, EISA)
    * CK_OPTI.C    - OPTi chipsets (391, 381, Viper, 212, EISA variants)
    * CK_VIA.C     - VIA chipsets (VT82C495, VP1, VP3, MVP3)
    * CK_ALI.C     - ALi chipsets (Aladdin IV, Aladdin V)
    * CK_LEGAC.C  - 286/386-era (NEAT, PEAK, SCAT, Headland, VLSI)
    * CK_OTHER.C   - UMC, Eteq, Faraday, MIC, Contaq, Forex, Suntac, MCA
    * CK_VIDEO.C/H - Video primitives (text mode, boxes, keyboard)
    * CK_UI.C/H    - UI components (dialogs, menus, status bars)
  - HAL operations include:
    * probe()       - Detect if chipset is present
    * cache_get/set/flush() - Cache control
    * nc_read/write/clear() - NC region management
    * shadow_get/set()      - Shadow RAM control
    * a20_get/set()         - A20 gate control
    * reg_read/write()      - Raw register access
  - Benefits:
    * Adding new chipsets requires only table entries, no switch modifications
    * Each chipset's code is self-contained and testable
    * Clean separation of UI, video, and hardware access
    * Easier to maintain and extend
  - HAL-first with legacy fallback ensures backward compatibility
  - Detection priority: MCA -> PCI -> EISA -> Legacy ISA
  - Multi-file build via MAKEFILE (wmake)
  - Binary size ~65KB (vs ~45KB single-file, acceptable for modularity)


SOURCE CODE MODULES (v3.1)
--------------------------

Architecture:

  +------------------------------------------------------------------+
  |                        CACHEKIT.C                                |
  |              (Main App: UI Screens, State, Main Loop)            |
  +------------------------------------------------------------------+
       |              |                |                |
       v              v                v                v
  +----------+  +----------+    +------------+   +------------+
  | CK_UI.C  |  |CK_VIDEO.C|    |  CK_HAL.C  |   |  CK_IO.C   |
  | Dialogs  |  |  VGA     |    |  Registry  |   | PCI/Legacy |
  |  Menus   |  | Keyboard |    |  Fallback  |   |  Port I/O  |
  +----------+  +----------+    +------------+   +------------+
                                      |
            +-------------------------+-------------------------+
            |         |         |         |         |           |
            v         v         v         v         v           v
       +---------+---------+---------+---------+---------+---------+
       |CK_INTEL |CK_SIS   |CK_OPTI  |CK_VIA   |CK_ALI   |CK_LEGAC|
       | 430FX   | 460     | 391     | VP3     | Aladdin | NEAT    |
       | 430HX   | Rabbit  | 381     | MVP3    | M1541   | Headland|
       | 430VX   | 496     | Viper   | VT82C495|         | VLSI    |
       | 430TX   | 5598    | EISA    |         |         |         |
       | EISA    | 530     |         |         |         |         |
       +---------+---------+---------+---------+---------+---------+
                                      |
                                      v
                               +------------+
                               | CK_OTHER.C |
                               | UMC, Eteq  |
                               | Faraday    |
                               | MIC, Forex |
                               | Contaq     |
                               +------------+

  Standalone CLI Tools (not shown): CHIPSET.C, NCCONFIG.C, CACHETST.C

HAL Core:
  File            Lines   Code   Purpose
  --------------- -----  -----   ----------------------------------------
  CACHEKIT.C      9,100+ 6,500+  Main app: UI screens, state, main loop
  CK_HAL.H          218    114   HAL interface: chipset_ops_t, result codes
  CK_HAL.C          374    210   Detection registry, unknown fallback, helpers
  CK_IO.C           281    157   PCI config space, legacy port I/O
  CK_BCFG.C       550+   400+  EISA/MCA bus configuration
  CK_BCFG.H       220+   120+  Bus configuration API

Chipset Implementations:
  File            Lines   Code   Chipsets Supported
  --------------- -----  -----   ----------------------------------------
  CK_LEGAC.C     1,212    689   C&T NEAT, Headland, VLSI, ALi M1217, OPTi 212
  CK_OTHER.C      1,123    698   UMC, Eteq, Faraday, MIC, Forex, Suntac, Contaq
  CK_SIS.C          902    552   SiS 460, Rabbit, 496, 5591, 5598, 530, EISA
  CK_OPTI.C         897    493   OPTi 391, 381, Viper, 212, EISA variants
  CK_INTEL.C        882    513   430FX/HX/VX/TX/MX, 82350DT, EISA PCIsets
  CK_VIA.C          632    349   VT82C495 (Venus), VT82C310, VP1, VP3, MVP3
  CK_ALI.C          414    221   M1209 (Finit), M1489 (Aladdin), M1541

UI Layer:
  File            Lines   Code   Purpose
  --------------- -----  -----   ----------------------------------------
  CK_VIDEO.H        220     97   Video API: constants, box chars, function decls
  CK_VIDEO.C        348    213   Direct VGA: putc, puts, box, cursor, getkey
  CK_UI.H           207     65   UI API: dialogs, menus, screen types
  CK_UI.C           631    456   Dialogs, menus, title/status bars, tables

Standalone CLI Tools:
  File            Lines   Code   Purpose
  --------------- -----  -----   ----------------------------------------
  NCCONFIG.C        847    656   NC region configuration utility
  CACHETST.C        721    491   Cache flush testing utility
  CHIPSET.C         596    412   Chipset detection utility

Build & Documentation:
  File            Lines          Purpose
  --------------- -----          ----------------------------------------
  MAKEFILE          157          Open Watcom build rules
  README.TXT        750+         User documentation (this file)

Summary:
  Category          Files   Lines    Code
  ---------------   -----   ------   ------
  HAL Core              4    9,770    6,824
  Chipset HAL           7    6,062    3,515
  UI Layer              4    1,406      831
  CLI Tools             3    2,164    1,559
  Build/Docs            2      900+      76
  ---------------   -----   ------   ------
  TOTAL                20   20,300+  12,805


SCREEN 1: INFO (F1) - System Information
----------------------------------------

+------------------------------------------------------------------------------+
| CACHEKIT v2.0  [F1 Info] F2 NC  F3 Test  F4 Reg  F5 Bench  F6 Prof    Alt-X |
+------------------------------------------------------------------------------+
|                                                                              |
|  CHIPSET DETECTED                          SYSTEM STATUS                     |
|  -----------------                         -------------                     |
|  Name:    OPTi 82C391 (SYSC)               CPU:     386DX                    |
|  Vendor:  OPTi Inc.                        Memory:  16384 KB                 |
|  Ports:   22h / 24h                        A20:     Enabled                  |
|  ID:      0x40 @ Index 20h                                                   |
|                                            CACHE         [########..] 256K   |
|  CACHE CONTROLLER                          NC Region     [..........] None   |
|  ----------------                                                            |
|  Type:       Write-Back                    STATUS                            |
|  Size:       256 KB                        ------                            |
|  Enabled:    Yes                           [X] Cache Enabled                 |
|  Flush:      Read Loop (386)               [ ] NC Region Active              |
|                                            [X] A20 Gate Open                 |
|  NC REGIONS                                                                  |
|  ----------                                SCORE                             |
|  Strategy:   Range-based                   -----                             |
|  Available:  2 regions                     9.2/10  [####################..]  |
|  Granularity: 8 KB                         S-TIER: Driver's Dream            |
|                                                                              |
+------------------------------------------------------------------------------+
| OPTi 82C391: Write-Back cache. FLUSH before DISABLE to prevent corruption!   |
+------------------------------------------------------------------------------+


SCREEN 2: NC CONFIG (F2) - Non-Cacheable Region Configuration
-------------------------------------------------------------

+------------------------------------------------------------------------------+
| CACHEKIT v2.0   F1 Info [F2 NC] F3 Test  F4 Reg  F5 Bench  F6 Prof     Alt-X |
+------------------------------------------------------------------------------+
|                                                                              |
|  MEMORY MAP                               NC REGIONS                         |
|  ----------                               ----------                         |
|  16 MB +------------------------+         #  Base     Size    Status         |
|        |    Extended Memory     |         -----------------------------      |
|        |      (Cacheable)       |         0  14.0 MB  512 KB  ACTIVE         |
|  14 MB +------------------------+ <---    1    --      --     empty          |
|        |:::: NC Region 0 :::::::|                                            |
|        |:::: (512 KB)    :::::::|         ACTIONS                            |
|13.5 MB +------------------------+         -------                            |
|        |    Extended Memory     |         [1] Edit Region 0                  |
|        |      (Cacheable)       |         [2] Edit Region 1                  |
|   1 MB +------------------------+         [C] Clear All                      |
|        |#### System / ROM ######|         [A] Auto-Config 512KB              |
| 640 KB +------------------------+                                            |
|        |  Conventional Memory   |         CHIPSET INFO                       |
|   0 KB +------------------------+         ------------                       |
|                                           Strategy: Range-based              |
|  Legend: : NC Region  # ROM/Reserved      Granularity: 8 KB                  |
|                                           Alignment: 8 KB boundary           |
|                                                                              |
+------------------------------------------------------------------------------+
| Region 0: Index 52h=0x80, 53h=0x4E  |  Enter=Edit  C=Clear  A=Auto           |
+------------------------------------------------------------------------------+


SCREEN 3: CACHE TEST (F3) - Cache Flush Testing
-----------------------------------------------

+------------------------------------------------------------------------------+
| CACHEKIT v2.0   F1 Info  F2 NC [F3 Test] F4 Reg  F5 Bench  F6 Prof     Alt-X |
+------------------------------------------------------------------------------+
|                                                                              |
|  CACHE INFORMATION                        TEST SELECTION                     |
|  -----------------                        --------------                     |
|  Chipset:   OPTi 82C391                   [X] Flush Sequence Test            |
|  Type:      Write-Back                    [X] Dirty Data Verification        |
|  Size:      256 KB                        [X] Timing Measurement             |
|  Status:    Enabled                       [ ] Stress Test (30 sec)           |
|  CPU:       386DX (no WBINVD)                                                |
|                                           [R] RUN SELECTED TESTS             |
|  FLUSH METHOD                                                                |
|  ------------                             TEST RESULTS                       |
|  (*) Read Loop (512 KB)                   ------------                       |
|  ( ) WBINVD (requires 486+)               Test                 Result        |
|  ( ) Hardware Trigger (MIC only)          ------------------------------     |
|                                           Flush Sequence       PASS          |
|  *** WRITE-BACK WARNING ***               Dirty Data           PASS          |
|  -------------------------                Timing               847 us        |
|  Correct order: FLUSH -> DISABLE          Stress Test          --            |
|  Wrong order causes DATA LOSS!                                               |
|                                           Overall: ALL TESTS PASSED          |
|                                                                              |
+------------------------------------------------------------------------------+
| Space=Toggle test  |  R=Run tests  |  S=Save report  |  Esc=Back to Info     |
+------------------------------------------------------------------------------+


SCREEN 4: REGISTERS (F4) - Register Dump
----------------------------------------

+------------------------------------------------------------------------------+
| CACHEKIT v2.0   F1 Info  F2 NC  F3 Test [F4 Reg] F5 Bench  F6 Prof     Alt-X |
+------------------------------------------------------------------------------+
|                                                                              |
|  REGISTER DUMP (Port 22h/24h)              SELECTED: Index 20h               |
|  ----------------------------              --------------------              |
|       00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F   Value:  41h          |
|  00:  40 00 00 00 80 00 00 00 00 00 00 00 00 00 00 00   Binary: 0100 0001    |
|  10:  00 00 00 42 00 00 00 00 00 00 00 00 00 00 00 00                        |
|  20: [41]00 00 00 00 00 00 00 00 00 00 00 00 00 00 00   BIT DECODE           |
|  30:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00   ----------           |
|  40:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00   7: 0 Reserved        |
|  50:  00 00 80 4E 00 00 00 00 00 00 00 00 00 00 00 00   6: 1 Cache: WB       |
|  60:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00   5: 0 Reserved        |
|  70:  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00   4: 0 Reserved        |
|                                                         3: 0 Reserved        |
|  KNOWN REGISTERS                                        2: 0 Reserved        |
|  ---------------                                        1: 0 Reserved        |
|  00h: Chip ID (0x40=391)                                0: 1 Cache ON        |
|  13h: Cache Size                                                             |
|  20h: Cache Control <--                                                      |
|  52h-55h: NC Regions                                                         |
|                                                                              |
+------------------------------------------------------------------------------+
| Arrow keys=Navigate  |  E=Edit  |  R=Refresh  |  S=Save dump  |  Esc=Back    |
+------------------------------------------------------------------------------+


SCREEN 5: BENCHMARK (F5) - Memory Bandwidth Tests (NEW in v2.0)
---------------------------------------------------------------

+------------------------------------------------------------------------------+
| CACHEKIT v2.0   F1 Info  F2 NC  F3 Test  F4 Reg [F5 Bench] F6 Prof     Alt-X |
+------------------------------------------------------------------------------+
|                                                                              |
|  MEMORY BANDWIDTH TESTS                RESULTS                               |
|  ----------------------                -------                               |
|  Test Size:   512 KB                   Test       Cache ON   Cache OFF       |
|  Iterations:  100                      ---------------------------------     |
|  Cache:       Enabled                  Copy       45.2 MB/s  12.1 MB/s       |
|  Chipset:     OPTi 82C391              Fill       52.3 MB/s  15.4 MB/s       |
|                                        Read       38.7 MB/s  10.8 MB/s       |
|  ACTIONS                                                                     |
|  -------                               COMPARISON                            |
|  [R] Run All Tests                     ----------                            |
|  [C] Compare (On vs Off)               Copy: 3.7x speedup                    |
|                                        Fill: 3.4x speedup                    |
|  PROGRESS                              Read: 3.6x speedup                    |
|  --------                                                                    |
|  [================    ] 78%            Overall: Cache provides 3.5x boost    |
|                                                                              |
+------------------------------------------------------------------------------+
| R=Run tests  |  C=Compare  |  S=Save report  |  Esc=Back                     |
+------------------------------------------------------------------------------+


SCREEN 6: PROFILES (F6) - Configuration Profiles (NEW in v2.0)
--------------------------------------------------------------

+------------------------------------------------------------------------------+
| CACHEKIT v2.0   F1 Info  F2 NC  F3 Test  F4 Reg  F5 Bench [F6 Prof]    Alt-X |
+------------------------------------------------------------------------------+
|                                                                              |
|  CURRENT SNAPSHOT                      SAVED PROFILES                        |
|  ----------------                      --------------                        |
|  Chipset: OPTi 82C391                  #  Filename      Date                 |
|  Cache:   256KB Write-Back             ----------------------------------------|
|  Memory:  16384 KB                     1. DEFAULT.CKT   2026-01-04           |
|                                        2. OPTIMAL.CKT   2026-01-03           |
|                                                                              |
|                                        ACTIONS                               |
|                                        -------                               |
|                                        [S] Save Current Snapshot             |
|                                        [L] Load Selected Profile             |
|                                        [A] Apply to Hardware                 |
|                                        [D] Delete Selected                   |
|                                                                              |
+------------------------------------------------------------------------------+
| S=Save  |  L=Load  |  (Profiles feature - placeholder in v2.0)               |
+------------------------------------------------------------------------------+


================================================================================
CLI UTILITIES (Standalone)
================================================================================

CHIPSET.EXE - Chipset Detection
  Auto-detects 17+ 386/486 chipsets and displays detailed information
  including cache type (Write-Back/Write-Through), NC region capabilities,
  and a suitability score for the Abacus driver.

  Usage: CHIPSET [/V] [/RAW]

    /V    Verbose output (show probed registers)
    /RAW  Raw hex dump of chipset registers
    /?    Help message

  Example output:

    Detected: OPTi 82C391 (SYSC)
      Ports:    22h/24h (Index/Data)
      Cache:    Write-Back (256KB detected)
      NC:       Range-based (2 regions, 8KB granularity)
      Score:    9.2/10 (S-Tier)


NCCONFIG.EXE - NC Region Configuration
  Configures Non-Cacheable memory regions for DMA bounce buffer
  allocation. Supports Range, Boundary, and Steering strategies
  depending on the detected chipset.

  Usage:
    NCCONFIG /S                        Show current NC config
    NCCONFIG /SET <base_mb> <size_kb>  Set NC region
    NCCONFIG /SET:<n> <base> <size>    Set specific region n
    NCCONFIG /CLEAR [region]           Clear NC region(s)
    NCCONFIG /TEST <base_mb> <size_kb> Dry-run (no writes)
    NCCONFIG /?                        Help message

  Parameters:
    base_mb   Base address in megabytes (e.g., 14 = 14MB)
    size_kb   Size in kilobytes (must match chipset granularity)

  Example:
    NCCONFIG /SET 14 512    Set 512KB NC region at 14MB


CACHETST.EXE - Cache Flush Testing
  Tests cache flush sequences and verifies Write-Back vs Write-Through
  behavior. Critical for validating DMA coherency.

  Usage: CACHETST [/FLUSH] [/VERIFY] [/TIMING]

    /FLUSH   Perform cache flush sequence test
    /VERIFY  Test write-back flush (WB chipsets only)
    /TIMING  Measure flush execution time
    /?       Help message

  With no options, all tests are performed.


SUPPORTED CHIPSETS
------------------

S-Tier (Score 9.0+) - Driver's Dream:
  - Intel 430HX             - WB, 4 NC regions, 64KB granularity (Pentium)
  - Intel 430HX EISA        - WB, 4 NC regions, 64KB granularity (EISA+PCI)
  - Intel 440FX EISA        - WB, 4 NC regions, 64KB granularity (EISA+PCI)
  - Intel 450GX EISA        - WB, 4 NC regions, 64KB granularity (Server EISA+PCI)
  - Intel 430FX (Triton)    - WB, 4 NC regions, 64KB granularity (Pentium)
  - Intel 430TX             - WB, 4 NC regions, 64KB granularity (Super7)
  - VIA Apollo MVP3         - WB, 4 NC regions, 64KB granularity (Super7)
  - SiS 85C496              - WB, 4 NC regions, 64KB granularity (Pentium)
  - SiS 85C460 (486)        - WB, 4 NC regions, 64KB granularity
  - SiS 85C310 (Rabbit)     - WB, 4 NC regions, 64KB granularity
  - OPTi 82C391 (SYSC)      - WB, 2 NC regions, 8KB granularity

A-Tier (Score 8.0-8.9) - High Performance:
  - Intel 430NX EISA        - WB, 4 NC regions, 64KB granularity (EISA+PCI Neptune)
  - Intel 430LX EISA        - WB, 4 NC regions, 64KB granularity (EISA+PCI Mercury)
  - Intel 420TX EISA        - WB, 4 NC regions, 64KB granularity (EISA+PCI Saturn)
  - Intel 420ZX EISA        - WB, 4 NC regions, 64KB granularity (EISA+PCI Saturn II)
  - Intel 430VX             - WB, 4 NC regions, 256KB granularity (Pentium)
  - Intel 430MX (mobile)    - WB, 4 NC regions, 64KB granularity (Pentium)
  - VIA Apollo VP3          - WB, 4 NC regions, 64KB granularity (Super7)
  - ALi M1541 (Aladdin V)   - WB, 4 NC regions, 64KB granularity (Super7)
  - SiS 5598                - WB, 4 NC regions, 64KB granularity (Super7)
  - SiS 5591                - WB, 4 NC regions, 64KB granularity (Super7)
  - SiS 530                 - WB, 4 NC regions, 64KB granularity (Super7)
  - OPTi 82C596/597 (Viper) - WB, 4 NC regions, 8KB granularity
  - Faraday FE3600          - WT, 3 NC regions, 128KB granularity
  - UMC UM82C491            - WB, 1 NC region, 8KB granularity
  - MIC MIC9391             - WB, 1 NC region + hardware flush!
  - Eteq 82C495WB (Bengal)  - WB, 2 NC regions, 8KB granularity
  - ALi M1489 (Aladdin)     - WB, 2 NC regions, 64KB granularity (Pentium)

B-Tier (Score 7.0-7.9) - Solid but Limited:
  - Intel 82350             - WT, 1 NC region, 64KB granularity (EISA)
  - Intel 82350DT (Mongoose)- WT, 1 NC region, 64KB granularity (EISA)
  - VIA VT82C570 (VP1)      - WT, 2 NC regions, 64KB granularity (Pentium)
  - OPTi 82C381 (Symphony)  - WT, 2 NC regions, 512KB granularity
  - VIA VT82C310 (FlexSet)  - WT, 2 NC regions, 64KB granularity
  - VLSI VL82C311 (SCAMP)   - WT, 1 NC region, 64KB granularity
  - Headland HT12 (G2)      - WT, Steering-based

C-Tier (Score 5.0-6.9) - Boundary Limitation:
  - Contaq 82C596           - WT, Shadow only (486 VLB-era)
  - C&T 82C301 (PEAK/386)   - WT, Boundary-based (top of RAM only)
  - C&T 82C235 (SCAT)       - WT, Boundary-based (top of RAM only)
  - ALi M1209 (FINIS)       - WT, Boundary-based (top of RAM only)

D-Tier (Score <5.0) - Primitive:
  - Forex FRX-386DX         - WT, Boundary, 1MB granularity
  - Suntac ST62C301         - WT, Segment-based

I-Tier (Info-Only) - 286/386SX Era (No Cache Controller):
  286-era chipsets:
  - C&T CS8221 (NEAT)       - Shadow RAM, A20 control (chipset)
  - VLSI VL82C100           - Shadow RAM
  - VLSI VL82C101           - Shadow RAM
  - VLSI VL82C102           - Shadow RAM
  - Headland HT101          - Shadow RAM, DRAM config
  - Headland HT102          - Shadow RAM, DRAM config
  - ALi M1217               - Shadow RAM
  - OPTi 82C212             - Shadow RAM, A20 control (chipset)

  386SX-era chipsets (286-style registers):
  - C&T CS8230 (NEAT-386)   - Shadow RAM, A20 control (chipset)
  - Headland HT18           - Shadow RAM, DRAM config
  - VLSI VL82C320           - Shadow RAM

  EISA chipsets (info-only, pending datasheets):
  - OPTi 82C682             - EISA 486WB chipset (detected, no register docs)
  - OPTi 82C683             - EISA 486AWB chipset (detected, no register docs)
  - OPTi 82C691/696 (Hunter)- EISA 486 chipset (detected, no register docs)
  - OPTi 82C693/6/7         - EISA Pentium WB chipset (detected, no register docs)
  - SiS 85C411/406          - EISA 386/486 chipset (detected, no register docs)
  - VIA VT82C495            - EISA chipset (detected, no register docs)
  - Atmel AT40957/58/59     - EISA 386/486 chipset (stub detection)
  - TI TACT84500            - EISA 486 chipset (stub detection)

  MCA systems (observer-only, hardware-enforced coherency):
  - IBM PS/2 MCA            - All 386+ PS/2 models with Micro Channel bus
                              (Model 70, 80, 90, 95, P70, P75, etc.)
                              MCA uses bus snooping for cache coherency -
                              no software cache management needed.

  NOTE: Info-only chipsets have no software cache control. For 286/386SX era,
        CACHEKIT provides system inventory (Shadow RAM, A20, DRAM config).
        For MCA, hardware handles coherency - detection only.


WRITE-BACK vs WRITE-THROUGH
---------------------------

CRITICAL: Write-Back (WB) caches hold dirty data not yet written to RAM.

  WB Flush Order: FLUSH first, then DISABLE
  WT Flush Order: DISABLE first, then FLUSH (less critical)

Disabling a WB cache BEFORE flushing causes DATA CORRUPTION!

WB Chipsets: OPTi 391, SiS Rabbit/460, UMC 491, MIC 9391, Eteq Bengal


BUILDING
--------

Requirements:
  - Open Watcom C/C++ compiler
  - Set WATCOM environment variable

Build commands:
  wmake              Build CACHEKIT.EXE (TUI, recommended)
  wmake cli          Build CLI utilities (standalone)
  wmake everything   Build all utilities
  wmake clean        Remove build artifacts
  wmake help         Show all targets

Individual targets:
  wmake CACHEKIT.EXE   Combined TUI utility
  wmake CHIPSET.EXE    CLI chipset detection
  wmake NCCONFIG.EXE   CLI NC configuration
  wmake CACHETST.EXE   CLI cache testing

The utilities are built for 8086 compatibility (runs on any x86 CPU)
but require a 386DX or better to access chipset registers.


TECHNICAL NOTES
---------------

Port Conflict Warning:
  Ports 22h/23h/24h may conflict with LPT2 or sound cards on some
  systems. The detection algorithm checks for floating bus conditions
  before probing.

I/O Port Pairs:
  Most chipsets use 22h (Index) / 23h (Data)
  OPTi family uses 22h (Index) / 24h (Data) - CRITICAL difference!

Cache Flush Methods:
  - 486+: WBINVD instruction (native, fast)
  - 386:  Read loop through 2x cache size
  - MIC 9391: Hardware trigger (Index 40h, bit 1)


REFERENCE
---------

Full chipset register documentation:
  docs/design/386-chipset-reference.md


LICENSE
-------

Part of the Abacus FPGA Project.
See project root for license information.

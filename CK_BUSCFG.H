/*============================================================================
 * CK_BUSCFG.H - CACHEKIT Bus Configuration Interface
 *
 * Part of CACHEKIT v3.1
 * Last Updated: 2026-01-13
 *
 * Provides slot/device configuration for EISA and MCA buses:
 *   - EISA: CFG file parsing, Extended CMOS NVM read/write
 *   - MCA: ADF file parsing, POS register read/write, NVRAM save
 *============================================================================*/

#ifndef CK_BUSCFG_H
#define CK_BUSCFG_H

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/* Maximum slots per bus */
#define EISA_MAX_SLOTS      16      /* Slots 0-15 (0 = system board) */
#define MCA_MAX_SLOTS       8       /* Slots 0-7 */
#define ISAPNP_MAX_CARDS    8       /* Max ISA PnP cards (CSN 1-8) */
#define ISAPNP_MAX_LOGDEV   4       /* Max logical devices per card */

/* Maximum resources per slot */
#define MAX_SLOT_IRQS       4       /* Max IRQs per slot */
#define MAX_SLOT_DMAS       4       /* Max DMA channels per slot */
#define MAX_SLOT_IOPORTS    8       /* Max I/O port ranges per slot */
#define MAX_SLOT_MEMRANGES  4       /* Max memory ranges per slot */

/* Configuration file limits */
#define MAX_CFG_CHOICES     16      /* Max choices in CFG/ADF */
#define MAX_CHOICE_NAME     32      /* Max choice name length */

/* Bus configuration result codes */
#define BUSCFG_OK           0
#define BUSCFG_ERR_NOBUS    -1      /* Bus not present */
#define BUSCFG_ERR_SLOT     -2      /* Invalid slot number */
#define BUSCFG_ERR_READ     -3      /* Read error */
#define BUSCFG_ERR_WRITE    -4      /* Write error */
#define BUSCFG_ERR_NOCFG    -5      /* No CFG/ADF file found */
#define BUSCFG_ERR_PARSE    -6      /* Parse error */
#define BUSCFG_ERR_NVM      -7      /* NVM/NVRAM error */
#define BUSCFG_ERR_CONFLICT -8      /* Resource conflict detected */

/* Resource conflict types (bit flags) */
#define CONFLICT_NONE       0x00
#define CONFLICT_IRQ        0x01
#define CONFLICT_DMA        0x02
#define CONFLICT_IOPORT     0x04
#define CONFLICT_MEMORY     0x08

/*============================================================================
 * RESOURCE STRUCTURES
 *============================================================================*/

/* I/O Port range */
typedef struct {
    unsigned int base;              /* Base I/O address */
    unsigned int size;              /* Size in bytes */
} ioport_range_t;

/* Memory range */
typedef struct {
    unsigned long base;             /* Base physical address */
    unsigned long size;             /* Size in bytes */
    unsigned char is_rom;           /* 1 = ROM, 0 = RAM */
    unsigned char is_shared;        /* 1 = shared with other devices */
} mem_range_t;

/* IRQ assignment */
typedef struct {
    unsigned char irq;              /* IRQ number (0-15) */
    unsigned char level_triggered;  /* 1 = level, 0 = edge */
    unsigned char shared;           /* 1 = shareable */
} irq_assign_t;

/* DMA assignment */
typedef struct {
    unsigned char channel;          /* DMA channel (0-7) */
    unsigned char transfer_size;    /* 0=8-bit, 1=16-bit, 2=32-bit */
    unsigned char timing;           /* 0=ISA, 1=TypeA, 2=TypeB, 3=TypeC(burst) */
} dma_assign_t;

/*============================================================================
 * SLOT CONFIGURATION STRUCTURE
 *============================================================================*/

typedef struct {
    /* Identification */
    unsigned char slot;             /* Slot number */
    unsigned char bus_type;         /* BUS_EISA or BUS_MCA */
    unsigned char enabled;          /* 1 = card enabled */

    /* EISA: Compressed vendor + product ID */
    /* MCA: Adapter ID in vendor_id, device_id unused */
    unsigned int vendor_id;
    unsigned int device_id;
    char vendor_str[4];             /* Decoded vendor (EISA only, e.g. "CPQ") */

    /* Board name (from embedded DB or CFG/ADF) */
    char name[48];

    /* Resources */
    irq_assign_t irqs[MAX_SLOT_IRQS];
    unsigned char irq_count;

    dma_assign_t dmas[MAX_SLOT_DMAS];
    unsigned char dma_count;

    ioport_range_t ioports[MAX_SLOT_IOPORTS];
    unsigned char ioport_count;

    mem_range_t mem_ranges[MAX_SLOT_MEMRANGES];
    unsigned char mem_count;

    /* MCA-specific: Raw POS registers */
    unsigned char pos[8];

    /* MCA-specific: Adapter capabilities (detected from POS/ADF) */
    unsigned char mca_bus_master;       /* 1 = bus master capable */
    unsigned char mca_arb_level;        /* Arbitration level (0-15) */
    unsigned char mca_fairness;         /* 1 = fairness enabled */
    unsigned char mca_streaming;        /* 1 = streaming data mode */

    /* EISA-specific: Init data (registers to program at boot) */
    unsigned char eisa_init[64];
    unsigned char eisa_init_len;

    /* EISA-specific: Adapter capabilities */
    unsigned char eisa_bus_master;      /* 1 = bus master capable */
    unsigned char eisa_dma_timing;      /* 0=ISA, 1=TypeA, 2=TypeB, 3=Burst */

    /* ISA PnP-specific: Card identification */
    unsigned char isapnp_csn;           /* Card Select Number (1-8) */
    unsigned char isapnp_logdev;        /* Logical device number (0-3) */
    unsigned long isapnp_vendor;        /* Vendor ID (EISA-style compressed) */
    unsigned int  isapnp_serial;        /* Serial number */
    unsigned char isapnp_checksum;      /* Resource data checksum */

} slot_config_t;

/*============================================================================
 * CONFIGURATION CHOICE STRUCTURE (from CFG/ADF files)
 *============================================================================*/

typedef struct {
    char name[MAX_CHOICE_NAME];     /* Choice description */

    /* Resource options for this choice */
    unsigned char irq_options[8];   /* Available IRQs */
    unsigned char irq_option_count;

    unsigned char dma_options[4];   /* Available DMA channels */
    unsigned char dma_option_count;

    unsigned int io_options[8];     /* Available I/O bases */
    unsigned char io_option_count;

    unsigned long mem_options[4];   /* Available memory bases */
    unsigned char mem_option_count;

    /* MCA: POS register values for this choice */
    unsigned char pos_mask[8];      /* Which bits to modify */
    unsigned char pos_value[8];     /* Values to set */

} config_choice_t;

/*============================================================================
 * EMBEDDED CARD DATABASE ENTRY
 *============================================================================*/

typedef struct {
    unsigned int vendor_id;         /* EISA compressed or MCA adapter ID */
    unsigned int device_id;         /* EISA product ID (0 for MCA) */
    const char *name;               /* Card name */
    unsigned char bus_type;         /* BUS_EISA or BUS_MCA */

    /* Default resources (when no CFG/ADF available) */
    unsigned char default_irq;
    unsigned char default_dma;
    unsigned int default_io;
    unsigned long default_mem;

} card_db_entry_t;

/*============================================================================
 * BUS CONFIGURATION STATE
 *============================================================================*/

/*============================================================================
 * RESOURCE CONFLICT INFORMATION
 *============================================================================*/

typedef struct {
    unsigned char slot_a;           /* First conflicting slot */
    unsigned char slot_b;           /* Second conflicting slot */
    unsigned char conflict_type;    /* CONFLICT_* flags */
    unsigned char resource_idx;     /* Index of conflicting resource */

    /* Details of the conflict */
    union {
        unsigned char irq;          /* Conflicting IRQ number */
        unsigned char dma;          /* Conflicting DMA channel */
        unsigned int io_base;       /* Conflicting I/O base */
        unsigned long mem_base;     /* Conflicting memory base */
    } value;

} resource_conflict_t;

#define MAX_CONFLICTS       16      /* Maximum conflicts to report */

typedef struct {
    unsigned char bus_detected;     /* BUS_EISA, BUS_MCA, or 0 */
    unsigned char slot_count;       /* Number of populated slots */
    slot_config_t slots[EISA_MAX_SLOTS];  /* Use larger of EISA/MCA */

    /* Currently selected slot for editing */
    unsigned char selected_slot;

    /* Dirty flag (config modified but not saved) */
    unsigned char modified;

    /* Auto-config state */
    unsigned char auto_configured;  /* 1 if auto-config has run */

    /* Conflict tracking */
    resource_conflict_t conflicts[MAX_CONFLICTS];
    unsigned char conflict_count;

} buscfg_state_t;

/*============================================================================
 * PUBLIC API - DETECTION AND INITIALIZATION
 *============================================================================*/

/* Initialize bus configuration subsystem
 * Detects EISA or MCA, returns detected bus type (0 if neither) */
int buscfg_init(void);

/* Get current bus configuration state */
buscfg_state_t *buscfg_get_state(void);

/* Check which bus is present (returns BUS_EISA, BUS_MCA, or 0) */
unsigned char buscfg_detect_bus(void);

/*============================================================================
 * PUBLIC API - SLOT ENUMERATION
 *============================================================================*/

/* Enumerate all populated slots
 * Returns number of slots found */
int buscfg_enum_slots(void);

/* Read full configuration for a slot
 * Returns BUSCFG_OK on success */
int buscfg_read_slot(unsigned char slot, slot_config_t *cfg);

/*============================================================================
 * PUBLIC API - SLOT CONFIGURATION
 *============================================================================*/

/* Enable or disable a slot
 * Returns BUSCFG_OK on success */
int buscfg_slot_enable(unsigned char slot, unsigned char enable);

/* Set slot IRQ
 * Returns BUSCFG_OK on success */
int buscfg_set_irq(unsigned char slot, unsigned char irq_idx, unsigned char irq);

/* Set slot DMA channel
 * Returns BUSCFG_OK on success */
int buscfg_set_dma(unsigned char slot, unsigned char dma_idx, unsigned char channel);

/* Set slot I/O base address
 * Returns BUSCFG_OK on success */
int buscfg_set_ioport(unsigned char slot, unsigned char io_idx, unsigned int base);

/* Set slot memory base address
 * Returns BUSCFG_OK on success */
int buscfg_set_membase(unsigned char slot, unsigned char mem_idx, unsigned long base);

/*============================================================================
 * PUBLIC API - MCA POS REGISTER ACCESS
 *============================================================================*/

/* Read raw POS register (MCA only)
 * Returns register value, or 0xFF on error */
unsigned char buscfg_mca_read_pos(unsigned char slot, unsigned char reg);

/* Write raw POS register (MCA only)
 * Returns BUSCFG_OK on success */
int buscfg_mca_write_pos(unsigned char slot, unsigned char reg, unsigned char value);

/*============================================================================
 * PUBLIC API - NVM/NVRAM OPERATIONS
 *============================================================================*/

/* Save configuration to NVM (EISA Extended CMOS or MCA NVRAM)
 * Returns BUSCFG_OK on success */
int buscfg_save_nvm(void);

/* Reload configuration from NVM
 * Returns BUSCFG_OK on success */
int buscfg_load_nvm(void);

/*============================================================================
 * PUBLIC API - CARD DATABASE
 *============================================================================*/

/* Lookup card name from embedded database
 * Returns pointer to name, or NULL if not found */
const char *buscfg_lookup_card(unsigned char bus_type,
                                unsigned int vendor_id,
                                unsigned int device_id);

/*============================================================================
 * PUBLIC API - UTILITY FUNCTIONS
 *============================================================================*/

/* Decode EISA compressed vendor ID to 3-char string */
void buscfg_decode_eisa_vendor(unsigned int compressed, char *vendor_str);

/* Encode EISA 3-char vendor string to compressed format */
unsigned int buscfg_encode_eisa_vendor(const char *vendor_str);

/* Format slot ID string (e.g., "CPQ0001" for EISA, "8EFF" for MCA) */
void buscfg_format_slot_id(const slot_config_t *cfg, char *id_str);

/*============================================================================
 * PUBLIC API - EISA BIOS VENDOR INFORMATION
 *============================================================================*/

/* Get detected EISA BIOS vendor name (for UI display)
 * Returns string like "Compaq", "HP", "AMI", etc. */
const char *buscfg_get_eisa_vendor_name(void);

/* Get detected EISA BIOS vendor type
 * Returns EISA_VENDOR_* constant */
unsigned char buscfg_get_eisa_vendor_type(void);

/*============================================================================
 * PUBLIC API - EISA SYSTEM BOARD INFORMATION
 *============================================================================*/

/* Get detected EISA system board name (for UI display)
 * Returns string like "Compaq ProLiant 2500" or "HP NetServer LH" */
const char *buscfg_get_eisa_sysboard_name(void);

/* Get EISA system board vendor ID (compressed format)
 * Returns 16-bit compressed vendor ID (e.g., 0x110E for "CPQ") */
unsigned int buscfg_get_eisa_sysboard_vendor(void);

/* Get EISA system board product ID
 * Returns 16-bit product ID (e.g., 0x0551 for ProLiant 2500) */
unsigned int buscfg_get_eisa_sysboard_product(void);

/* Get number of EISA slots for this system (from system board database) */
unsigned char buscfg_get_eisa_slot_count(void);

/* EISA BIOS vendor type constants */
#define EISA_VENDOR_UNKNOWN     0
#define EISA_VENDOR_COMPAQ      1   /* Compaq, most clones */
#define EISA_VENDOR_HP          2   /* Hewlett-Packard */
#define EISA_VENDOR_DEC         3   /* Digital Equipment */
#define EISA_VENDOR_AMI         4   /* AMI EISA BIOS */
#define EISA_VENDOR_AWARD       5   /* Award EISA BIOS */
#define EISA_VENDOR_PHOENIX     6   /* Phoenix EISA BIOS */
#define EISA_VENDOR_OLIVETTI    7   /* Olivetti */
#define EISA_VENDOR_NCR         8   /* NCR */
#define EISA_VENDOR_DELL        9   /* Dell */
#define EISA_VENDOR_MICRONICS  10   /* Micronics */
#define EISA_VENDOR_INTEL      11   /* Intel */

/*============================================================================
 * PUBLIC API - MCA/PS/2 MODEL INFORMATION
 *============================================================================*/

/* Get detected MCA system model name (for UI display)
 * Returns string like "PS/2 Model 80 (386-20)" or "NCR System 3000" */
const char *buscfg_get_mca_model_name(void);

/* Get MCA model byte from INT 15h AH=C0h
 * Common values: F8h=Model 70/80/90/95, FCh=Model 30/50/55/60, 9Ah=Model 35/40 */
unsigned char buscfg_get_mca_model_byte(void);

/* Get MCA submodel byte from INT 15h AH=C0h */
unsigned char buscfg_get_mca_submodel_byte(void);

/* Get number of MCA slots for this system (from PS/2 model database) */
unsigned char buscfg_get_mca_slot_count(void);

/* Get MCA bus width (16 or 32 bits)
 * 16-bit MCA: Model 50, 50Z, 55SX, 55LS, 60, 65SX, 35/40 (286/386SX CPU)
 * 32-bit MCA: Model 70, 80, 90, 95 (386DX/486/Pentium CPU)
 * Note: All slots in a system have the same width */
unsigned char buscfg_get_mca_bus_width(void);

/* Get MCA bus speed in MHz (10, 16, 20, 25, or 33)
 * Speed varies by model - see PS/2 Hardware Technical Reference */
unsigned char buscfg_get_mca_bus_speed(void);

/* Check if system is an IBM PS/2 (vs third-party MCA clone)
 * Returns 1 for IBM PS/2, 0 for third-party (NCR, Tandy, Reply, etc.) */
int buscfg_is_ibm_ps2(void);

/*============================================================================
 * PUBLIC API - ISA PLUG AND PLAY
 *============================================================================*/

/* Initialize ISA PnP subsystem
 * Sends initiation key, isolates cards, assigns CSNs
 * Returns number of PnP cards found (0 if none) */
int isapnp_init(void);

/* Enumerate ISA PnP cards
 * Populates card_count and device list
 * Returns number of logical devices found */
int isapnp_enum_devices(void);

/* Get number of ISA PnP cards detected */
unsigned char isapnp_get_card_count(void);

/* Get number of logical devices across all cards */
unsigned char isapnp_get_device_count(void);

/* Read ISA PnP card information
 * CSN = Card Select Number (1-8)
 * Returns BUSCFG_OK on success */
int isapnp_read_card(unsigned char csn, slot_config_t *cfg);

/* Activate/deactivate a logical device
 * CSN = Card Select Number, logdev = logical device index
 * Returns BUSCFG_OK on success */
int isapnp_activate(unsigned char csn, unsigned char logdev, unsigned char activate);

/* Set resources for a logical device
 * Must call isapnp_activate() after setting resources */
int isapnp_set_io(unsigned char csn, unsigned char logdev, unsigned char idx, unsigned int base);
int isapnp_set_irq(unsigned char csn, unsigned char logdev, unsigned char idx, unsigned char irq);
int isapnp_set_dma(unsigned char csn, unsigned char logdev, unsigned char idx, unsigned char dma);
int isapnp_set_mem(unsigned char csn, unsigned char logdev, unsigned char idx, unsigned long base);

/* Format ISA PnP vendor ID as string (e.g., "CTL0001" for Creative) */
void isapnp_format_id(unsigned long vendor_id, unsigned int product_id, char *id_str);

/* Decode ISA PnP vendor ID to 3-character string */
void isapnp_decode_vendor(unsigned long vendor_id, char *vendor_str);

/* Check if ISA PnP cards are present (quick test without full init) */
int isapnp_detect(void);

/*============================================================================
 * PUBLIC API - AUTO-CONFIGURATION
 *============================================================================*/

/* Perform automatic resource configuration for all slots
 * Assigns non-conflicting IRQ, DMA, I/O, and memory resources
 * Returns number of conflicts that could not be resolved */
int buscfg_auto_config(void);

/* Check for resource conflicts across all configured slots
 * Populates conflict array in buscfg_state
 * Returns number of conflicts found */
int buscfg_check_conflicts(void);

/* Check if a specific resource assignment would cause a conflict
 * Returns CONFLICT_* flags for any conflicts detected */
unsigned char buscfg_check_irq_conflict(unsigned char slot, unsigned char irq);
unsigned char buscfg_check_dma_conflict(unsigned char slot, unsigned char dma);
unsigned char buscfg_check_io_conflict(unsigned char slot, unsigned int base, unsigned int size);
unsigned char buscfg_check_mem_conflict(unsigned char slot, unsigned long base, unsigned long size);

/* Get the default resources for a card from the embedded database
 * Returns BUSCFG_OK if defaults found, BUSCFG_ERR_NOCFG otherwise */
int buscfg_get_defaults(unsigned char slot, slot_config_t *defaults);

/* Validate current configuration (call after manual changes)
 * Returns BUSCFG_OK if valid, BUSCFG_ERR_CONFLICT if conflicts exist */
int buscfg_validate_config(void);

/*============================================================================
 * PUBLIC API - RESOURCE AVAILABILITY
 *============================================================================*/

/* Get available (unused) IRQs - returns bitmask of available IRQs */
unsigned int buscfg_get_available_irqs(unsigned char exclude_slot);

/* Get available DMA channels - returns bitmask of available channels */
unsigned char buscfg_get_available_dmas(unsigned char exclude_slot);

/*============================================================================
 * PUBLIC API - REPORT EXPORT
 *============================================================================*/

/* Export bus configuration to text file
 * Creates detailed report with system info, slot config, and conflicts
 * Returns BUSCFG_OK on success */
int buscfg_export_report(const char *filename);

#endif /* CK_BUSCFG_H */

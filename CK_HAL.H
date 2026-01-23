/*============================================================================
 * CK_HAL.H - CACHEKIT Hardware Abstraction Layer Interface
 *
 * Part of CACHEKIT v3.0
 * Last Updated: 2026-01-06 17:08:46 EST
 *
 * This header defines the HAL interface for chipset-independent access to
 * cache control, NC regions, shadow RAM, and A20 gate functionality.
 *============================================================================*/

#ifndef CK_HAL_H
#define CK_HAL_H

/*============================================================================
 * HAL RESULT CODES
 *============================================================================*/

#define HAL_OK          0       /* Operation successful */
#define HAL_ERR_UNSUP  -1       /* Operation not supported by this chipset */
#define HAL_ERR_PARAM  -2       /* Invalid parameter */
#define HAL_ERR_HW     -3       /* Hardware error or timeout */

/*============================================================================
 * CACHE STATE FLAGS
 *============================================================================*/

#define CACHE_DISABLED      0x00
#define CACHE_ENABLED       0x01
#define CACHE_WRITEBACK     0x02    /* Write-back mode active */
#define CACHE_WRITETHROUGH  0x04    /* Write-through mode active */

/*============================================================================
 * SHADOW RAM MODES
 *============================================================================*/

#define SHADOW_DISABLED 0   /* ROM visible, not cached */
#define SHADOW_RO       1   /* RAM shadowed, read-only (cached ROM copy) */
#define SHADOW_RW       2   /* RAM visible, read-write */

/*============================================================================
 * NC REGION STRUCTURE
 *============================================================================*/

typedef struct {
    unsigned long base_kb;      /* Base address in KB */
    unsigned long size_kb;      /* Size in KB */
    unsigned char active;       /* 1 if region is enabled */
} nc_region_t;

/*============================================================================
 * CHIPSET OPERATIONS STRUCTURE
 *
 * Each supported chipset implements this interface. The global g_hal pointer
 * is set during detection to point to the appropriate ops struct.
 *============================================================================*/

typedef struct chipset_ops {
    /*--- Identity ---*/
    const char *name;               /* Full chipset name, e.g. "Intel 430FX Triton" */
    const char *vendor;             /* Vendor name, e.g. "Intel" */
    const char *tier;               /* Quality tier: "S", "A", "B", "C", "D", "I" */
    unsigned char score_x10;        /* Quality score * 10 (945 = 94.5%) */

    /*--- Detection ---*/
    int (*probe)(void);             /* Returns 1 if this chipset is detected */

    /*--- Cache Control ---*/
    int (*cache_get)(void);                     /* Returns CACHE_* flags */
    int (*cache_set)(int enable);               /* Enable/disable, returns HAL_* */
    int (*cache_flush)(void);                   /* Flush write-back cache */
    unsigned char is_writeback;                 /* 1 = write-back capable */

    /*--- NC (Non-Cacheable) Regions ---*/
    unsigned char nc_count;                     /* Number of NC regions (0-4) */
    unsigned char nc_granularity;               /* Minimum size in KB (8, 64, etc) */
    unsigned long nc_max_kb;                    /* Maximum region size in KB */
    int (*nc_read)(int idx, nc_region_t *r);    /* Read region config */
    int (*nc_write)(int idx, unsigned long base_kb, unsigned long size_kb);
    int (*nc_clear)(int idx);                   /* Disable region */

    /*--- Shadow RAM ---*/
    unsigned char shadow_regions;               /* Number of shadow regions (0, 4, 6) */
    int (*shadow_get)(int region);              /* Returns SHADOW_* mode */
    int (*shadow_set)(int region, int mode);    /* Set shadow mode */

    /*--- A20 Gate ---*/
    int (*a20_get)(void);                       /* Returns 0 (disabled) or 1 (enabled) */
    int (*a20_set)(int enable);                 /* Enable/disable A20 line */

    /*--- Raw Register Access (for F4 Registers screen) ---*/
    unsigned char reg_count;                    /* Number of config registers */
    unsigned char reg_base;                     /* First register index */
    int (*reg_read)(int reg);                   /* Read register, -1 if invalid */
    int (*reg_write)(int reg, int val);         /* Write register */

    /*--- Metadata ---*/
    unsigned char info_only;                    /* 1 = read-only chipset (286-era, EISA) */
    unsigned int index_port;                    /* Index port for display (0x22, etc) */
    unsigned int data_port;                     /* Data port for display (0x23, etc) */
} chipset_ops_t;

/*============================================================================
 * GLOBAL HAL POINTER
 *
 * Set by detect_chipset_hal() to point to the detected chipset's ops.
 * Defaults to &ops_unknown if no chipset is detected.
 *============================================================================*/

extern const chipset_ops_t *g_hal;

/*============================================================================
 * FALLBACK OPS FOR UNKNOWN CHIPSETS
 *============================================================================*/

extern const chipset_ops_t ops_unknown;

/*============================================================================
 * HAL DETECTION FUNCTION
 *============================================================================*/

void detect_chipset_hal(void);

/*============================================================================
 * GENERIC HELPER FUNCTIONS
 *
 * These are shared implementations that multiple chipsets can use.
 *============================================================================*/

/* Cache flush using WBINVD instruction (486+) */
int generic_wbinvd_flush(void);

/* Cache flush using INVD instruction (386, no writeback) */
int generic_invd_flush(void);

/* A20 control via Port 0x92 (Fast A20 on AT-compatible systems) */
int generic_port92_a20_get(void);
int generic_port92_a20_set(int enable);

/* Generic "not supported" stubs for ops that aren't implemented */
int hal_stub_unsupported(void);
int hal_stub_unsupported_i(int x);
int hal_stub_unsupported_ii(int x, int y);
int hal_stub_unsupported_iul(int x, unsigned long y);
int hal_stub_unsupported_iull(int x, unsigned long y, unsigned long z);
int hal_stub_nc_read(int idx, nc_region_t *r);

/*============================================================================
 * PCI CONFIGURATION SPACE ACCESS (defined in CK_IO.C)
 *============================================================================*/

/* PCI bus detection */
int pci_bus_present(void);

/* PCI config space read */
unsigned char pci_read_config_byte(unsigned char bus, unsigned char dev,
                                   unsigned char func, unsigned char reg);
unsigned int pci_read_config_word(unsigned char bus, unsigned char dev,
                                  unsigned char func, unsigned char reg);
unsigned long pci_read_config_dword(unsigned char bus, unsigned char dev,
                                    unsigned char func, unsigned char reg);

/* PCI config space write */
void pci_write_config_byte(unsigned char bus, unsigned char dev,
                           unsigned char func, unsigned char reg,
                           unsigned char val);
void pci_write_config_word(unsigned char bus, unsigned char dev,
                           unsigned char func, unsigned char reg,
                           unsigned int val);
void pci_write_config_dword(unsigned char bus, unsigned char dev,
                            unsigned char func, unsigned char reg,
                            unsigned long val);

/*============================================================================
 * LEGACY INDEX/DATA PORT ACCESS (defined in CK_IO.C)
 *============================================================================*/

unsigned char legacy_read_reg(unsigned int index_port, unsigned int data_port,
                              unsigned char reg);
void legacy_write_reg(unsigned int index_port, unsigned int data_port,
                      unsigned char reg, unsigned char val);

/* Standard 0x22/0x23 ports */
unsigned char legacy_read_22_23(unsigned char reg);
void legacy_write_22_23(unsigned char reg, unsigned char val);

/* OPTi 0x22/0x24 ports */
unsigned char legacy_read_22_24(unsigned char reg);
void legacy_write_22_24(unsigned char reg, unsigned char val);

/* VIA 0xA8/0xA9 ports */
unsigned char legacy_read_a8_a9(unsigned char reg);
void legacy_write_a8_a9(unsigned char reg, unsigned char val);

/*============================================================================
 * SAFE PORT ACCESS (defined in CK_IO.C)
 *============================================================================*/

/* Port validation - checks if port pair responds */
int legacy_port_valid(unsigned int index_port, unsigned int data_port);

/* Port probe - checks for floating bus condition */
int safe_port_probe(unsigned int port);

/* Safe read/write with documentation (no actual timeout in DOS real mode) */
unsigned char safe_read_byte(unsigned int port);
void safe_write_byte(unsigned int port, unsigned char val);

/*============================================================================
 * CHIPSET OPS DECLARATIONS (defined in CK_*.C files)
 *============================================================================*/

/* Intel chipsets (CK_INTEL.C) */
extern const chipset_ops_t ops_intel_430fx;
extern const chipset_ops_t ops_intel_430hx;
extern const chipset_ops_t ops_intel_430vx;
extern const chipset_ops_t ops_intel_430tx;
extern const chipset_ops_t ops_intel_430mx;
extern const chipset_ops_t ops_intel_82350dt;
extern const chipset_ops_t ops_intel_420zx;
extern const chipset_ops_t ops_intel_430nx;
extern const chipset_ops_t ops_intel_440fx;
extern const chipset_ops_t ops_intel_450gx;

/* SiS chipsets (CK_SIS.C) */
extern const chipset_ops_t ops_sis_460;
extern const chipset_ops_t ops_sis_rabbit;
extern const chipset_ops_t ops_sis_496;
extern const chipset_ops_t ops_sis_5598;
extern const chipset_ops_t ops_sis_530;
extern const chipset_ops_t ops_sis_5591;
extern const chipset_ops_t ops_sis_eisa;

/* OPTi chipsets (CK_OPTI.C) */
extern const chipset_ops_t ops_opti_391;
extern const chipset_ops_t ops_opti_381;
extern const chipset_ops_t ops_opti_viper;
extern const chipset_ops_t ops_opti_212;
extern const chipset_ops_t ops_opti_682;
extern const chipset_ops_t ops_opti_683;
extern const chipset_ops_t ops_opti_hunter;
extern const chipset_ops_t ops_opti_pent_eisa;

/* VIA chipsets (CK_VIA.C) */
extern const chipset_ops_t ops_via_vt82c495;
extern const chipset_ops_t ops_via_vp1;
extern const chipset_ops_t ops_via_vp3;
extern const chipset_ops_t ops_via_mvp3;

/* ALi chipsets (CK_ALI.C) */
extern const chipset_ops_t ops_ali_aladdin4;
extern const chipset_ops_t ops_ali_aladdin5;

/* Legacy chipsets (CK_LEGAC.C) */
extern const chipset_ops_t ops_ct_neat;
extern const chipset_ops_t ops_ct_neat386;
extern const chipset_ops_t ops_ct_peak;
extern const chipset_ops_t ops_ct_scat;
extern const chipset_ops_t ops_headland_ht12;
extern const chipset_ops_t ops_headland_ht18;
extern const chipset_ops_t ops_headland_ht101;
extern const chipset_ops_t ops_headland_ht102;
extern const chipset_ops_t ops_vlsi_vl82c100;
extern const chipset_ops_t ops_vlsi_vl82c311;
extern const chipset_ops_t ops_vlsi_vl82c320;
extern const chipset_ops_t ops_via_vt82c310;
extern const chipset_ops_t ops_ali_finis;
extern const chipset_ops_t ops_ali_m1217;

/* Other chipsets (CK_OTHER.C) */
extern const chipset_ops_t ops_umc_491;
extern const chipset_ops_t ops_eteq_bengal;
extern const chipset_ops_t ops_faraday;
extern const chipset_ops_t ops_mic_9391;
extern const chipset_ops_t ops_contaq_596;
extern const chipset_ops_t ops_forex;
extern const chipset_ops_t ops_suntac;
extern const chipset_ops_t ops_mca_generic;

#endif /* CK_HAL_H */

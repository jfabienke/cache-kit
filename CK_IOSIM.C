/*============================================================================
 * CK_IOSIM.C - Host-only I/O simulation for off-target unit tests
 *
 * Substitutes for CK_IO.C on a host (clang/gcc) build: a simulated 0x22/0x23
 * index/data register file + port 0x92, plus the legacy/generic/stub helpers
 * the chipset implementations call. This lets the real CK_LEGAC.C detection,
 * shadow, and A20 logic run under a unit test without DOS hardware.
 *
 * Entirely guarded out of the Watcom DOS build (CK_IO.C provides the real I/O).
 *
 * Host build:  cc -std=c89 CK_LEGAC.C CK_IOSIM.C test_neat.c -o test_neat
 *============================================================================*/

#if !defined(__WATCOMC__)

#include "CK_HAL.H"
#include "CK_IOSIM.H"

/*--- simulated port state -------------------------------------------------*/

static unsigned char sim_regs[256];   /* 0x22/0x23 index/data register file */
static unsigned char sim_idx;         /* last index written to 0x22         */
static unsigned char sim_p92;         /* port 0x92                          */
static int           sim_win_lo = 0x00; /* readable data-port window low     */
static int           sim_win_hi = 0xFF; /* readable data-port window high    */

unsigned char io_read_byte(unsigned int port)
{
    if (port == 0x22) return sim_idx;   /* index latch reads back (real NEAT) */
    if (port == 0x23) {
        if ((int) sim_idx >= sim_win_lo && (int) sim_idx <= sim_win_hi)
            return sim_regs[sim_idx];
        return 0xFF;                    /* index outside decoded window -> float */
    }
    if (port == 0x92) return sim_p92;
    return 0xFF;                        /* unmapped -> floating bus */
}

void io_write_byte(unsigned int port, unsigned char val)
{
    if (port == 0x22)      sim_idx = val;
    else if (port == 0x23) sim_regs[sim_idx] = val;
    else if (port == 0x92) sim_p92 = val;
}

/*--- legacy index/data helpers (mirror CK_IO.C) ---------------------------*/

unsigned char legacy_read_reg(unsigned int index_port, unsigned int data_port,
                              unsigned char reg)
{
    io_write_byte(index_port, reg);
    return io_read_byte(data_port);
}

void legacy_write_reg(unsigned int index_port, unsigned int data_port,
                      unsigned char reg, unsigned char val)
{
    io_write_byte(index_port, reg);
    io_write_byte(data_port, val);
}

unsigned char legacy_read_22_23(unsigned char reg)
{
    return legacy_read_reg(0x22, 0x23, reg);
}

void legacy_write_22_23(unsigned char reg, unsigned char val)
{
    legacy_write_reg(0x22, 0x23, reg, val);
}

int legacy_port_valid(unsigned int index_port, unsigned int data_port)
{
    unsigned char val1, val2;
    io_write_byte(index_port, 0x00);
    val1 = io_read_byte(data_port);
    val2 = io_read_byte(data_port);
    if (val1 != val2) return 0;
    if (val1 == 0xFF) return 0;
    return 1;
}

/*--- generic helpers ------------------------------------------------------*/

int generic_invd_flush(void)  { return HAL_OK; }
int generic_wbinvd_flush(void) { return HAL_OK; }

int generic_port92_a20_get(void)
{
    return (io_read_byte(0x92) & 0x02) ? 1 : 0;
}

int generic_port92_a20_set(int enable)
{
    unsigned char val = io_read_byte(0x92);
    val &= ~0x01;                          /* never set the reset bit */
    if (enable) val |= 0x02; else val &= ~0x02;
    io_write_byte(0x92, val);
    return HAL_OK;
}

/*--- unsupported-op stubs -------------------------------------------------*/

int hal_stub_unsupported(void)                 { return HAL_ERR_UNSUP; }
int hal_stub_unsupported_i(int x)              { (void)x; return HAL_ERR_UNSUP; }
int hal_stub_unsupported_ii(int x, int y)      { (void)x; (void)y; return HAL_ERR_UNSUP; }
int hal_stub_unsupported_iul(int x, unsigned long y)
                                               { (void)x; (void)y; return HAL_ERR_UNSUP; }
int hal_stub_unsupported_iull(int x, unsigned long y, unsigned long z)
                                               { (void)x; (void)y; (void)z; return HAL_ERR_UNSUP; }
int hal_stub_nc_read(int idx, nc_region_t *r)
{
    (void)idx;
    if (r) { r->base_kb = 0; r->size_kb = 0; r->active = 0; }
    return HAL_ERR_UNSUP;
}

/*--- test controls --------------------------------------------------------*/

void          cksim_reset(void)
{
    int i;
    for (i = 0; i < 256; i++) sim_regs[i] = 0;
    sim_idx = 0; sim_p92 = 0;
    sim_win_lo = 0x00; sim_win_hi = 0xFF;
}
void          cksim_set_reg(unsigned char reg, unsigned char val) { sim_regs[reg] = val; }
unsigned char cksim_get_reg(unsigned char reg)                    { return sim_regs[reg]; }
void          cksim_set_p92(unsigned char val)                    { sim_p92 = val; }
unsigned char cksim_get_p92(void)                                 { return sim_p92; }
void          cksim_set_reg_window(int lo, int hi)                { sim_win_lo = lo; sim_win_hi = hi; }

#endif /* !__WATCOMC__ */

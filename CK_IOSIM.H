/*============================================================================
 * CK_IOSIM.H - Host-only I/O simulation hooks (off-target unit tests)
 *
 * Declares the test-side controls for CK_IOSIM.C, which substitutes for
 * CK_IO.C on a host (clang/gcc) build so chipset detection / shadow / A20
 * logic can be unit-tested without DOS hardware. Not used in the Watcom build.
 *============================================================================*/

#ifndef CK_IOSIM_H
#define CK_IOSIM_H

#if !defined(__WATCOMC__)

void          cksim_reset(void);                                  /* clear regs + window */
void          cksim_set_reg(unsigned char reg, unsigned char val);
unsigned char cksim_get_reg(unsigned char reg);
void          cksim_set_p92(unsigned char val);
unsigned char cksim_get_p92(void);
/* Restrict the readable data-port window to [lo,hi]; real NEAT decodes only
 * index 0x60-0x6F, out-of-window reads float to 0xFF. Default 0x00-0xFF. */
void          cksim_set_reg_window(int lo, int hi);

#endif /* !__WATCOMC__ */

#endif /* CK_IOSIM_H */

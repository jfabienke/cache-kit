# Open Watcom C Compiler Notes for DOS Development

**Last Updated:** 2026-01-23 18:13:29 CET

This document captures compiler findings and best practices discovered while building CACHEKIT with Open Watcom v2 cross-compiled from macOS Apple Silicon.

## Build Environment

| Item | Details |
|------|---------|
| Host Platform | macOS Apple Silicon (ARM64) |
| Host Compiler | Clang 17.0.0 |
| Cross Compiler | Open Watcom v2 (wcc 16-bit, wcc386 32-bit) |
| Target | DOS 16-bit (8086/compatible) |
| Compiler Flags | `-0 -mm -s -ox -w4 -zq` |

### Compiler Flag Reference

| Flag | Description |
|------|-------------|
| `-0` | Generate 8086 compatible code |
| `-3` | Generate 386+ code (faster, requires 386) |
| `-ms` | Small memory model (64KB code, 64KB data) |
| `-mm` | Medium memory model (64KB data, multiple 64KB code segments) |
| `-ml` | Large memory model (multiple 64KB code and data segments) |
| `-s` | Disable stack checking |
| `-ox` | Maximum optimization |
| `-w4` | Warning level 4 (most warnings) |
| `-zq` | Quiet mode (suppress banner) |

## Memory Model Selection

For larger projects (>64KB code), use medium or large memory model:

| Model | Code | Data | Library | Use Case |
|-------|------|------|---------|----------|
| `-ms` (small) | 64KB | 64KB | clibs.lib, maths.lib | Simple utilities |
| `-mm` (medium) | Multiple 64KB segs | 64KB | clibm.lib, mathm.lib | Large code, small data |
| `-ml` (large) | Multiple 64KB segs | Multiple 64KB segs | clibl.lib, mathl.lib | Large code and data |

**CACHEKIT uses medium model (-mm)** because the combined code exceeds 64KB but data fits in a single segment.

## C89/C90 Compliance Requirements

Open Watcom C defaults to **strict C89/C90** mode. This affects:

### 1. Variable Declarations

**All variable declarations must be at the beginning of a block** (before any statements).

```c
/* WRONG - C99 style (fails in Open Watcom) */
void function(void)
{
    int x = 10;
    x++;
    int y = 20;  /* ERROR: declaration after statement */
}

/* CORRECT - C89 style */
void function(void)
{
    int x;
    int y;

    x = 10;
    x++;
    y = 20;
}
```

### 2. For Loop Declarations

```c
/* WRONG - C99 style */
for (int i = 0; i < 10; i++) { }

/* CORRECT - C89 style */
int i;
for (i = 0; i < 10; i++) { }
```

### 3. Mixed Declarations in Switch Cases

Each case that declares variables needs its own block:

```c
/* WRONG */
switch (x) {
    case 1:
        int value = get_value();  /* ERROR */
        break;
}

/* CORRECT */
switch (x) {
    case 1:
        {
            int value;
            value = get_value();
        }
        break;
}
```

## Inline Assembly

### CPU Mode Directives

When using 386+ instructions in 8086-targeted code, you must specify the CPU mode:

```c
/* For 486+ instructions (wbinvd) */
_asm {
    .486
    wbinvd
}

/* For 386 instructions (pushfd, eax, etc.) */
_asm {
    .386
    pushfd
    pop eax
    mov ecx, eax
    /* ... */
}
```

### pragma aux Syntax

For function-like assembly, use `#pragma aux`:

```c
/* Define inline assembly function */
#pragma aux do_wbinvd = \
    ".486" \
    "wbinvd" \
    modify exact [];
extern void do_wbinvd(void);

/* Call it */
do_wbinvd();
```

**Note:** The `.386` or `.486` directive must be first in the assembly string.

### Register Clobbering

Always declare which registers are modified:

```c
#pragma aux my_func = \
    "mov ax, 1234h" \
    "int 21h" \
    modify [ax bx];  /* List modified registers */
```

### 32-bit I/O in 16-bit Mode

The 16-bit `conio.h` only provides `inp()`/`outp()` (byte) and `inpw()`/`outpw()` (word). For 32-bit port I/O (needed for PCI configuration), implement with inline assembly:

```c
unsigned long io_read_dword(unsigned int port)
{
    unsigned long val;
    _asm {
        .386
        mov dx, port
        in eax, dx
        mov word ptr val, ax
        shr eax, 16
        mov word ptr val+2, ax
    }
    return val;
}

void io_write_dword(unsigned int port, unsigned long val)
{
    _asm {
        .386
        mov ax, word ptr val
        mov dx, ax
        mov ax, word ptr val+2
        shl eax, 16
        mov ax, dx
        mov dx, port
        out dx, eax
    }
}
```

**Note:** The `.386` directive is required to enable 32-bit registers (EAX, etc.) in 16-bit code.

### Raw Opcode Encoding

For instructions that don't work well with Watcom's assembler, use raw opcodes:

```c
/* WBINVD - Write Back and Invalidate Cache (486+) */
_asm {
    db 0Fh, 09h     /* Opcode bytes for WBINVD */
}

/* INVD - Invalidate Cache without writeback (386+) */
_asm {
    db 0Fh, 08h     /* Opcode bytes for INVD */
}
```

## Common Issues and Solutions

### 1. Forward Reference Errors

Static functions and variables must be declared before use:

```c
/* Solution: Add forward declarations */
static int helper_function(int x);
static app_state_t g_state;

/* ... later in file ... */
static int helper_function(int x)
{
    return x * 2;
}
```

### 2. Symbol Conflicts with Headers

When using HAL headers that declare extern functions, don't redefine them as static:

```c
/* Header declares: */
unsigned char pci_read_config_byte(unsigned char bus, ...);

/* WRONG - conflicts with header */
static unsigned char pci_read_config_byte(unsigned char bus, ...);

/* CORRECT - use conditional compilation */
#ifndef CK_HAL_H
static unsigned char pci_read_config_byte(unsigned char bus, ...);
#endif
```

### 3. DOS-Specific Headers

These headers are DOS/Watcom specific and won't compile with modern clang:
- `<conio.h>` - Console I/O
- `<dos.h>` - DOS system calls
- `<i86.h>` - x86 intrinsics

Expect clang-based LSP diagnostics to show errors for these - ignore them.

### 4. Library Names

DOS library naming conventions by memory model:

| Model | C Library | Math Library |
|-------|-----------|--------------|
| Small (-ms) | clibs.lib | maths.lib |
| Medium (-mm) | clibm.lib | mathm.lib |
| Compact (-mc) | clibc.lib | mathc.lib |
| Large (-ml) | clibl.lib | mathl.lib |

Libraries are in `$WATCOM/lib286/dos/`

## macOS Cross-Compilation Notes

### Environment Setup

```bash
export WATCOM="/path/to/open-watcom-v2/rel"
export PATH="$PATH:$WATCOM/armo64"
export INCLUDE="$WATCOM/h"
export LIB="$WATCOM/lib286/dos:$WATCOM/lib286"
```

### Build Script vs Makefile

- **DOS builds:** Use `wmake` with the Makefile
- **macOS builds:** Use `build_macos.sh` script

The Makefile uses DOS-specific inference rules (`.c.obj:`) that may not work identically on macOS wmake.

### Linker Differences

macOS wlink requires explicit library paths. Example for medium model:
```bash
wlink option quiet option stack=8192 \
    libpath "$WATCOM/lib286/dos" \
    libpath "$WATCOM/lib286" \
    library clibm.lib library mathm.lib \
    format dos name output.exe file input.obj
```

For small model, use `clibs.lib` and `maths.lib` instead.

## File Extension Conventions

| Extension | Purpose |
|-----------|---------|
| `.C` | C source file (uppercase for DOS) |
| `.H` | Header file (uppercase for DOS) |
| `.OBJ` | Object file (DOS convention) |
| `.EXE` | Executable |
| `.MAP` | Linker map file |
| `.ERR` | Error output |

## Debugging Tips

1. **Get verbose errors:** Remove `-zq` flag to see full compiler output
2. **Check line numbers:** Error line numbers may be offset from actual issue (especially with inline asm)
3. **Test with DOS wmake:** Some issues only appear on the target platform
4. **Use DOSBox:** Test compiled executables in DOSBox emulator

## References

- [Open Watcom Documentation](https://open-watcom.github.io/)
- [Watcom C/C++ User's Guide](https://open-watcom.github.io/open-watcom-v2-wikidocs/cguide.html)
- [Watcom Inline Assembler](https://open-watcom.github.io/open-watcom-v2-wikidocs/cguide.html#InlineAssembly)

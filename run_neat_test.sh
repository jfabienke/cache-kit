#!/bin/sh
# run_neat_test.sh - Off-target unit test for the NEAT (CS8221) chipset ops.
#
# Compiles the REAL CK_LEGAC.C detection/shadow/A20 logic against the
# CK_IOSIM.C host I/O simulation (no DOS or Watcom needed) and runs test_neat.c.
# Intended for CI / quick local verification. Exit code is the test result.
#
# Usage:  sh run_neat_test.sh        (override compiler with CC=gcc, etc.)

CC="${CC:-cc}"
OUT="$(mktemp -t ck_neat_test.XXXXXX)" || exit 1

# Uppercase .C files would be taken as C++; force C with -x c.
if ! "$CC" -x c -std=c89 -Wall -Wextra -o "$OUT" CK_LEGAC.C CK_IOSIM.C test_neat.c; then
    echo "run_neat_test: build failed"
    rm -f "$OUT"
    exit 1
fi

"$OUT"
rc=$?
rm -f "$OUT"
exit $rc

#!/usr/bin/env bash
# Compile and run the event bus host gates with gcc. No hardware, no ESP-IDF; runnable from anywhere.
# Non-zero exit on any test failure or compiler warning (-Werror). This is what CI runs.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

CC="${CC:-gcc}"
flags=(-std=c11 -Wall -Wextra -Werror -Iinclude)
srcs=(src/pool.c src/dispatch.c src/isr_pool.c)

rc=0
for t in test/pool_exhaustion.c test/dispatch.c test/release.c test/cascade.c test/isr.c; do
    bin="$(mktemp)"
    echo ">>> $(basename "$t")"
    "$CC" "${flags[@]}" "${srcs[@]}" "$t" -o "$bin"
    "$bin" || rc=1
    rm -f "$bin"
    echo ""
done

if [ "$rc" -eq 0 ]; then
    echo "ALL EVENT BUS GATES OK"
fi
exit "$rc"

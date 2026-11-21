#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:-build/arm64/syntraos_arm64.elf}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
GDB_BIN="${GDB_BIN:-gdb-multiarch}"
GDB_PORT="1234"

if [[ ! -f "$ELF_PATH" ]]; then
    echo "ARM64 ELF not found: $ELF_PATH" >&2
    exit 1
fi

command -v "$QEMU_BIN" >/dev/null 2>&1 || {
    echo "QEMU executable not found: $QEMU_BIN" >&2
    exit 1
}
command -v "$GDB_BIN" >/dev/null 2>&1 || {
    echo "GDB executable not found: $GDB_BIN" >&2
    exit 1
}

QEMU_LOG="$(mktemp)"
UART_LOG="$(mktemp)"
QEMU_PID=""

cleanup() {
    if [[ -n "$QEMU_PID" ]]; then
        kill "$QEMU_PID" >/dev/null 2>&1 || true
        wait "$QEMU_PID" >/dev/null 2>&1 || true
    fi
    rm -f "$QEMU_LOG" "$UART_LOG"
}
trap cleanup EXIT

"$QEMU_BIN" \
    -machine virt,gic-version=3,virtualization=off,secure=off \
    -cpu cortex-a57 \
    -m 512M \
    -nic none \
    -kernel "$ELF_PATH" \
    -display none \
    -serial "file:$UART_LOG" \
    -monitor none \
    -no-reboot \
    -no-shutdown \
    -S \
    -gdb "tcp:127.0.0.1:$GDB_PORT" \
    >"$QEMU_LOG" 2>&1 &
QEMU_PID=$!

sleep 1
if ! kill -0 "$QEMU_PID" >/dev/null 2>&1; then
    echo "QEMU exited before GDB could connect:" >&2
    cat "$QEMU_LOG" >&2
    exit 1
fi

echo "Waiting for SyntraOS boot stage to return from QEMU virt..."

GDB_OUTPUT="$(timeout 15s "$GDB_BIN" -q -batch "$ELF_PATH" \
    -ex "set architecture aarch64" \
    -ex "set pagination off" \
    -ex "set confirm off" \
    -ex "target remote 127.0.0.1:$GDB_PORT" \
    -ex "hbreak *0x10000000" \
    -ex "continue" \
    -ex 'printf "EL0_ENTRY_STOP PC=%#lx\\n", $pc' \
    -ex "break syntra_arm64_exception_handle_and_select" \
    -ex 'condition 2 *(unsigned long*)($x0+288)==8 && (((*(unsigned long*)($x0+272)>>26)&0x3f)==0x15) && ((*(unsigned long*)($x0+264)&0xf)==0) && ((*(unsigned long*)($x0+272)&0xffff)==0x44)' \
    -ex "continue" \
    -ex 'printf "EL0_SVC_STOP vector=%lu ESR=%#lx ELR=%#lx FAR=%#lx SPSR=%#lx SVC_IMM=%#lx\\n", *(unsigned long*)($x0+288), *(unsigned long*)($x0+272), *(unsigned long*)($x0+256), *(unsigned long*)($x0+280), *(unsigned long*)($x0+264), (*(unsigned long*)($x0+272)&0xffff)' \
endToken" || { \
        printf '%s\n' "$GDB_OUTPUT" >&2; \
        echo "GDB diagnostic breakpoint did not complete (timeout or remote error)." >&2; \
        echo "Continuing with UART smoke-test checks." >&2; \
    }
printf '%s\n' "$GDB_OUTPUT"

echo "Waiting for EL0 UART completion markers..."
for _ in $(seq 1 100); do
    if grep -Fq "Hello from EL0" "$UART_LOG" && grep -Fq "EL0 test returned to EL1: PASS" "$UART_LOG"; then
        break
    fi
    sleep 0.1
done

echo "QEMU UART output:"
cat "$UART_LOG"

if ! grep -Fq 'PASS: ARM64 Scheduler switched between two threads' "$UART_LOG"; then
    echo "ARM64 scheduler context-switch marker was not observed on UART." >&2
    exit 1
fi

if ! grep -Fq 'EL0_ENTRY_STOP PC=0x10000000' <<<"$GDB_OUTPUT"; then
    echo "GDB did not stop at EL0 entry PC 0x10000000." >&2
    exit 1
fi

if ! grep -Fq 'Hello from EL0' "$UART_LOG"; then
    echo "First EL0 process did not print its UART hello marker." >&2
    exit 1
fi

if ! grep -Fq 'EL0 test returned to EL1: PASS' "$UART_LOG"; then
    echo "EL0 process did not return safely to EL1." >&2
    exit 1
fi

echo "QEMU ARM64 boot smoke test passed."

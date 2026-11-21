#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:-build/arm64/syntraos_arm64.elf}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-20}"
LOG_DIR="${LOG_DIR:-build/arm64/el0-no-gdb-logs}"
UART_LOG="$LOG_DIR/uart.log"
QEMU_LOG="$LOG_DIR/qemu.stderr.log"

if [[ ! -f "$ELF_PATH" ]]; then
    echo "FAIL: ARM64 ELF not found: $ELF_PATH" >&2
    exit 1
fi

command -v "$QEMU_BIN" >/dev/null 2>&1 || {
    echo "FAIL: QEMU executable not found: $QEMU_BIN" >&2
    exit 1
}
command -v timeout >/dev/null 2>&1 || {
    echo "FAIL: timeout command is required" >&2
    exit 1
}

mkdir -p "$LOG_DIR"
: > "$UART_LOG"
: > "$QEMU_LOG"

echo "Running SyntraOS on QEMU without GDB; timeout=${TIMEOUT_SECONDS}s"
set +e
timeout --signal=TERM --kill-after=2 "${TIMEOUT_SECONDS}s" "$QEMU_BIN" \
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
    >"$QEMU_LOG" 2>&1
QEMU_STATUS=$?
set -e

echo "QEMU exit status: $QEMU_STATUS"
echo "=== QEMU stderr ==="
cat "$QEMU_LOG"
echo "=== UART output ==="
cat "$UART_LOG"

check_marker() {
    local marker="$1"
    if grep -Fq "$marker" "$UART_LOG"; then
        echo "PASS: observed UART marker: $marker"
    else
        echo "FAIL: missing UART marker: $marker" >&2
        return 1
    fi
}

FAILURES=0
check_marker "Hello from EL0" || FAILURES=1
check_marker "EL0 exit SVC: returning to EL1" || FAILURES=1
check_marker "EL0 test returned to EL1: PASS" || FAILURES=1

if [[ "$FAILURES" -ne 0 ]]; then
    echo "FAIL: EL0 no-GDB QEMU test did not observe every required marker." >&2
    echo "Diagnostic logs are retained in: $LOG_DIR" >&2
    exit 1
fi

echo "PASS: EL0 entered, handled both SVC calls, returned to EL1, and emitted the final UART marker without GDB."

#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:-build/arm64/syntraos_arm64.elf}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-20}"
LOG_DIR="${LOG_DIR:-build/arm64/el0-no-gdb-logs}"
UART_LOG="$LOG_DIR/uart.log"
QEMU_LOG="$LOG_DIR/qemu.stderr.log"
QEMU_INT_LOG="$LOG_DIR/qemu-int.log"

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
: > "$QEMU_INT_LOG"

echo "Running SyntraOS on QEMU without GDB; timeout=${TIMEOUT_SECONDS}s"
echo "QEMU exception tracing enabled: -d int -D $QEMU_INT_LOG"
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
    -d int \
    -D "$QEMU_INT_LOG" \
    >"$QEMU_LOG" 2>&1
QEMU_STATUS=$?
set -e

echo "QEMU exit status: $QEMU_STATUS"
echo "=== QEMU stderr ==="
cat "$QEMU_LOG"
echo "=== UART output ==="
cat "$UART_LOG"
echo "=== QEMU exception trace (last 250 lines) ==="
tail -n 250 "$QEMU_INT_LOG" || true

# Resolve the first EL1 Data Abort from QEMU's architectural trace.
FAULT_ELR="$(awk '
    /Taking exception 4/ && /Data Abort/ { in_abort=1; next }
    in_abort && index($0, "...with ELR ") {
        sub(/^.*with ELR /, "")
        print
        exit
    }
' "$QEMU_INT_LOG" | tr -d '\r')"
FAULT_FAR="$(awk '
    /Taking exception 4/ && /Data Abort/ { in_abort=1; next }
    in_abort && index($0, "...with FAR ") {
        sub(/^.*with FAR /, "")
        print
        exit
    }
' "$QEMU_INT_LOG" | tr -d '\r')"

if [[ -n "$FAULT_ELR" ]]; then
    echo "=== First EL1 Data Abort address resolution ==="
    echo "ELR=$FAULT_ELR FAR=${FAULT_FAR:-unknown}"
    if command -v aarch64-linux-gnu-addr2line >/dev/null 2>&1; then
        aarch64-linux-gnu-addr2line -e "$ELF_PATH" -f -C "$FAULT_ELR" ${FAULT_FAR:+"$FAULT_FAR"} || true
    else
        echo "addr2line is unavailable on this runner."
    fi
    if command -v aarch64-linux-gnu-objdump >/dev/null 2>&1; then
        FAULT_DECIMAL=$((FAULT_ELR))
        START_ADDR=$(printf '0x%x' "$((FAULT_DECIMAL > 32 ? FAULT_DECIMAL - 32 : 0))")
        END_ADDR=$(printf '0x%x' "$((FAULT_DECIMAL + 48))")
        aarch64-linux-gnu-objdump -d --start-address="$START_ADDR" --stop-address="$END_ADDR" "$ELF_PATH" || true
    fi
else
    echo "No EL1 Data Abort ELR found in QEMU exception trace."
fi

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
    echo "Full UART, QEMU stderr, and exception trace are retained in: $LOG_DIR" >&2
    exit 1
fi

echo "PASS: EL0 entered, handled both SVC calls, returned to EL1, and emitted the final UART marker without GDB."

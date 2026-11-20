#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:-build/arm64/syntraos_arm64.elf}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-15}"

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

LOG_FILE="$(mktemp)"
trap 'rm -f "$LOG_FILE"' EXIT

echo "Running SyntraOS on QEMU virt ARM64; timeout=${TIMEOUT_SECONDS}s"
set +e
timeout "${TIMEOUT_SECONDS}s" "$QEMU_BIN" \
    -machine virt,gic-version=3,virtualization=off,secure=off \
    -cpu cortex-a57 \
    -m 512M \
    -kernel "$ELF_PATH" \
    -display none \
    -serial stdio \
    -monitor none \
    -no-reboot \
    -no-shutdown >"$LOG_FILE" 2>&1
QEMU_STATUS=$?
set -e

cat "$LOG_FILE"

if grep -Fq "SyntraOS boot OK" "$LOG_FILE"; then
    echo "PASS: UART PL011 emitted 'SyntraOS boot OK' from the running kernel."
    exit 0
fi

if [[ "$QEMU_STATUS" -eq 124 ]]; then
    echo "FAIL: QEMU timed out after ${TIMEOUT_SECONDS}s without the UART success marker." >&2
else
    echo "FAIL: QEMU exited with status $QEMU_STATUS without the UART success marker." >&2
fi
exit 1

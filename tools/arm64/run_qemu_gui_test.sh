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

echo "Running SyntraOS with QEMU ramfb attached; timeout=${TIMEOUT_SECONDS}s"
set +e
timeout "${TIMEOUT_SECONDS}s" "$QEMU_BIN" \
    -machine virt,gic-version=3,virtualization=off,secure=off \
    -cpu cortex-a57 \
    -m 512M \
    -nic none \
    -kernel "$ELF_PATH" \
    -device ramfb \
    -display none \
    -serial stdio \
    -monitor none \
    -no-reboot \
    -no-shutdown >"$LOG_FILE" 2>&1
QEMU_STATUS=$?
set -e

cat "$LOG_FILE"

if grep -Fq "QEMU ramfb: READY 640x480 XRGB8888" "$LOG_FILE" &&
   grep -Fq "SyntraOS boot OK" "$LOG_FILE"; then
    echo "PASS: QEMU ramfb accepted the framebuffer configuration and SyntraOS completed boot."
    exit 0
fi

if [[ "$QEMU_STATUS" -eq 124 ]]; then
    echo "FAIL: QEMU timed out after ${TIMEOUT_SECONDS}s without the ramfb success marker." >&2
else
    echo "FAIL: QEMU exited with status $QEMU_STATUS without the ramfb success marker." >&2
fi
exit 1

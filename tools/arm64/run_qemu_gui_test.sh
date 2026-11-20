#!/usr/bin/env bash
set -euo pipefail

ELF_PATH="${1:-build/arm64/syntraos_arm64.elf}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-30}"
SCREENSHOT_PATH="${SCREENSHOT_PATH:-build/arm64/syntraos-desktop.ppm}"

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
command -v python3 >/dev/null 2>&1 || {
    echo "FAIL: python3 is required for screenshot verification" >&2
    exit 1
}

TMP_DIR="$(mktemp -d)"
MONITOR_SOCKET="$TMP_DIR/qemu-monitor.sock"
SERIAL_LOG="$TMP_DIR/serial.log"
QEMU_PID=""

cleanup() {
    if [[ -n "$QEMU_PID" ]] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
    fi
    if [[ -f "$SERIAL_LOG" ]]; then
        if [[ "${KEEP_QEMU_LOG:-0}" == "1" ]]; then
            cat "$SERIAL_LOG"
        fi
    fi
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

mkdir -p "$(dirname "$SCREENSHOT_PATH")"
rm -f "$SCREENSHOT_PATH"

echo "Starting QEMU ramfb desktop screenshot test; timeout=${TIMEOUT_SECONDS}s"
timeout --signal=TERM --kill-after=2 "${TIMEOUT_SECONDS}s" "$QEMU_BIN" \
    -machine virt,gic-version=3,virtualization=off,secure=off \
    -cpu cortex-a57 \
    -m 512M \
    -nic none \
    -kernel "$ELF_PATH" \
    -device ramfb \
    -display none \
    -serial "file:$SERIAL_LOG" \
    -monitor "unix:$MONITOR_SOCKET,server=on,wait=off" \
    -no-reboot \
    -no-shutdown &
QEMU_PID=$!

BOOT_READY=0
for _ in $(seq 1 300); do
    if grep -Fq "QEMU ramfb: READY 640x480 XRGB8888" "$SERIAL_LOG" 2>/dev/null; then
        BOOT_READY=1
        break
    fi
    if ! kill -0 "$QEMU_PID" 2>/dev/null; then
        echo "FAIL: QEMU exited before ramfb became ready." >&2
        cat "$SERIAL_LOG" 2>/dev/null || true
        exit 1
    fi
    sleep 0.1
done

if [[ "$BOOT_READY" -ne 1 ]]; then
    echo "FAIL: QEMU ramfb did not become ready within 30 seconds." >&2
    cat "$SERIAL_LOG" 2>/dev/null || true
    exit 1
fi

python3 tools/arm64/verify_qemu_screenshot.py \
    "$MONITOR_SOCKET" "$SCREENSHOT_PATH"

if ! grep -Fq "SyntraOS boot OK" "$SERIAL_LOG"; then
    echo "FAIL: SyntraOS did not complete its boot sequence." >&2
    cat "$SERIAL_LOG" 2>/dev/null || true
    exit 1
fi

echo "PASS: QEMU screenshot confirms the 640x480 SyntraOS desktop pixels."
echo "Screenshot: $SCREENSHOT_PATH"

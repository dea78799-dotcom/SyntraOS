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
command -v python3 >/dev/null 2>&1 || {
    echo "FAIL: python3 is required for QEMU monitor input injection" >&2
    exit 1
}

TMP_DIR="$(mktemp -d)"
LOG_FILE="$TMP_DIR/serial.log"
QMP_SOCKET="$TMP_DIR/qmp.sock"
QEMU_PID=""

cleanup() {
    if [[ -n "$QEMU_PID" ]] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill -TERM "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
    fi
    if [[ -f "$LOG_FILE" ]]; then
        cat "$LOG_FILE"
    fi
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

echo "Running SyntraOS on QEMU virt ARM64; timeout=${TIMEOUT_SECONDS}s"
timeout --signal=TERM --kill-after=2 "${TIMEOUT_SECONDS}s" "$QEMU_BIN" \
    -machine virt,gic-version=3,virtualization=off,secure=off \
    -cpu cortex-a57 \
    -m 512M \
    -nic none \
    -device virtio-keyboard-device,bus=virtio-mmio-bus.0 \
    -kernel "$ELF_PATH" \
    -display none \
    -serial "file:$LOG_FILE" \
    -qmp "unix:$QMP_SOCKET,server=on,wait=off" \
    -no-reboot \
    -no-shutdown &
QEMU_PID=$!

BOOT_READY=0
for _ in $(seq 1 300); do
    if grep -Fq "VirtIO Input queue: STARTED" "$LOG_FILE" 2>/dev/null; then
        BOOT_READY=1
        break
    fi
    if ! kill -0 "$QEMU_PID" 2>/dev/null; then
        echo "FAIL: QEMU exited before VirtIO Input queue started." >&2
        cat "$LOG_FILE" 2>/dev/null || true
        exit 1
    fi
    sleep 0.1
done

if [[ "$BOOT_READY" -ne 1 ]]; then
    echo "FAIL: VirtIO Input queue did not start in time." >&2
    cat "$LOG_FILE" 2>/dev/null || true
    exit 1
fi

# Inject KEY_A through QMP so the event is delivered to the virtual keyboard.
python3 - "$QMP_SOCKET" <<'PY'
import json
import socket
import sys
import time

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(3)
sock.connect(sys.argv[1])
stream = sock.makefile("rwb", buffering=0)
greeting = json.loads(stream.readline())
print("QMP greeting received:", "QMP" in greeting)
stream.write(b'{"execute":"qmp_capabilities"}\r\n')
print("QMP capabilities:", stream.readline().decode().strip())

def send_key(down):
    payload = {
        "execute": "input-send-event",
        "arguments": {
            "events": [{
                "type": "key",
                "data": {
                    "down": down,
                    "key": {"type": "qcode", "data": "a"}
                }
            }]
        }
    }
    stream.write((json.dumps(payload) + "\r\n").encode())
    print("QMP key", "down" if down else "up", stream.readline().decode().strip())

send_key(True)
time.sleep(0.2)
send_key(False)
stream.close()
sock.close()
PY

INPUT_READY=0
for _ in $(seq 1 50); do
    if grep -Fq "VirtIO Input test: KEY_A DOWN" "$LOG_FILE" 2>/dev/null; then
        INPUT_READY=1
        break
    fi
    sleep 0.1
done

if [[ "$INPUT_READY" -ne 1 ]]; then
    echo "FAIL: QEMU injected KEY_A but SyntraOS did not confirm the input event." >&2
    cat "$LOG_FILE" 2>/dev/null || true
    exit 1
fi

if ! grep -Fq "SyntraOS boot OK" "$LOG_FILE" ||
   ! grep -Fq "VirtIO-MMIO function: INPUT" "$LOG_FILE" ||
   ! grep -Fq "VirtIO Input transport: DRIVER FEATURES NEGOTIATED" "$LOG_FILE" ||
   ! grep -Fq "VirtIO Input GIC trigger: CONFIGURED" "$LOG_FILE" ||
   ! grep -Fq "VirtIO Input event queue: CONFIGURED" "$LOG_FILE" ||
   ! grep -Fq "VirtIO Input IRQ: REGISTERED" "$LOG_FILE" ||
   ! grep -Fq "VirtIO Input queue: STARTED" "$LOG_FILE"; then
    echo "FAIL: QEMU boot or Input startup markers are missing." >&2
    cat "$LOG_FILE" 2>/dev/null || true
    exit 1
fi

echo "PASS: QEMU injected KEY_A and SyntraOS confirmed it through the VirtIO Input IRQ path."

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
TRACE_FILE="$TMP_DIR/qemu-trace.log"
TRACE_EVENTS_FILE="$TMP_DIR/trace-events"
QMP_SOCKET="$TMP_DIR/qmp.sock"

cat > "$TRACE_EVENTS_FILE" <<'TRACE_EVENTS'
input_event_key_qcode
input_event_sync
virtio_input_queue_full
virtio_queue_notify
virtio_notify
virtio_mmio_queue_write
TRACE_EVENTS
QEMU_PID=""

cleanup() {
    if [[ -n "$QEMU_PID" ]] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill -TERM "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
    fi
    if [[ -f "$LOG_FILE" ]]; then
        cat "$LOG_FILE"
    fi
    if [[ -f "$TRACE_FILE" ]]; then
        echo "=== QEMU trace ==="
        cat "$TRACE_FILE"
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
    -device ramfb,id=ramfb \
    -device virtio-keyboard-device,display=ramfb,bus=virtio-mmio-bus.0 \
    -kernel "$ELF_PATH" \
    -display none \
    -serial "file:$LOG_FILE" \
    -qmp "unix:$QMP_SOCKET,server=on,wait=off" \
    -trace "events=$TRACE_EVENTS_FILE,file=$TRACE_FILE" \
    -no-reboot \
    -no-shutdown &
QEMU_PID=$!

BOOT_READY=0
for _ in $(seq 1 300); do
    if grep -Fq "VirtIO Input queue: STARTED" "$LOG_FILE" 2>/dev/null &&
       grep -Fq "SyntraOS boot OK" "$LOG_FILE" 2>/dev/null; then
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
    echo "FAIL: SyntraOS boot and VirtIO Input queue startup did not complete in time." >&2
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
# This QEMU build lacks query-qtree. The subsequent injected key and\n# kernel-side IRQ marker verify keyboard attachment end-to-end.\n
def send_key(down):
    payload = {
        "execute": "input-send-event",
        "arguments": {
            "device": "ramfb",
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
    response = json.loads(stream.readline())
    print("QMP key", "down" if down else "up", json.dumps(response))
    if "error" in response:
        raise RuntimeError("QMP input-send-event failed: " + json.dumps(response["error"]))
    if "return" not in response:
        raise RuntimeError("QMP input-send-event returned an unexpected response")

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
    # Wait briefly for the kernel's bounded diagnostic observation window.
    for _ in $(seq 1 30); do
        if grep -Fq "VirtIO Input diagnostic: irq_status=" "$LOG_FILE" 2>/dev/null; then
            break
        fi
        sleep 0.1
    done
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
   ! grep -Fq "VirtIO Input queue: STARTED" "$LOG_FILE" ||
   ! grep -Fq "SyntraOS GUI v5: COMPOSED DESKTOP + LAUNCHER + FILES + SETTINGS" "$LOG_FILE"; then
    echo "FAIL: QEMU boot, GUI v5 composition, or Input startup markers are missing." >&2
    cat "$LOG_FILE" 2>/dev/null || true
    exit 1
fi

echo "PASS: GUI v5 composition marker present; QEMU injected KEY_A and SyntraOS confirmed it through the VirtIO Input IRQ path."

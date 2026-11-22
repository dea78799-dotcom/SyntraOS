#!/usr/bin/env python3
"""Capture and verify the SyntraOS QEMU ramfb desktop using the HMP monitor."""

import os
import socket
import sys
import time


def receive_prompt(connection):
    data = bytearray()
    deadline = time.monotonic() + 5.0
    while b"(qemu) " not in data:
        if time.monotonic() >= deadline:
            raise RuntimeError("timed out waiting for QEMU monitor prompt")
        chunk = connection.recv(4096)
        if not chunk:
            raise RuntimeError("QEMU monitor closed the connection")
        data.extend(chunk)
    return bytes(data)


def read_ppm(path):
    with open(path, "rb") as image_file:
        raw = image_file.read()

    cursor = 0

    def token():
        nonlocal cursor
        while cursor < len(raw):
            if raw[cursor] in b" \t\r\n":
                cursor += 1
            elif raw[cursor] == ord("#"):
                while cursor < len(raw) and raw[cursor] not in b"\r\n":
                    cursor += 1
            else:
                break
        start = cursor
        while cursor < len(raw) and raw[cursor] not in b" \t\r\n#":
            cursor += 1
        if start == cursor:
            raise RuntimeError("malformed PPM header")
        return raw[start:cursor]

    magic = token()
    width = int(token())
    height = int(token())
    max_value = int(token())

    if magic != b"P6" or width != 640 or height != 480 or max_value != 255:
        raise RuntimeError(
            f"unexpected screenshot format: {magic!r} {width}x{height} max={max_value}"
        )

    if cursor >= len(raw) or raw[cursor] not in b" \t\r\n":
        raise RuntimeError("PPM header is not separated from pixel data")
    if raw[cursor:cursor + 2] == b"\r\n":
        cursor += 2
    else:
        cursor += 1

    pixels = raw[cursor:]
    expected_size = width * height * 3
    if len(pixels) != expected_size:
        raise RuntimeError(
            f"PPM pixel payload has {len(pixels)} bytes; expected {expected_size}"
        )
    return width, height, pixels


def pixel(pixels, x, y):
    offset = (y * 640 + x) * 3
    return tuple(pixels[offset:offset + 3])


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: verify_qemu_screenshot.py MONITOR_SOCKET OUTPUT.ppm")

    monitor_path, screenshot_path = sys.argv[1:3]
    deadline = time.monotonic() + 10.0
    connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    while True:
        try:
            connection.connect(monitor_path)
            break
        except OSError:
            if time.monotonic() >= deadline:
                raise RuntimeError("QEMU monitor socket did not become available")
            time.sleep(0.05)

    connection.settimeout(5.0)
    try:
        receive_prompt(connection)
        command = f"screendump {screenshot_path}\n".encode("utf-8")
        connection.sendall(command)
        response = receive_prompt(connection)
        if b"Error" in response or b"error" in response:
            raise RuntimeError(f"QEMU screendump failed: {response.decode(errors='replace')}")
    finally:
        connection.close()

    width, height, pixels = read_ppm(screenshot_path)
    checks = {
        "wallpaper": ((10, 40), (16, 26, 53)),
        "top_status_panel": ((10, 10), (32, 45, 73)),
        "welcome_panel": ((30, 60), (32, 45, 73)),
        # The Files window begins at y=190 and covers the tile below it.
        # Sample the exposed blue strip above that window, not its selected row.
        "blue_app_tile": ((80, 185), (85, 120, 242)),
        "bottom_dock": ((10, 460), (32, 45, 73)),
    }

    for name, (position, expected) in checks.items():
        actual = pixel(pixels, *position)
        if actual != expected:
            raise RuntimeError(
                f"{name} pixel at {position}: got {actual}, expected {expected}"
            )
        print(f"PASS: {name} pixel {position} = {actual}")

    print(f"PASS: QEMU screendump is a valid {width}x{height} PPM image")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)

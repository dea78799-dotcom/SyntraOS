#!/usr/bin/env python3
"""Convert the QEMU P6 PPM desktop screenshot to a standard RGB PNG."""

import os
import struct
import sys
import zlib

from verify_qemu_screenshot import read_ppm


def png_chunk(kind, data):
    payload = kind + data
    return struct.pack(">I", len(data)) + payload + struct.pack(">I", zlib.crc32(payload) & 0xFFFFFFFF)


def write_png(path, width, height, pixels):
    stride = width * 3
    scanlines = b"".join(
        b"\\x00" + pixels[row * stride:(row + 1) * stride]
        for row in range(height)
    )
    png = (
        b"\\x89PNG\\r\\n\\x1a\\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(scanlines, level=9))
        + png_chunk(b"IEND", b"")
    )
    with open(path, "wb") as output:
        output.write(png)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: ppm_to_png.py INPUT.ppm OUTPUT.png")
    source, destination = sys.argv[1:3]
    width, height, pixels = read_ppm(source)
    os.makedirs(os.path.dirname(os.path.abspath(destination)), exist_ok=True)
    write_png(destination, width, height, pixels)
    with open(destination, "rb") as image_file:
        if image_file.read(8) != b"\\x89PNG\\r\\n\\x1a\\n":
            raise RuntimeError("PNG signature verification failed")
    print(f"PASS: converted {width}x{height} PPM to PNG: {destination}")


if __name__ == "__main__":
    main()

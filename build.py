#!/usr/bin/env python3
"""Build net_gotchi and merge firmware into build/."""

import os
import subprocess
import sys

ENV      = "m5stack-cores3"
OUT_DIR  = os.path.join(os.path.dirname(__file__), "build")
OUT_BIN  = os.path.join(OUT_DIR, f"netgotchi-{ENV}.bin")

PIO_BUILD = os.path.join(os.path.dirname(__file__), ".pio", "build", ENV)

# ESP32-S3: bootloader at 0x0, partitions at 0x8000, app at 0x10000
SEGMENTS = [
    ("bootloader", os.path.join(PIO_BUILD, "bootloader.bin"), 0x00000),
    ("partitions", os.path.join(PIO_BUILD, "partitions.bin"), 0x08000),
    ("firmware",   os.path.join(PIO_BUILD, "firmware.bin"),   0x10000),
]


def build():
    print(f"[build] pio run -e {ENV}")
    result = subprocess.run(["pio", "run", "-e", ENV])
    if result.returncode != 0:
        print("[build] ERROR: pio build failed")
        sys.exit(result.returncode)


def merge():
    os.makedirs(OUT_DIR, exist_ok=True)

    items = []
    for name, path, offset in SEGMENTS:
        if not os.path.isfile(path):
            print(f"[merge] missing {name}: {path}")
            sys.exit(1)
        with open(path, "rb") as f:
            data = f.read()
        print(f"[merge] {name:12s} @ 0x{offset:05x}  size 0x{len(data):x}  ({path})")
        items.append((offset, data))

    total = max(off + len(data) for off, data in items)
    buf = bytearray(b"\xff") * total
    for off, data in items:
        buf[off:off + len(data)] = data

    with open(OUT_BIN, "wb") as f:
        f.write(buf)
    print(f"[merge] written {OUT_BIN}  ({len(buf)} bytes)")


if __name__ == "__main__":
    build()
    merge()

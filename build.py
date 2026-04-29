Import("env")
import os
import traceback

pioenv      = env.subst("${PIOENV}")
BUILD_DIR   = env.subst("$BUILD_DIR")
PROJECT_DIR = env.get("PROJECT_DIR")

OUT_DIR = os.path.join(PROJECT_DIR, "build")
OUT_BIN = os.path.join(OUT_DIR, f"netgotchi-{pioenv}.bin")

board = env.BoardConfig()
mcu   = board.get("build.mcu", "")
boot_offset = 0x1000 if mcu == "esp32" else 0x0

SEGMENTS = [
    ("bootloader", os.path.join(BUILD_DIR, "bootloader.bin"), boot_offset),
    ("partitions", os.path.join(BUILD_DIR, "partitions.bin"), 0x8000),
    ("firmware",   os.path.join(BUILD_DIR, "firmware.bin"),   0x10000),
]


def _merge(source, target, env):
    try:
        os.makedirs(OUT_DIR, exist_ok=True)

        items = []
        for name, path, offset in SEGMENTS:
            if not os.path.isfile(path):
                print(f"[build] missing {name}: {path}")
                return
            with open(path, "rb") as f:
                data = f.read()
            print(f"[build] {name:12s} @ 0x{offset:05x}  {len(data)} bytes")
            items.append((offset, data))

        total = max(off + len(d) for off, d in items)
        buf = bytearray(b"\xff") * total
        for off, data in items:
            buf[off:off + len(data)] = data

        with open(OUT_BIN, "wb") as f:
            f.write(buf)
        print(f"[build] merged -> {OUT_BIN}  ({len(buf)} bytes)")
    except Exception:
        print("[build] merge failed:")
        traceback.print_exc()


env.AddPostAction(os.path.join(BUILD_DIR, "firmware.bin"), _merge)

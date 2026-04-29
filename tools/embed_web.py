Import("env")  # type: ignore
import os

PROJECT_DIR = env.get("PROJECT_DIR")
SRC_DIR     = os.path.join(PROJECT_DIR, "web", "file_manager")
OUT_PATH    = os.path.join(PROJECT_DIR, "src", "net", "WebFiles.h")

FILES = [
    ("index.html", "WEBFILE_HTML"),
    ("index.css",  "WEBFILE_CSS"),
    ("index.js",   "WEBFILE_JS"),
]

lines = ["#pragma once", "#include <pgmspace.h>", ""]

for fname, var in FILES:
    fpath = os.path.join(SRC_DIR, fname)
    if not os.path.exists(fpath):
        print("[embed_web] WARNING: %s not found, skipping" % fpath)
        continue
    with open(fpath, "rb") as f:
        data = f.read()
    rows = []
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        rows.append("  " + ", ".join("0x%02x" % b for b in chunk))
    lines.append("static const uint8_t %s[] PROGMEM = {" % var)
    lines.append(",\n".join(rows))
    lines.append("};")
    lines.append("static const size_t %s_LEN = %d;" % (var, len(data)))
    lines.append("")
    print("[embed_web] %s -> %s (%d bytes)" % (fname, var, len(data)))

with open(OUT_PATH, "w") as f:
    f.write("\n".join(lines) + "\n")

print("[embed_web] Generated %s" % OUT_PATH)

#!/usr/bin/env python3
"""
Compile crack.c → crack.wasm.  Run once whenever crack.c changes:
    python3 tools/build_wasm.py
Requires LLVM with wasm32 support (brew install llvm) or emscripten.
"""
import os, subprocess, sys

ROOT     = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
C_PATH   = os.path.join(ROOT, "web", "file_manager", "crack.c")
OUT_PATH = os.path.join(ROOT, "web", "file_manager", "crack.wasm")

EXPORTS = [
    "wasm_try_password", "wasm_pw_buf", "wasm_ssid_buf",
    "wasm_prf_data_buf", "wasm_eapol_buf", "wasm_mic_buf",
]

CLANG_CANDIDATES = [
    "/opt/homebrew/opt/llvm/bin/clang",   # Homebrew LLVM (Apple Silicon)
    "/usr/local/opt/llvm/bin/clang",       # Homebrew LLVM (Intel)
    "clang-18", "clang-17", "clang-16", "clang-15", "clang",
]

def try_clang():
    export_flags = ["-Wl,--export=" + e for e in EXPORTS]
    for cc in CLANG_CANDIDATES:
        try:
            r = subprocess.run([cc, "--version"], capture_output=True)
        except FileNotFoundError:
            continue
        if r.returncode != 0:
            continue
        cmd = [cc, "--target=wasm32-unknown-unknown", "-O3",
               "-nostdlib", "-fno-builtin", "-Wl,--no-entry"] + \
              export_flags + ["-o", OUT_PATH, C_PATH]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode == 0:
            return cc
        print("  %s: %s" % (cc, r.stderr.strip()))
        return None  # found a clang but wasm32 unsupported — don't try others
    return None

def try_emcc():
    exported = ",".join('"_' + e + '"' for e in EXPORTS)
    try:
        r = subprocess.run(["emcc", "--version"], capture_output=True)
    except FileNotFoundError:
        return False
    if r.returncode != 0:
        return False
    cmd = ["emcc", "-O3", "-s", "STANDALONE_WASM=1",
           "-s", "EXPORTED_FUNCTIONS=[%s]" % exported,
           "--no-entry", "-o", OUT_PATH, C_PATH]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode == 0:
        return True
    print("  emcc: %s" % r.stderr.strip())
    return False

print("Building %s -> %s" % (C_PATH, OUT_PATH))

cc = try_clang()
if cc:
    size = os.path.getsize(OUT_PATH)
    print("OK via %s (%d bytes)" % (cc, size))
    sys.exit(0)

if try_emcc():
    size = os.path.getsize(OUT_PATH)
    print("OK via emcc (%d bytes)" % size)
    sys.exit(0)

print("ERROR: no wasm32 compiler found.")
print("  Install: brew install llvm   (adds /opt/homebrew/opt/llvm/bin/clang)")
print("  Or:      brew install emscripten")
sys.exit(1)

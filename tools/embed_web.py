Import("env")  # type: ignore
import os, re, subprocess, tempfile

PROJECT_DIR = env.get("PROJECT_DIR")
SRC_DIR     = os.path.join(PROJECT_DIR, "web", "file_manager")
OUT_PATH    = os.path.join(PROJECT_DIR, "src", "net", "WebFiles.h")

FILES = [
    ("index.html", "WEBFILE_HTML"),
    ("index.css",  "WEBFILE_CSS"),
    ("index.js",   "WEBFILE_JS"),
    ("crack.wasm", "WEBFILE_WASM"),
]

# ── Minifiers ─────────────────────────────────────────────────

def minify_html(src):
    src = re.sub(r'<!--.*?-->', '', src, flags=re.DOTALL)  # remove comments
    src = re.sub(r'>\s+<', '><', src)                      # collapse inter-tag space
    src = re.sub(r'[ \t]+', ' ', src)                      # collapse runs of spaces
    src = re.sub(r'\n+', '\n', src)
    return src.strip()

def minify_css(src):
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.DOTALL)   # remove comments
    src = re.sub(r'\s*([{};:,>~+])\s*', r'\1', src)        # remove spaces around punctuation
    src = re.sub(r'\s+', ' ', src)
    return src.strip()

def minify_js(src):
    result = []
    i, n = 0, len(src)
    last = ''
    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ''
        if c in ('"', "'", '`'):
            j = i + 1
            while j < n:
                if src[j] == '\\' and j + 1 < n:
                    j += 2; continue
                if src[j] == c:
                    j += 1; break
                j += 1
            result.append(src[i:j]); last = c; i = j
        elif c == '/' and nxt == '*':
            i += 2
            while i + 1 < n and src[i:i + 2] != '*/':
                i += 1
            i += 2; result.append(' ')
        elif c == '/' and nxt == '/':
            while i < n and src[i] != '\n':
                i += 1
            result.append('\n')
        elif c == '/':
            if last and (last in ')}]_' or last.isalnum()):
                result.append(c); last = c; i += 1
            else:
                j = i + 1
                while j < n:
                    if src[j] == '\\' and j + 1 < n:
                        j += 2; continue
                    if src[j] == '[':
                        j += 1
                        while j < n and src[j] != ']':
                            if src[j] == '\\' and j + 1 < n:
                                j += 2; continue
                            j += 1
                        j += 1; continue
                    if src[j] == '/':
                        j += 1; break
                    j += 1
                while j < n and src[j].isalpha():
                    j += 1
                result.append(src[i:j]); last = '/'; i = j
        else:
            result.append(c)
            if not c.isspace():
                last = c
            i += 1
    src = ''.join(result)
    src = re.sub(r'[ \t]+', ' ', src)
    src = re.sub(r' *([{}\(\)\[\];,]) *', r'\1', src)
    src = '\n'.join(l.strip() for l in src.split('\n') if l.strip())
    return src

MINIFIERS = {
    ".html": minify_html,
    ".css":  minify_css,
    ".js":   minify_js,
}

def check_js(text, fname):
    node = subprocess.run(["node", "--version"], capture_output=True)
    if node.returncode != 0:
        print("[embed_web] WARNING: node not found, skipping JS syntax check")
        return
    with tempfile.NamedTemporaryFile(suffix=".js", mode="w",
                                     encoding="utf-8", delete=False) as tf:
        tf.write(text)
        tmp = tf.name
    try:
        r = subprocess.run(["node", "--check", tmp],
                           capture_output=True, text=True)
        if r.returncode != 0:
            err = (r.stderr or r.stdout).replace(tmp, fname)
            raise SystemExit("[embed_web] JS syntax error in minified %s:\n%s" % (fname, err))
        print("[embed_web] JS syntax OK: %s" % fname)
    finally:
        os.unlink(tmp)

# ── WASM build ────────────────────────────────────────────────

def build_wasm(c_path, wasm_path):
    exports = [
        "wasm_try_password", "wasm_pw_buf", "wasm_ssid_buf",
        "wasm_prf_data_buf", "wasm_eapol_buf", "wasm_mic_buf",
    ]
    export_flags = ["--export=" + e for e in exports]

    candidates = ["clang", "clang-18", "clang-17", "clang-16", "clang-15"]
    for cc in candidates:
        r = subprocess.run([cc, "--version"], capture_output=True)
        if r.returncode != 0:
            continue
        cmd = [cc, "--target=wasm32-unknown-unknown",
               "-O3", "-nostdlib", "-fno-builtin",
               "-Wl,--no-entry"] + ["-Wl," + f for f in export_flags] + \
              ["-o", wasm_path, c_path]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode == 0:
            print("[embed_web] WASM built: %s" % wasm_path)
            return True
        print("[embed_web] %s wasm32 failed: %s" % (cc, r.stderr.strip()))
        break

    # Try emscripten
    r = subprocess.run(["emcc", "--version"], capture_output=True)
    if r.returncode == 0:
        exported = ",".join('"_' + e + '"' for e in exports)
        cmd = ["emcc", "-O3", "-s", "STANDALONE_WASM=1",
               "-s", "EXPORTED_FUNCTIONS=[%s]" % exported,
               "--no-entry", "-o", wasm_path, c_path]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode == 0:
            print("[embed_web] WASM built via emcc: %s" % wasm_path)
            return True
        print("[embed_web] emcc failed: %s" % r.stderr.strip())

    print("[embed_web] WARNING: cannot build WASM — install LLVM (brew install llvm) or emscripten")
    return False

# ── Main ──────────────────────────────────────────────────────

lines = ["#pragma once", "#include <pgmspace.h>", ""]

for fname, var in FILES:
    fpath = os.path.join(SRC_DIR, fname)
    ext   = os.path.splitext(fname)[1]

    if ext == ".wasm":
        c_path = fpath.replace(".wasm", ".c")
        if not os.path.exists(fpath) or (
            os.path.exists(c_path) and
            os.path.getmtime(c_path) > os.path.getmtime(fpath)
        ):
            if not build_wasm(c_path, fpath):
                print("[embed_web] WARNING: %s not available, skipping" % fname)
                continue
        if not os.path.exists(fpath):
            print("[embed_web] WARNING: %s not found, skipping" % fpath)
            continue
        with open(fpath, "rb") as f:
            data = f.read()
        rows = []
        for i in range(0, len(data), 16):
            rows.append("  " + ", ".join("0x%02x" % b for b in data[i:i+16]))
        lines.append("static const uint8_t %s[] PROGMEM = {" % var)
        lines.append(",\n".join(rows))
        lines.append("};")
        lines.append("static const size_t %s_LEN = %d;" % (var, len(data)))
        lines.append("")
        print("[embed_web] %s -> %s (%d bytes)" % (fname, var, len(data)))
        continue

    if not os.path.exists(fpath):
        print("[embed_web] WARNING: %s not found, skipping" % fpath)
        continue
    with open(fpath, "r", encoding="utf-8") as f:
        text = f.read()
    minify = MINIFIERS.get(ext)
    if minify:
        text = minify(text)
    if ext == ".js":
        check_js(text, fname)
    data = text.encode("utf-8")
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

from os import remove, rename
from os.path import isfile, join
import subprocess
import sys

Import("env")  # type: ignore

FRAMEWORK_DIR = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
TOOLCHAIN_DIR = env.PioPlatform().get_package_dir("toolchain-xtensa-esp-elf")

board_mcu = env.BoardConfig()
mcu       = board_mcu.get("build.mcu", "esp32s3")

lib_dir        = join(FRAMEWORK_DIR, "tools", "esp32-arduino-libs", mcu, "lib")
patchflag_path = join(lib_dir, ".patched_netgotchi")
original_file  = join(lib_dir, "libnet80211.a")
patched_file   = join(lib_dir, "libnet80211.a.patched")
objcopy        = join(TOOLCHAIN_DIR, "bin", "xtensa-%s-elf-objcopy" % mcu)

print("[pre-build] Checking libnet80211.a patch for %s..." % mcu)

if isfile(patchflag_path):
    print("[pre-build] Patch already applied, skipping.")
elif not isfile(original_file):
    sys.stderr.write("[pre-build] WARNING: %s not found — skipping patch\n" % original_file)
elif not isfile(objcopy):
    sys.stderr.write("[pre-build] WARNING: objcopy not found at %s — skipping patch\n" % objcopy)
else:
    print("[pre-build] Patching libnet80211.a (weaken ieee80211_raw_frame_sanity_check)...")
    ret = subprocess.call([objcopy,
                           "--weaken-symbol=ieee80211_raw_frame_sanity_check",
                           original_file, patched_file])
    if ret != 0 or not isfile(patched_file):
        sys.stderr.write("[pre-build] ERROR: objcopy failed (exit %d) — aborting patch\n" % ret)
        env.Exit(1)

    if isfile("%s.orig" % original_file):
        remove("%s.orig" % original_file)
    rename(original_file, "%s.orig" % original_file)
    rename(patched_file, original_file)

    with open(patchflag_path, "w") as f:
        f.write("")
    print("[pre-build] Patch applied.")

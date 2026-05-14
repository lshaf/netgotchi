from os import remove, rename
from os.path import isfile, join
import subprocess
import sys

Import("env")  # type: ignore

FRAMEWORK_DIR = env.PioPlatform().get_package_dir("framework-arduinoespressif32")

board_mcu = env.BoardConfig()
mcu       = board_mcu.get("build.mcu", "esp32s3")

# Toolchain package name varies by platform version:
#   Espressif 51.x+ (Arduino 3.x): toolchain-xtensa-esp-elf       (xtensa-esp-elf-objcopy)
#   Espressif 6.x   (Arduino 2.x): toolchain-xtensa-<mcu>          (xtensa-<mcu>-elf-objcopy)
TOOLCHAIN_DIR = (env.PioPlatform().get_package_dir("toolchain-xtensa-esp-elf")
                 or env.PioPlatform().get_package_dir("toolchain-xtensa-%s" % mcu)
                 or env.PioPlatform().get_package_dir("toolchain-xtensa-esp32s3"))

# Pick the matching objcopy binary
if TOOLCHAIN_DIR and "esp-elf" in TOOLCHAIN_DIR:
    objcopy = join(TOOLCHAIN_DIR, "bin", "xtensa-esp-elf-objcopy")
elif TOOLCHAIN_DIR:
    objcopy = join(TOOLCHAIN_DIR, "bin", "xtensa-%s-elf-objcopy" % mcu)
else:
    objcopy = None

# Library path also differs between framework versions
lib_dir_new = join(FRAMEWORK_DIR, "tools", "esp32-arduino-libs", mcu, "lib")
lib_dir_old = join(FRAMEWORK_DIR, "tools", "sdk", mcu, "lib")
lib_dir     = lib_dir_new if isfile(join(lib_dir_new, "libnet80211.a")) else lib_dir_old

patchflag_path = join(lib_dir, ".patched_netgotchi")
original_file  = join(lib_dir, "libnet80211.a")
patched_file   = join(lib_dir, "libnet80211.a.patched")

print("[pre-build] Checking libnet80211.a patch for %s..." % mcu)

if isfile(patchflag_path):
    print("[pre-build] Patch already applied, skipping.")
elif not isfile(original_file):
    sys.stderr.write("[pre-build] WARNING: %s not found — skipping patch\n" % original_file)
elif not objcopy or not isfile(objcopy):
    sys.stderr.write("[pre-build] WARNING: objcopy not found (%s) — skipping patch\n" % objcopy)
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

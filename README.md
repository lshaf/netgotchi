# net_gotchi

A WPA2 handshake hunter and offline password cracker running on the M5Stack CoreS3. Presents as a retro terminal with a touch menu, animated mascot, and persistent XP system.

---

## Hardware

| | |
|---|---|
| **Target board** | M5Stack CoreS3 (ESP32-S3, 320×240 touch display) |
| **Storage** | SD card (required for captures, dictionaries, cracked passwords) |
| **Framework** | Arduino via PlatformIO |

---

## Features

### Services (one runs at a time)

| Command | Mode | Description |
|---|---|---|
| `nethunt` | Active | Sweeps channels 1–13, deauths connected stations to force a WPA2 handshake, captures the 4-way EAPOL exchange to PCAP |
| `nettrap` | Passive | Listens on each channel without transmitting; captures handshakes opportunistically |
| `netguard` | Passive | Threat monitor — detects deauth attacks, beacon floods, and evil twin APs |

### Utility Commands

| Command | Description |
|---|---|
| `crack` | Dictionary attack on a captured PCAP; runs PBKDF2-HMAC-SHA1 + PRF-512 MIC verification across two FreeRTOS tasks in parallel |
| `vault` | Browse and display previously cracked passwords saved to SD |
| `cleaneapol` | Delete incomplete PCAP files (captures missing M2 or anonce) |
| `profile` | View XP, level, capture/crack/discovery counts, and battery status |
| `set` | Configure theme, display brightness, and display-off timeout |
| `poweroff` | Safe shutdown |

---

## How nethunt Works

1. Sets channel 1, waits 5 s for beacons.
2. For each unvalidated AP on the channel: sends 3 deauth bursts (spaced 2 s apart), then waits 5 s post-deauth for a reconnect handshake.
3. On EAPOL capture: writes a `.pcap` file with the beacon + all EAPOL frames; marks the AP validated.
4. Advances to the next channel. After channel 13 exhausts all targets, enters a 60 s sleep, then restarts from channel 1.

The raw deauth frames require weakening the `ieee80211_raw_frame_sanity_check` symbol in `libnet80211.a` — this patch is applied automatically at build time (see [Build Notes](#build-notes)).

---

## How crack Works

- Select a PCAP from `/netgotchi/eapol/`, then select a wordlist from `/netgotchi/dictionaries/`.
- A producer FreeRTOS task reads passwords from the wordlist file and feeds them into a queue.
- Two worker tasks pull passwords and test each with `PBKDF2(password, SSID, 4096) → PRF-512 → HMAC-SHA1 MIC verify`.
- The custom `FastWpaCrack` implementation uses software SHA-1 with pre-computed HMAC pad states to avoid mbedTLS mutex overhead on dual-core.
- On success the password is written to `/netgotchi/cracked/<BSSID>_<SSID>.pass`.

---

## XP System

Actions earn XP and are persisted to SD on every event.

| Action | XP |
|---|---|
| Handshake capture | +4 |
| Successful crack | +8 |
| AP discovery | +1 |

Level = `(total XP / 100) + 1`.

---

## SD Card Layout

Directories are created automatically on boot.

| Path | Contents |
|---|---|
| `/netgotchi/eapol/<BSSID>_<SSID>.pcap` | Captured WPA2 handshakes (standard pcap format) |
| `/netgotchi/dictionaries/<name>` | Wordlists — plaintext, one password per line, 8–63 chars |
| `/netgotchi/cracked/<BSSID>_<SSID>.pass` | Cracked passwords (plaintext) |
| `/netgotchi/config` | Theme, brightness, display-off setting (binary) |
| `/netgotchi/stats` | XP and lifetime counters (binary) |

---

## UI

The screen is divided into three zones:

```
┌────────────────────────────────────┐
│  HUD: level · xp bar · battery     │  ← tap right side to open menu
│  HUD: channel · event counters     │
├────────────────────────────────────┤
│                                    │
│  Scrolling log                     │
│  (20 lines, ring buffer)           │
│                                    │
├────────────────────────────────────┤
│  $ typing animation                │  ← command with typewriter effect
└────────────────────────────────────┘
```

- Tap the top-right area (where the mascot lives) to open the menu.
- The animated mascot changes state depending on the active service: `Active`, `Trap`, `Guard`, `Decrypting`, `Sleep`, or `Idle`.
- Eight built-in themes; brightness and display-off timeout are configurable from the `set` menu.

---

## Build Notes

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE extension)
- Python 3 (used by the pre/post build scripts)

### Build

```sh
pio run -e m5stack-cores3
```

### libnet80211 Patch

Raw 802.11 frame injection on ESP32-S3 requires weakening `ieee80211_raw_frame_sanity_check` in the Arduino framework's `libnet80211.a`. The pre-build script `patch.py` does this automatically using `xtensa-esp32s3-elf-objcopy` the first time you build. A `.patched_netgotchi` flag file is written so subsequent builds skip it.

If you update the Arduino-ESP32 framework, delete the flag file to re-apply the patch:

```sh
rm <pio-packages>/framework-arduinoespressif32/tools/esp32-arduino-libs/esp32s3/lib/.patched_netgotchi
```

### Output Binary

After a successful build, `build.py` merges the bootloader, partition table, and firmware into a single flashable image:

```
build/netgotchi-m5stack-cores3.bin
```

### Flash

```sh
pio run -e m5stack-cores3 -t upload
```

Or flash the merged binary directly:

```sh
esptool.py --chip esp32s3 write_flash 0x0 build/netgotchi-m5stack-cores3.bin
```

---

## Web File Manager

The `web/file_manager/` directory contains the "Geek File Manager" — a browser-based SD card manager intended to run on a companion server or served from the device. It allows uploading wordlists, downloading captured PCAPs, and managing files over HTTP.

---

## Project Structure

```
src/
  main.cpp                  Arduino entry point
  term/
    App.cpp / App.h         Main application loop, touch handling, draw
    AppLayout.h             Screen geometry constants
    AppDraw.cpp             HUD, log, input, and menu rendering
    Virus.cpp / Virus.h     Animated mascot widget
    Stats.cpp / Stats.h     XP / level / battery
    Theme.cpp / Theme.h     8 colour themes + brightness + display-off
    command/
      MenuCommand.h         IMenuHost interface + MenuCommand base
      NethuntCommand.*      Active handshake hunter
      NettrapCommand.*      Passive handshake listener
      NetguardCommand.*     Threat detection monitor
      CrackCommand.*        Dictionary WPA2 cracker
      VaultCommand.*        Browse cracked passwords
      CleanEapolCommand.h   Delete incomplete PCAPs
      ProfileCommand.*      Show XP / stats
      SetCommand.*          Theme / brightness / display-off settings
      PowerOffCommand.*     Safe shutdown
  net/
    WiFiHunter.cpp / .h     Promiscuous capture, deauth injection, PCAP write
    WifiGuard.cpp  / .h     Passive threat monitor (deauth / flood / evil twin)
  core/
    FastWpaCrack.cpp / .h   Software PBKDF2-HMAC-SHA1 + PRF-512 WPA2 tester
    RandomSeed.cpp / .h     Hardware RNG seed on boot
web/
  file_manager/             Browser-based SD card file manager
patch.py                    Pre-build: weaken libnet80211.a for raw frame TX
build.py                    Post-build: merge firmware segments into single .bin
platformio.ini              PlatformIO config (M5Stack CoreS3)
```

---

## Legal

This tool is intended for authorised security testing, research, and educational use only. Do not use it against networks you do not own or have explicit written permission to test.

# net_gotchi

WPA2 handshake hunter and cracker for ESP32 (M5Stack CoreS3).

## SD Card Paths

All files are stored under `/netgotchi/` on the SD card. Directories are created automatically on boot.

| Path | Description |
|------|-------------|
| `/netgotchi/config` | Theme and brightness settings (binary) |
| `/netgotchi/stats` | XP and capture count (binary) |
| `/netgotchi/eapol/<BSSID>_<SSID>.pcap` | Captured WPA2 handshakes (pcap format) |
| `/netgotchi/dictionaries/<name>` | Wordlists for crack command (plaintext, one password per line, 8–63 chars) |
| `/netgotchi/cracked/<BSSID>_<SSID>.pass` | Cracked passwords (plaintext) |

# Gateway-Flasher (Browser, USB) — ESP Web Tools

Flasht den **ESP32-S3-Gateway** (OEPL-Shim) **per USB direkt aus dem Browser**
(Chrome/Edge, Web Serial) — ohne PlatformIO-Kommandozeile und ohne
Partitionsumbau. Es wird der komplette Flash geschrieben (Bootloader +
Partitionstabelle + App), also auch zum **Recovery** geeignet.

> Kein echtes HA-Supervisor-Addon nötig: Das hier ist eine statische Webseite.
> Sie kann aus HA verlinkt werden, der eigentliche Flash läuft aber
> Browser ↔ USB (nicht über HA).

## Einmalig: Firmware bereitstellen

Die `.bin`-Dateien sind **Build-Artefakte und absichtlich nicht im Repo**
(`.gitignore`). Nach jedem Build neben diese Seite kopieren:

```bash
cd gateway/gateway_oepl_shim
pio run                      # baut .pio/build/esp32-s3-devkitc-1/

# ins Flasher-Verzeichnis kopieren:
B=.pio/build/esp32-s3-devkitc-1
cp "$B/bootloader.bin" "$B/partitions.bin" "$B/firmware.bin" ../flasher/
# boot_app0.bin kommt aus dem Framework-Paket:
cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin ../flasher/
```

(Windows/PowerShell: `boot_app0.bin` liegt unter
`%USERPROFILE%\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin`.)

## Flashen

1. Ordner über **HTTPS oder localhost** ausliefern (Web Serial braucht einen
   sicheren Kontext — `file://` und einfaches `http://…` über LAN gehen **nicht**):
   ```bash
   cd gateway/flasher && python -m http.server 8000
   ```
   Dann im **Chrome/Edge** öffnen: `http://localhost:8000/`
   *(Alternativen: GitHub Pages oder ein anderer HTTPS-Host.)*
2. ESP32-S3 per USB anstecken → **Install** → seriellen Port wählen → flashen.
   Kommt kein Port: beim Einstecken **BOOT** gedrückt halten (Download-Modus).

## Partitions-Offsets (im `manifest.json` hinterlegt)

| Datei | Offset | |
|-------|--------|--|
| `bootloader.bin` | `0x0` | **ESP32-S3**: Bootloader liegt bei 0x0 (nicht 0x1000 wie beim alten ESP32) |
| `partitions.bin` | `0x8000` | Partitionstabelle |
| `boot_app0.bin` | `0xe000` | OTA-Data (Framework) |
| `firmware.bin` | `0x10000` | App |

## Nur-Browser ohne lokale Kopie (optional)
Wer nicht jedes Mal kopieren will: eine **gemergte** Datei bauen und hosten
(z. B. als GitHub-Release-Asset, `manifest.json` dann auf eine einzige Part
`{"path": "<url>", "offset": 0}` zeigen lassen):
```bash
cd gateway/gateway_oepl_shim
B=.pio/build/esp32-s3-devkitc-1
pio pkg exec -- esptool.py --chip esp32s3 merge_bin -o merged.bin \
  --flash_mode dio --flash_size 4MB \
  0x0 "$B/bootloader.bin" 0x8000 "$B/partitions.bin" \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 "$B/firmware.bin"
```

## Grenzen
- **Nur Chrome/Edge (Desktop)** — Firefox/Safari können kein Web Serial.
- Flash geht nur **per USB** an dem Rechner, an dem der ESP hängt (das ist der
  Zweck). Für kabellose Updates wäre OTA (A/B-Partitionen) nötig — separat.

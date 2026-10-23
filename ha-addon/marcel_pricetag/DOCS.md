# Marcel PriceTag — Home-Assistant-Addon

Ein Addon mit Weboberfläche, um die **ESP-Gateway-Firmware zu bauen & zu flashen**
und später **CC2510-Tags zu flashen** — alles aus Home Assistant heraus.

> **Status: v0-Gerüst.** Konventionskonform aufgebaut, aber **nicht auf echter
> HA getestet** (in der Entwicklungsumgebung kein HA/USB/Registry-Zugriff).
> Als Startpunkt gedacht — bitte lokal installieren und iterieren.

## Installieren (lokales Addon)
1. Den Ordner `ha-addon/marcel_pricetag/` in den HA-Share **`/addons/`** kopieren
   (z. B. per Samba/SSH-Addon), sodass `/addons/marcel_pricetag/config.yaml` existiert.
2. HA → **Einstellungen → Add-ons → Add-on Store → ⋮ → Repositories neu laden**
   (bzw. „Lokale Add-ons"). „Marcel PriceTag" erscheint → **Installieren**.
3. Unter **Konfiguration** eintragen:
   - `wifi_ssid` / `wifi_pass` — kommen in die Gateway-Firmware (nötig, damit das
     Gateway sich mit WLAN verbindet).
   - `repo_url` / `branch` — Quelle (Default: dieses Repo / `main`).
   - `gateway_host` — mDNS/IP des Gateways (für spätere OTA).
4. **Starten** und **„Weboberfläche öffnen"** (Ingress).

## Benutzen
1. **Repo holen/aktualisieren** — klont/aktualisiert die Quellen nach `/data/repo`.
2. **Gateway-Firmware bauen** — PlatformIO (lädt beim ersten Mal die ESP-Toolchain;
   braucht Internet). Schreibt vorher `credentials.h` aus den Optionen.
3. **Gateway per USB flashen** — ESP32-S3 am HA-Host anstecken, Port wählen, flashen
   (`pio run -t upload`).
4. **Tag-Firmware bauen** (2,66"/4,2", SDCC) → **Tag flashen (CCLib)** über den
   ESP32-S3-Debugger an einem USB-Port des HA-Hosts.

## Voraussetzungen & Grenzen
- **USB-Zugriff:** Das Addon nutzt `usb`/`uart`. Der ESP bzw. der CCLib-Debugger
  muss **am HA-Host** hängen und als `/dev/ttyUSB*`/`/dev/ttyACM*` erscheinen.
- **Bauen braucht Internet** (PlatformIO-Registry/Toolchain, SDCC-Pakete im Image).
- **OTA (kabellos)** ist noch **nicht** dabei: dafür braucht die Firmware zuerst
  ArduinoOTA **und** eine A/B-Partitionstabelle (aktuell nur ein App-Slot). Bis
  dahin: USB-Flash (oben) oder der Browser-USB-Flasher in `gateway/flasher/`.
- **Tag-OTA über Funk** ist nicht enthalten (eigenes, größeres Thema).
- Persistenz: Quellen + PlatformIO-Cache liegen im Addon-Volume `/data`
  (überleben Neustarts, nicht aber eine Addon-Neuinstallation).

## Dateien
- `config.yaml` / `build.yaml` — Addon-Metadaten + Basis-Images.
- `Dockerfile` — git, make, sdcc, PlatformIO, esptool, pyserial, flask.
- `run.sh` — liest Optionen, startet den Server.
- `app/server.py` — Flask-Backend + Ingress-UI (Build/Flash, Live-Log).

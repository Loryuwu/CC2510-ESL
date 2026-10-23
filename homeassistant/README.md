# Home-Assistant-Wetterdisplay (OpenEPaperLink)

Zeigt das Wetter auf dem PriceTag-Preisschild an — gerendert in Home Assistant
über die **OpenEPaperLink**-Integration, gefunkt über den PriceTag-**OEPL-Shim**
auf dem ESP32.

```
Wetter-Entity ─▶ Home Assistant ─(drawcustom)─▶ ESP32 OEPL-Shim ─(nRF24)─▶ CC2510 ─▶ E-Ink (296×152 BWR)
```

Dateien:
- [`weather_oepl.yaml`](./weather_oepl.yaml) — die HA-Automation (das Display).
- [`weather_forecast_sensors.yaml`](./weather_forecast_sensors.yaml) — optionale
  Template-Sensoren für **Hoch/Tief**.

Die Wetterquelle ist egal (met.no `weather.forecast_home`, OpenWeatherMap,
…) — Hauptsache eine `weather.*`-Entity. Die Beispiele nutzen
`weather.forecast_home`.

## ⚠️ Zwei Stolperfallen (beide hier schon behoben)

1. **OEPL rendert die Payload-Templates nur mit HA-State-Kontext** — also
   `states()`, `state_attr()`, `now()`. Die **`variables:` der Automation** und
   **response_variablen** (z. B. `fc` aus `weather.get_forecasts`) sind beim
   Rendern **nicht sichtbar**. Deshalb stehen in `weather_oepl.yaml` überall
   **literale Entity-IDs** statt einer Variablen. (Das war der Grund, warum eine
   frühere Version „keine Wetterdaten" zeigte.)
2. **Nicht die ganze Service-Struktur in das `payload`-Feld packen.** Der
   visuelle Editor neigt dazu, einen kompletten `action/target/data`-Block *in*
   `payload` zu verschachteln. `data.payload` muss **direkt die Liste der
   Zeichen-Elemente** sein. Am einfachsten: Automation per **„In YAML
   bearbeiten"** komplett ersetzen, nicht im visuellen Editor zusammenklicken.

## Voraussetzungen
1. **OEPL-Shim** läuft (`gateway/gateway_oepl_shim/`), erreichbar z. B. als
   `pricetag-ap.local`. Er meldet den Tag als **hwType 0x56 = 2,66" 296×152 BWR**
   — darauf ist das Layout ausgelegt.
2. **OpenEPaperLink-Integration** installiert und auf den Shim gezeigt, Tag
   enrollt.
3. Eine **Wetter-Entity** in HA (met.no „Wettervorhersage" liefert von Haus aus
   `weather.forecast_home`; OpenWeatherMap o. ä. geht genauso).

## Einrichten
1. In `weather_oepl.yaml` anpassen:
   - **`device_id`** im `target:` → dein Tag (ab OEPL 1.0 wird per Geräte-ID
     adressiert, nicht per Entity).
   - **`weather.forecast_home`** → deine Wetter-Entity (in allen Templates), falls
     anders.
   - **`"Dortmund"`** → dein Ort fürs Banner.
2. **Einstellungen → Automationen → Neue Automation → ⋮ → „In YAML bearbeiten"**,
   kompletten Inhalt einfügen, speichern.
3. Testen: Automation öffnen → **„Ausführen"**. Das Bild erscheint beim nächsten
   Tag-Poll (Pull-Modell).

### Hoch/Tief (optional)
Inhalt von `weather_forecast_sensors.yaml` in die `configuration.yaml` einfügen
(oder als Package), Wetter-Entity ggf. anpassen, HA neu starten. Das legt
`sensor.pricetag_hoch` / `sensor.pricetag_tief` an, die das Display liest. Ohne
diese Sensoren zeigt das Display dort einfach `--`.

## Layout (296×152)
Rote Banner-Leiste (Ort + Wochentag/Uhr) · große Temperatur (schwarz) +
Zustand (rot) · „gefühlt" klein · Wetter-Icon oben rechts (Sonne/Nacht rot,
sonst schwarz) · Hoch/Tief + Feuchte/Wind rechts.

## Anpassen / Fehlersuche
- **Umlaute** (ö/ü) kommen über die TTF-Schrift; falls dein Build sie nicht
  zeigt, in den Strings durch `oe`/`ue` ersetzen.
- **Icon-Farbe** wird über `color:` gesetzt; zeigt deine OEPL-Version das Icon
  nicht farbig, `color:` beim `icon`-Element in `fill:` umbenennen.
- **Weitere Elementtypen** + Felder: offizielle OEPL-Doku
  `drawcustom/supported_types.md` im Integrations-Repo. Design per Drag&Drop:
  Online-Designer wie OpenEpaperGUI / odl-drawcustom-designer erzeugen
  `drawcustom`-Payloads.

#!/usr/bin/env python3
"""
Marcel PriceTag - HA-Addon-Backend (v0).

Kleiner Flask-Server (Ingress, Port 8099) mit Weboberflaeche zum
 - Quellen holen/aktualisieren (git clone/pull),
 - ESP-Gateway-Firmware bauen (PlatformIO) und per USB flashen,
 - CC2510-Tag-Firmware bauen (SDCC) und ueber den ESP32-S3-CCLib-Debugger flashen.

Alle Aktionen streamen ihre Ausgabe live in den Browser (text/plain).
Nicht in der Cloud getestet - als Startpunkt gedacht, lokal auf HA iterieren.
"""
import os, subprocess, glob
from flask import Flask, Response, request

REPO_URL     = os.environ.get("REPO_URL", "https://github.com/marcel7162/CC2510-ESL")
BRANCH       = os.environ.get("BRANCH", "main")
GATEWAY_HOST = os.environ.get("GATEWAY_HOST", "pricetag-ap.local")
WIFI_SSID    = os.environ.get("WIFI_SSID", "")
WIFI_PASS    = os.environ.get("WIFI_PASS", "")

REPO   = "/data/repo"
GW_DIR = os.path.join(REPO, "gateway", "gateway_oepl_shim")
FW_DIR = os.path.join(REPO, "firmware")

app = Flask(__name__)


def stream(argv, cwd=None):
    """Startet ein Kommando und liefert dessen Ausgabe zeilenweise (Generator)."""
    yield "$ " + " ".join(argv) + "\n"
    try:
        p = subprocess.Popen(argv, cwd=cwd, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, bufsize=1, universal_newlines=True)
    except FileNotFoundError as e:
        yield "FEHLER: %s\n" % e
        return
    for line in p.stdout:
        yield line
    p.wait()
    yield "\n[exit %d]\n" % p.returncode


def txt(gen):
    return Response(gen, mimetype="text/plain; charset=utf-8")


def ensure_repo():
    """git clone (erstes Mal) bzw. hartes Update auf origin/BRANCH."""
    if os.path.isdir(os.path.join(REPO, ".git")):
        yield from stream(["git", "-C", REPO, "fetch", "origin", BRANCH])
        yield from stream(["git", "-C", REPO, "reset", "--hard", "origin/" + BRANCH])
    else:
        yield from stream(["git", "clone", "-b", BRANCH, REPO_URL, REPO])


def write_credentials():
    """Schreibt credentials.h fuer den Gateway-Build aus den Addon-Optionen."""
    if not WIFI_SSID:
        yield "WARN: wifi_ssid leer -> Platzhalter; Gateway wird kein WLAN verbinden.\n"
        ssid, pw = "DEIN_WLAN", "DEIN_WLAN_PASSWORT"
    else:
        ssid, pw = WIFI_SSID, WIFI_PASS
    esc = lambda s: s.replace("\\", "\\\\").replace('"', '\\"')
    path = os.path.join(GW_DIR, "src", "credentials.h")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write('#pragma once\n#define WIFI_SSID  "%s"\n#define WIFI_PASS  "%s"\n'
                % (esc(ssid), esc(pw)))
    yield "credentials.h geschrieben (SSID=%s)\n" % ssid


def list_ports():
    ports = sorted(glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*"))
    return ports


# ---------------------------------------------------------------- Routen
@app.route("/ports")
def ports():
    return {"ports": list_ports()}


@app.route("/pull", methods=["POST"])
def pull():
    return txt(ensure_repo())


@app.route("/build-gateway", methods=["POST"])
def build_gateway():
    def gen():
        yield from ensure_repo()
        yield from write_credentials()
        yield from stream(["pio", "run"], cwd=GW_DIR)
        yield ("\nFertig. Bins unter %s/.pio/build/esp32-s3-devkitc-1/\n" % GW_DIR)
    return txt(gen())


@app.route("/flash-gateway-usb", methods=["POST"])
def flash_gateway_usb():
    port = request.form.get("port", "")
    def gen():
        if not port:
            yield "FEHLER: kein Port gewaehlt.\n"; return
        yield from stream(["pio", "run", "-t", "upload", "--upload-port", port], cwd=GW_DIR)
    return txt(gen())


@app.route("/build-tag", methods=["POST"])
def build_tag():
    panel = request.form.get("panel", "266")
    target = "display42" if panel == "42" else "display"
    def gen():
        yield from ensure_repo()
        yield from stream(["make", target], cwd=FW_DIR)
        yield ("\nFertig: %s/%s.bin\n" % (FW_DIR, target))
    return txt(gen())


@app.route("/flash-tag", methods=["POST"])
def flash_tag():
    port  = request.form.get("port", "")
    panel = request.form.get("panel", "266")
    binf  = "display42.bin" if panel == "42" else "display.bin"
    binpath = os.path.join(FW_DIR, binf)
    cclib = os.path.join(REPO, "tools", "CCLib", "Python", "cc_write_direct.py")
    def gen():
        if not port:
            yield "FEHLER: kein Port gewaehlt.\n"; return
        if not os.path.exists(binpath):
            yield "FEHLER: %s fehlt - erst 'Tag-Firmware bauen'.\n" % binf; return
        yield from stream(["python3", cclib, port, binpath])
    return txt(gen())


@app.route("/")
def index():
    return Response(INDEX_HTML, mimetype="text/html; charset=utf-8")


INDEX_HTML = """<!doctype html><html lang=de><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>Marcel PriceTag</title>
<style>
 body{font:14px/1.5 system-ui,sans-serif;margin:0;background:#15181c;color:#e6e6e6}
 header{background:#1f2630;padding:12px 16px;font-size:18px;color:#fff}
 main{padding:16px;max-width:900px;margin:0 auto}
 fieldset{border:1px solid #333;border-radius:8px;margin:0 0 16px;padding:12px 14px}
 legend{color:#9cf;padding:0 6px}
 button{background:#2a6;border:0;color:#fff;padding:8px 14px;border-radius:6px;cursor:pointer;font-size:14px}
 button.alt{background:#36c} button:disabled{opacity:.5;cursor:default}
 select{padding:6px;border-radius:6px;background:#222;color:#eee;border:1px solid #444}
 pre{background:#0c0e11;color:#b7f7b7;padding:10px;border-radius:8px;height:280px;overflow:auto;white-space:pre-wrap}
 .row{display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin:6px 0}
 .note{color:#9aa;font-size:13px}
</style></head><body>
<header>&#x1F5A5;&#xFE0F; Marcel PriceTag</header>
<main>
 <fieldset><legend>1 &middot; Quellen</legend>
   <div class=row><button onclick="run('pull')">Repo holen/aktualisieren</button>
   <span class=note>klont/aktualisiert das GitHub-Repo nach /data/repo</span></div>
 </fieldset>

 <fieldset><legend>2 &middot; ESP-Gateway</legend>
   <div class=row><button onclick="run('build-gateway')">Gateway-Firmware bauen</button>
   <span class=note>PlatformIO (laedt beim 1. Mal die ESP-Toolchain)</span></div>
   <div class=row>
     <button class=alt onclick="flash('flash-gateway-usb')">Gateway per USB flashen</button>
     <select id=gwport></select><button onclick="loadPorts()">&#x21bb;</button>
   </div>
   <p class=note>OTA (kabellos) und Browser-USB-Flash: siehe DOCS - OTA braucht erst
   ArduinoOTA + A/B-Partitionen; der Browser-Flasher liegt in <code>gateway/flasher/</code>.</p>
 </fieldset>

 <fieldset><legend>3 &middot; CC2510-Tags</legend>
   <div class=row>Panel:
     <select id=panel><option value=266>2,66"</option><option value=42>4,2"</option></select>
     <button onclick="buildTag()">Tag-Firmware bauen</button>
   </div>
   <div class=row>
     <button class=alt onclick="flashTag()">Tag flashen (CCLib)</button>
     <select id=tagport></select><button onclick="loadPorts()">&#x21bb;</button>
     <span class=note>ESP32-S3-Debugger per USB am HA-Host</span>
   </div>
 </fieldset>

 <pre id=out>Bereit.</pre>
</main>
<script>
const out=document.getElementById('out');
function log(t){out.textContent=t;out.scrollTop=out.scrollHeight;}
async function post(path,body){
  out.textContent='';
  const r=await fetch('./'+path,{method:'POST',body:body});
  const rd=r.body.getReader(),dec=new TextDecoder();let acc='';
  for(;;){const{done,value}=await rd.read();if(done)break;acc+=dec.decode(value,{stream:true});log(acc);}
}
function run(p){post(p);}
function flash(p){const port=document.getElementById('gwport').value;
  const b=new URLSearchParams();b.set('port',port);post(p,b);}
function buildTag(){const b=new URLSearchParams();b.set('panel',document.getElementById('panel').value);post('build-tag',b);}
function flashTag(){const b=new URLSearchParams();b.set('panel',document.getElementById('panel').value);
  b.set('port',document.getElementById('tagport').value);post('flash-tag',b);}
async function loadPorts(){
  const j=await(await fetch('./ports')).json();
  for(const id of ['gwport','tagport']){const s=document.getElementById(id);s.innerHTML='';
    (j.ports.length?j.ports:['(kein Port)']).forEach(p=>{const o=document.createElement('option');o.value=p;o.textContent=p;s.appendChild(o);});}
}
loadPorts();
</script>
</body></html>"""


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8099, threaded=True)

/*
 * PriceTag Gateway - Weather, full-frame rendering (Adafruit GFX)
 *
 * Renders the COMPLETE display image on the ESP32 with Adafruit GFX (real
 * fonts + graphics), RLE-compresses it and streams it to the display. The
 * CC firmware (firmware/src/main_display.c) is generic and final - change the
 * look here, never re-flash the tag.
 *
 * Needs libraries (Arduino Library Manager): "Adafruit GFX Library",
 * "RF24" (TMRh20). Fill in WiFi + location below.
 *
 * Serial: y approve pending, n reject, l list, w fetch+push now.
 *
 * Image path: two GFXcanvas1 (black + red) -> packed into the IL0373 two
 * colour planes (296x152) -> RLE tokens [runlen][value] -> streamed as
 * CMD_IMG_BEGIN / CMD_IMG_DATA* / CMD_IMG_SHOW.
 */

#include <SPI.h>
#include <RF24.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

// ===================== CONFIG - EDIT =====================
// Eigene Werte eintragen! Keine echten Zugangsdaten committen.
#define WIFI_SSID  "DEIN_WLAN"
#define WIFI_PASS  "DEIN_WLAN_PASSWORT"
#define LAT        "0.0000"        // Breitengrad deines Standorts
#define LON        "0.0000"        // Laengengrad deines Standorts
#define CITY       "Zuhause"       // Ortsname fuer den Banner (frei waehlbar)
#define TZ_STRING  "CET-1CEST,M3.5.0,M10.5.0/3"
#define WEATHER_INTERVAL 900000UL   // 15 min
// =========================================================

#define CE_PIN 1
#define CSN_PIN 2
#define NRF_CHANNEL 76
#define NRF_PAYLOAD_SIZE 32

#define ENROLL_ADDR        0x00
#define CMD_IMG_BEGIN      0x20
#define CMD_IMG_DATA       0x21
#define CMD_IMG_SHOW       0x22
#define CMD_ACK            0xF1
#define CMD_ENROLL_REQ     0xE0
#define CMD_ENROLL_ASSIGN  0xE1
#define CMD_ENROLL_PENDING 0xE2

#define CMD_PING           0xF0
#define CMD_POLL           0x23   // display -> gateway: awake, anything new?
#define CMD_NOUPDATE       0x24   // gateway -> display: nothing new, sleep
#define SCR_W 296
#define SCR_H 152
#define HBYTES 19
#define PLANE 5624            // SCR_W * HBYTES

// A display is considered offline if we hear nothing from it for this long.
// It pings us every ~30s while idle, so this is comfortably longer.
// Pull model: the display polls ~every 5 min, so it is "offline" only if we
// miss several polls in a row. Must be well above the poll interval.
#define OFFLINE_MS 900000UL

const uint8_t txAddr[5] = {0xA7, 0x5C, 0xA7, 0xAA, 0xAA};
const uint8_t rxAddr[3] = {0xA7, 0x5C, 0xA7};

RF24 radio(CE_PIN, CSN_PIN);
Preferences prefs;

GFXcanvas1 cb(SCR_W, SCR_H);   // black plane canvas
GFXcanvas1 cr(SCR_W, SCR_H);   // red plane canvas

static uint8_t planeBW[PLANE];
static uint8_t planeRD[PLANE];
static uint8_t tokens[24000];

#define MAX_ENTRIES 16
struct Entry { uint8_t uid[7]; uint8_t addr; uint8_t label[4]; };
Entry reg[MAX_ENTRIES]; uint8_t regCount = 0;
bool online[MAX_ENTRIES]; uint32_t lastFrame[MAX_ENTRIES]; uint32_t lastSeen[MAX_ENTRIES];

// Image DATA payload bytes per packet. Smaller = a mid-packet RF glitch
// corrupts fewer bytes, so each packet is far likelier to survive a noisy link
// (at the cost of more packets). Max is RADIO_MAX_PAYLOAD(29) - 2 (cmd+seq).
#define IMG_CHUNK 12

// Retransmits used during the current stream - a direct measure of link
// quality (0 = every packet acked first try).
uint16_t streamReTx = 0;

// Monotonic sequence tagged onto every reliable packet (BEGIN/DATA/SHOW). The
// display ACKs with the same seq and processes each seq exactly once, so a
// retransmit after a lost ACK is recognised as a duplicate and dropped instead
// of being fed into the stateful RLE decoder twice (which caused the snow).
// Never reset, so a new frame's first packet never collides with the last.
uint8_t txSeq = 0;

// Link metrics reported by the CC in its ACKs (quality of the gateway->CC path).
uint8_t lastRssi = 0, lastLqi = 0, lastFreq = 0;

#define MAX_PENDING 8
struct Pend { uint8_t uid[7]; uint8_t label[4]; };
Pend pend[MAX_PENDING]; uint8_t pendCount = 0;

bool wxValid = false; float wxTemp = 0; int wxCode = -1; uint32_t lastWeather = 0;
// Zusatzwerte fuer das volle Layout (alle optional - -999 = "kein Wert").
float wxApp = -999, wxMax = -999, wxMin = -999, wxWind = -999; int wxHum = -1;

// ---------- helpers ----------
String hexBytes(const uint8_t *p, uint8_t n, char sep) {
    String s;
    for (uint8_t i = 0; i < n; i++) { if (i && sep) s += sep; if (p[i] < 16) s += '0'; s += String(p[i], HEX); }
    s.toUpperCase(); return s;
}
bool uidEq(const uint8_t *a, const uint8_t *b){ for (uint8_t i=0;i<7;i++) if(a[i]!=b[i]) return false; return true; }
int findReg(const uint8_t *u){ for(uint8_t i=0;i<regCount;i++) if(uidEq(reg[i].uid,u)) return i; return -1; }
int findPend(const uint8_t *u){ for(uint8_t i=0;i<pendCount;i++) if(uidEq(pend[i].uid,u)) return i; return -1; }
int findAddr(uint8_t a){ for(uint8_t i=0;i<regCount;i++) if(reg[i].addr==a) return i; return -1; }
uint8_t nextFreeAddr(){ for(uint16_t a=1;a<=254;a++){ bool u=false; for(uint8_t i=0;i<regCount;i++) if(reg[i].addr==a){u=true;break;} if(!u) return a; } return 0; }
void saveReg(){ prefs.putBytes("reg", reg, regCount*sizeof(Entry)); }
void loadReg(){ size_t n=prefs.getBytes("reg",reg,sizeof(reg)); regCount=n/sizeof(Entry); for(uint8_t i=0;i<MAX_ENTRIES;i++){online[i]=false;lastFrame[i]=0;lastSeen[i]=0;} }

// CRC-8 (poly 0x07, init 0x00) - must match the CC firmware's pkt_crc().
uint8_t crc8(const uint8_t *p, uint8_t n){ uint8_t c=0; for(uint8_t i=0;i<n;i++){ c^=p[i]; for(uint8_t b=0;b<8;b++) c=(c&0x80)?(uint8_t)((c<<1)^0x07):(uint8_t)(c<<1);} return c; }

// ---------- nRF24 ----------
uint8_t rdReg(uint8_t r){ uint8_t v; digitalWrite(CSN_PIN,LOW); SPI.transfer(r&0x1F); v=SPI.transfer(0xFF); digitalWrite(CSN_PIN,HIGH); return v; }
void wrReg(uint8_t r,uint8_t v){ digitalWrite(CSN_PIN,LOW); SPI.transfer(0x20|(r&0x1F)); SPI.transfer(v); digitalWrite(CSN_PIN,HIGH); }
void crcOff(){ wrReg(0x00, rdReg(0x00)&~0x0C); }
void modeTx(){ radio.stopListening(); radio.setAddressWidth(5); radio.openWritingPipe(txAddr); }
void modeRx(){ radio.setAddressWidth(3); radio.openReadingPipe(0,rxAddr); radio.startListening(); crcOff(); }
void writeCmd(uint8_t dest,const uint8_t*data,uint8_t n){
    uint8_t p[NRF_PAYLOAD_SIZE]; memset(p,0,sizeof(p)); p[0]=dest; p[1]=n; memcpy(&p[2],data,n);
    p[n+2]=crc8(p,n+2); radio.write(p,sizeof(p));
}
// Accept only a CRC-valid ACK that echoes the seq we are waiting for. Matching
// the seq is what stops a late ACK from the PREVIOUS packet being mistaken for
// this one's ACK (which let the gateway skip ahead and desync the stream).
bool waitAck(uint32_t to,uint8_t seq){
    uint32_t t0=millis();
    while(millis()-t0<to){
        if(radio.available()){
            uint8_t raw[NRF_PAYLOAD_SIZE]; radio.read(raw,sizeof(raw)); uint8_t n=raw[1];
            if(n>=1 && (uint8_t)(n+3)<=NRF_PAYLOAD_SIZE && crc8(raw,n+2)==raw[n+2]
               && raw[2]==CMD_ACK && (n<3 || raw[4]==seq)){
                if(n>=6){ lastRssi=raw[5]; lastLqi=raw[6]; lastFreq=raw[7]; }
                return true;
            }
        }
    }
    return false;
}
bool sendReliable(uint8_t addr,const uint8_t*d,uint8_t n,uint32_t to,uint8_t tries,uint8_t seq){
    for(uint8_t t=0;t<tries;t++){ if(t) streamReTx++; modeTx(); writeCmd(addr,d,n); modeRx(); if(waitAck(to,seq)){ delay(3); return true; } }
    return false;
}
void sendCmd(uint8_t dest,const uint8_t*d,uint8_t n){ modeTx(); writeCmd(dest,d,n); modeRx(); }
// Enroll replies are fire-and-forget; send a few copies so a single drop does
// not cost the display a whole ~0.6s retry cycle (snappier connect).
void replyAssign(const uint8_t*u,uint8_t a){ uint8_t d[9]={CMD_ENROLL_ASSIGN}; memcpy(&d[1],u,7); d[8]=a; for(uint8_t k=0;k<3;k++){ sendCmd(ENROLL_ADDR,d,9); delay(5);} }
void replyPending(const uint8_t*u){ uint8_t d[8]={CMD_ENROLL_PENDING}; memcpy(&d[1],u,7); for(uint8_t k=0;k<3;k++){ sendCmd(ENROLL_ADDR,d,8); delay(5);} }
void replyAck(uint8_t dest,uint8_t seq){ uint8_t d[3]={CMD_ACK,0,seq}; sendCmd(dest,d,3); }

// ---------- weather ----------
enum { IC_SUN, IC_CLOUD, IC_RAIN, IC_SNOW, IC_PARTLY, IC_FOG };
int iconFor(int c){
    if(c==0) return IC_SUN;
    if(c==1||c==2) return IC_PARTLY;
    if(c==3) return IC_CLOUD;
    if(c==45||c==48) return IC_FOG;
    if((c>=51&&c<=67)||(c>=80&&c<=82)||c>=95) return IC_RAIN;
    if((c>=71&&c<=77)||c==85||c==86) return IC_SNOW;
    return IC_CLOUD;
}
const char* labelFor(int c){
    if(c==0) return "Klar"; if(c==1) return "Heiter"; if(c==2) return "Wolkig"; if(c==3) return "Bedeckt";
    if(c==45||c==48) return "Nebel"; if(c>=51&&c<=57) return "Niesel"; if(c>=61&&c<=67) return "Regen";
    if(c>=71&&c<=77) return "Schnee"; if(c>=80&&c<=82) return "Schauer"; if(c==85||c==86) return "Schnee";
    if(c>=95) return "Gewitter"; return "---";
}

// Draw a weather icon centred at (cx,cy). Sun/sun-parts -> red, cloud/rain -> black.
void drawCloud(int cx, int cy) {
    cb.fillCircle(cx-16, cy+4, 11, 1);
    cb.fillCircle(cx,    cy-4, 15, 1);
    cb.fillCircle(cx+16, cy+4, 11, 1);
    cb.fillRect(cx-16, cy+2, 34, 14, 1);
}
void drawIcon(int kind, int cx, int cy) {
    if (kind == IC_SUN) {
        cr.fillCircle(cx, cy, 16, 1);
        for (int k=0;k<8;k++){ float a=k*PI/4; cr.drawLine(cx+20*cos(a),cy+20*sin(a),cx+27*cos(a),cy+27*sin(a),1); cr.drawLine(cx+20*cos(a)+1,cy+20*sin(a),cx+27*cos(a)+1,cy+27*sin(a),1);}
    } else if (kind == IC_PARTLY) {
        cr.fillCircle(cx-10, cy-12, 11, 1);
        for (int k=0;k<8;k++){ float a=k*PI/4; cr.drawLine(cx-10+14*cos(a),cy-12+14*sin(a),cx-10+19*cos(a),cy-12+19*sin(a),1);}
        drawCloud(cx+4, cy+8);
    } else if (kind == IC_CLOUD || kind == IC_FOG) {
        drawCloud(cx, cy);
        if (kind==IC_FOG){ for(int yy=cy+20; yy<=cy+30; yy+=5) cb.fillRect(cx-22,yy,44,2,1); }
    } else if (kind == IC_RAIN) {
        drawCloud(cx, cy-6);
        for(int dx=-14;dx<=14;dx+=14) cb.drawLine(cx+dx,cy+14,cx+dx-4,cy+24,1);
    } else { // snow
        drawCloud(cx, cy-6);
        for(int dx=-14;dx<=14;dx+=14) cb.fillCircle(cx+dx,cy+18,3,1);
    }
}

// Kleines Grad-Ringlein hinter einer 9pt-Zahl: Label + gerundeter Wert + "°".
// Gibt nichts aus, wenn der Wert fehlt (-999), damit das Layout Luecken schluckt.
void printTempLine(int x, int y, const char* pre, float v) {
    if (v <= -900) return;
    char b[8]; snprintf(b,sizeof(b),"%d",(int)lroundf(v));
    cb.setFont(&FreeSans9pt7b); cb.setTextColor(1);
    cb.setCursor(x,y);
    if (pre && *pre) cb.print(pre);
    cb.print(b);
    cb.drawCircle(cb.getCursorX()+4, y-8, 2, 1);   // kleines Gradzeichen
}

void renderWeather(const char* label) {
    char buf[16], clock[6], line[20];
    struct tm t;
    bool haveTime = getLocalTime(&t,50);
    if (haveTime) strftime(clock,6,"%H:%M",&t); else strcpy(clock,"--:--");
    static const char* wd[] = {"So","Mo","Di","Mi","Do","Fr","Sa"};

    cb.fillScreen(0);
    cr.fillScreen(0);

    // --- Banner (rote Leiste, weiss ausgestanzt): Ort links, Tag+Uhr rechts ---
    cr.fillRect(0,0,SCR_W,28,1);
    cr.setFont(&FreeSans9pt7b);
    cr.setTextColor(0);               // 0 = punch white holes in the red bar
    cr.setCursor(6,20); cr.print(CITY);
    if (haveTime) snprintf(line,sizeof(line),"%s %s", wd[t.tm_wday & 7], clock);
    else          snprintf(line,sizeof(line),"%s", clock);
    int16_t bx,by; uint16_t bw,bh;
    cr.getTextBounds(line,0,0,&bx,&by,&bw,&bh);
    cr.setCursor(SCR_W-6-bw,20); cr.print(line);

    if (wxValid) {
        // --- Grosse Temperatur (schwarz) ---
        snprintf(buf,sizeof(buf),"%d",(int)lroundf(wxTemp));
        cb.setFont(&FreeSansBold24pt7b); cb.setTextColor(1);
        cb.setCursor(8,84); cb.print(buf);
        cb.getTextBounds(buf,8,84,&bx,&by,&bw,&bh);
        int dx = 8 + bw + 10;
        cb.drawCircle(dx, 52, 5, 1); cb.drawCircle(dx, 52, 4, 1);     // Grad
        cb.setFont(&FreeSansBold12pt7b);
        cb.setCursor(dx+9, 84); cb.print("C");

        // --- Zustand (rot) ---
        cr.setFont(&FreeSansBold12pt7b); cr.setTextColor(1);
        cr.setCursor(8, 112); cr.print(labelFor(wxCode));

        // --- Gefuehlt (klein, schwarz) unter dem Zustand ---
        printTempLine(8, 136, "gefuehlt ", wxApp);

        // --- Icon oben rechts ---
        drawIcon(iconFor(wxCode), 236, 54);

        // --- Rechte Spalte: Hoch/Tief/Feuchte ---
        printTempLine(183, 98,  "Hoch ", wxMax);
        printTempLine(183, 118, "Tief ", wxMin);
        if (wxHum >= 0) {
            cb.setFont(&FreeSans9pt7b); cb.setTextColor(1);
            snprintf(line,sizeof(line),"%d%%", wxHum);
            cb.setCursor(183,138); cb.print(line);
            if (wxWind > -900) {
                snprintf(line,sizeof(line),"%d km/h",(int)lroundf(wxWind));
                cb.setCursor(221,138); cb.print(line);
            }
        }
    } else {
        cb.setFont(&FreeSansBold12pt7b); cb.setTextColor(1);
        cb.setCursor(10,70); cb.print("Keine Daten");
        cb.setCursor(10,100); cb.print("WLAN/Fetch?");
    }

    // --- Footer: Tag-ID klein unten links (eingebauter Font) ---
    cb.setFont(NULL);
    cb.setTextColor(1); cb.setTextSize(1);
    cb.setCursor(2,148); cb.print("ID "); cb.print(label);
}

// Pack the two canvases into the IL0373 plane byte order.
void packPlanes() {
    memset(planeBW, 0xFF, PLANE);
    memset(planeRD, 0xFF, PLANE);
    for (int x=0;x<SCR_W;x++) {
        for (int y=0;y<SCR_H;y++) {
            int p = (SCR_H-1) - y;            // panel sy
            int idx = x*HBYTES + (p>>3);
            uint8_t mask = 0x80 >> (p & 7);
            if (cb.getPixel(x,y)) planeBW[idx] &= ~mask;   // black
            else if (cr.getPixel(x,y)) planeRD[idx] &= ~mask; // red (black wins overlap)
        }
    }
}

int rleEncode(const uint8_t* data, int len, uint8_t* out, int cap) {
    int o=0,i=0;
    while(i<len){
        uint8_t v=data[i]; int run=1;
        while(i+run<len && data[i+run]==v && run<255) run++;
        if(o+2>cap) return -1;
        out[o++]=run; out[o++]=v; i+=run;
    }
    return o;
}

bool streamImage(uint8_t addr, const char* label) {
    renderWeather(label);
    packPlanes();
    int o = rleEncode(planeBW, PLANE, tokens, sizeof(tokens));
    int o2 = (o>=0) ? rleEncode(planeRD, PLANE, tokens+o, sizeof(tokens)-o) : -1;
    if (o<0 || o2<0) { Serial.println("RLE overflow!"); return false; }
    int total = o + o2;
    streamReTx = 0;
    Serial.printf("  render ok: BW=%d RD=%d total=%d RLE-Bytes, ~%d DATA-Pakete\n",
                  o, o2, total, (total + IMG_CHUNK - 1) / IMG_CHUNK);

    // Packet = [cmd][seq][payload].
    uint8_t d[2+IMG_CHUNK];
    uint8_t s;
    s = ++txSeq; d[0] = CMD_IMG_BEGIN; d[1] = s;
    // BEGIN is ACKed by the display immediately now (before its slow panel
    // wake), so a short timeout + many tries pushes it through a weak link fast
    // (instead of 3s wasted per lost attempt).
    if (!sendReliable(addr, d, 2, 150, 40, s)) { Serial.println("  BEGIN kein ACK"); return false; }
    // Link quality as the CC itself measures it: high RSSI + bad LQI = demod
    // problem (bit-sync/frequency), not weak signal.
    Serial.printf("  link@CC: RSSI=%ddBm LQI=%d (kleiner=besser) FREQEST=%d\n",
                  ((int8_t)lastRssi)/2 - 71, lastLqi & 0x7F, (int)(int8_t)lastFreq);

    int sent=0;
    for (int i=0;i<total; ) {
        int chunk = min(IMG_CHUNK, total-i);
        s = ++txSeq; d[0]=CMD_IMG_DATA; d[1]=s;
        memcpy(&d[2], &tokens[i], chunk);
        // 500ms: a single chunk can expand to thousands of SPI writes on the CC
        // (long RLE runs) and the ACK comes AFTER that. waitAck returns the
        // instant the ACK arrives, so a generous timeout costs nothing on the
        // happy path - it only gives slow chunks room (a tight 100ms stalled
        // the stream mid-image). A duplicate from a lost ACK is harmless (the
        // display dedups on seq).
        // A valid ACK for a 12-byte chunk comes back in ~20ms, so a failed try
        // need only wait ~120ms before retrying. The link is still ~40% noisy
        // so retries are frequent - the retransmit cost IS the stream time, so
        // keep each failed try cheap. 20 tries so a packet never gives up.
        // 60 tries: at a marginal (~-75dBm, ~5m) link only ~1 in 4 attempts
        // gets through, so a packet may need many retries. Enough tries let the
        // whole stream grind through (slower, but it completes - fine for a
        // 5-min background update) instead of aborting on one unlucky packet.
        if (!sendReliable(addr, d, 2+chunk, 80, 60, s)) {
            Serial.printf("  DATA kein ACK @%d (chunk=%d seq=%d von total=%d, %d Retransmits bis hier) -> Abbruch, kein SHOW\n",
                          i, chunk, s, total, streamReTx);
            return false;
        }
        i += chunk; sent++;
    }

    s = ++txSeq; d[0]=CMD_IMG_SHOW; d[1]=s;
    bool ok = sendReliable(addr, d, 2, 60000, 3, s);   // refresh takes ~25-30s
    Serial.printf("  stream %d RLE-Bytes in %d Paketen, %d Retransmits, refresh %s\n",
                  total, sent, streamReTx, ok?"OK":"FAIL");
    return ok;
}

bool fetchWeather() {
    if (WiFi.status()!=WL_CONNECTED) return false;
    WiFiClientSecure c; c.setInsecure();
    HTTPClient https;
    // current = jetzt-Werte, daily = Tages-Hoch/Tief. forecast_days=1 haelt die
    // Antwort klein. timezone=auto, damit daily auf den lokalen Tag bezogen ist.
    String url=String("https://api.open-meteo.com/v1/forecast?latitude=")+LAT+"&longitude="+LON
        +"&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m"
        +"&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto";
    if(!https.begin(c,url)) return false;
    int code=https.GET(); bool ok=false;
    if(code==200){ String p=https.getString();
        // Anchor at the "current": data block - otherwise indexOf hits the
        // "current_units" section first, where temperature_2m is the STRING
        // "°C" (-> toFloat 0.0) and weather_code is "wmo code" (-> toInt 0).
        int cur=p.indexOf("\"current\":");
        int ti=(cur>=0)?p.indexOf("temperature_2m\":",cur):-1;
        int ci=(cur>=0)?p.indexOf("weather_code\":",cur):-1;
        if(ti>=0&&ci>=0){
            wxTemp=p.substring(ti+16).toFloat(); wxCode=p.substring(ci+14).toInt();
            wxValid=true; ok=true;
            // Zusatzwerte - fehlen sie, bleibt der jeweilige Default (-999/-1)
            // und das Layout blendet die Zeile einfach aus.
            int ai=p.indexOf("apparent_temperature\":",cur);
            int hi=p.indexOf("relative_humidity_2m\":",cur);
            int wi=p.indexOf("wind_speed_10m\":",cur);
            wxApp = (ai>=0)? p.substring(ai+22).toFloat() : -999;
            wxHum = (hi>=0)? p.substring(hi+22).toInt()   : -1;
            wxWind= (wi>=0)? p.substring(wi+16).toFloat() : -999;
            // daily-Bloecke sind Arrays: "temperature_2m_max":[23.4]
            int dly=p.indexOf("\"daily\":");
            int xi=(dly>=0)?p.indexOf("temperature_2m_max\":[",dly):-1;
            int ni=(dly>=0)?p.indexOf("temperature_2m_min\":[",dly):-1;
            wxMax = (xi>=0)? p.substring(xi+21).toFloat() : -999;
            wxMin = (ni>=0)? p.substring(ni+21).toFloat() : -999;
        } }
    https.end();
    Serial.printf("[wx] %d temp=%.1f app=%.1f hum=%d wind=%.1f max=%.1f min=%.1f code=%d %s\n",
                  code, wxTemp, wxApp, wxHum, wxWind, wxMax, wxMin, wxCode, ok?"OK":"FAIL");
    return ok;
}

void sendFrame(uint8_t idx){
    Serial.printf("[%lu] render+stream -> 0x%02X\n", millis(), reg[idx].addr);
    streamImage(reg[idx].addr, hexBytes(reg[idx].label,4,0).c_str());
    lastFrame[idx]=millis();
}

// ---------- enroll ----------
void handleReq(const uint8_t*uid,const uint8_t*label){
    int idx=findReg(uid);
    if(idx>=0){ replyAssign(reg[idx].uid,reg[idx].addr); lastSeen[idx]=millis();
        if(!online[idx]){ online[idx]=true; lastFrame[idx]=millis()-WEATHER_INTERVAL;
            Serial.printf("[online] %s Addr 0x%02X\n", hexBytes(reg[idx].label,4,0).c_str(), reg[idx].addr); }
        return; }
    if(findPend(uid)<0 && pendCount<MAX_PENDING){ memcpy(pend[pendCount].uid,uid,7); memcpy(pend[pendCount].label,label,4); pendCount++;
        Serial.printf("\n*** NEU *** %s  UID %s  -> 'y'/'n'\n", hexBytes(label,4,0).c_str(), hexBytes(uid,7,':').c_str()); }
    replyPending(uid);
}
void approveOldest(){ if(!pendCount){Serial.println("(keine)");return;} uint8_t a=nextFreeAddr(); if(!a||regCount>=MAX_ENTRIES){Serial.println("voll");return;}
    memcpy(reg[regCount].uid,pend[0].uid,7); reg[regCount].addr=a; memcpy(reg[regCount].label,pend[0].label,4); online[regCount]=false; lastFrame[regCount]=0; regCount++; saveReg();
    Serial.printf("FREIGEGEBEN %s -> 0x%02X\n", hexBytes(pend[0].label,4,0).c_str(), a);
    for(uint8_t i=1;i<pendCount;i++) pend[i-1]=pend[i]; pendCount--; }
void rejectOldest(){ if(!pendCount)return; for(uint8_t i=1;i<pendCount;i++) pend[i-1]=pend[i]; pendCount--; }
void listAll(){ Serial.printf("\n-- Reg(%u) --\n",regCount); for(uint8_t i=0;i<regCount;i++) Serial.printf(" 0x%02X %s %s\n",reg[i].addr,hexBytes(reg[i].label,4,0).c_str(),online[i]?"online":"-");
    Serial.printf("-- Pend(%u) --\n",pendCount); for(uint8_t i=0;i<pendCount;i++) Serial.printf(" %s\n",hexBytes(pend[i].label,4,0).c_str()); }
// Force a fresh stream to every REGISTERED display (not just the ones marked
// online), so 'w' re-pushes to a tag that is already running in do_serve
// without needing it power-cycled to re-enroll.
void pushAll(){ for(uint8_t i=0;i<regCount;i++){ online[i]=true; lastSeen[i]=millis(); lastFrame[i]=millis()-WEATHER_INTERVAL; } }
// Toggle WiFi off/on to A/B test whether the ESP's own 2.4GHz WiFi is jamming
// the co-located nRF24. Stream with WiFi OFF and compare the Retransmit count.
void toggleWifi(){
    if(WiFi.getMode()==WIFI_OFF){ WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASS); Serial.println("WiFi AN"); }
    else { WiFi.disconnect(true,true); WiFi.mode(WIFI_OFF); Serial.println("WiFi AUS (Funk-Test)"); }
}
void handleSerial(){ while(Serial.available()){ char c=Serial.read();
    if(c=='y'||c=='Y')approveOldest(); else if(c=='n'||c=='N')rejectOldest(); else if(c=='l'||c=='L')listAll();
    else if(c=='w'||c=='W'){fetchWeather();Serial.println("Wetter geholt - Display zeigt es beim naechsten Poll");} else if(c=='x'||c=='X')toggleWifi(); } }

void setup(){
    Serial.begin(115200); while(!Serial) delay(10);
    Serial.println("\n=== PriceTag Gateway (Weather / full-frame) ===");
    prefs.begin("pricetag",false); loadReg();
    Serial.printf("Registry: %u\n", regCount);

    WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASS); Serial.print("WiFi");
    for(int i=0;i<40 && WiFi.status()!=WL_CONNECTED;i++){ delay(250); Serial.print("."); }
    Serial.println(WiFi.status()==WL_CONNECTED?" ok":" FEHLT");
    if(WiFi.status()==WL_CONNECTED){ configTzTime(TZ_STRING,"pool.ntp.org","time.google.com"); fetchWeather(); }

    SPI.begin(3,9,7,CSN_PIN);
    if(!radio.begin(&SPI)){ Serial.println("ERROR nRF24"); while(1) delay(1000); }
    radio.setChannel(NRF_CHANNEL); radio.setDataRate(RF24_250KBPS); radio.setPALevel(RF24_PA_MAX);
    radio.setPayloadSize(NRF_PAYLOAD_SIZE); radio.setAutoAck(false); radio.setRetries(0,0);
    radio.disableDynamicPayloads(); radio.disableCRC(); modeRx();
    Serial.println("Bereit (Pull: Display pollt ~alle 5min). y/n/l Enroll, w=Wetter holen, x=WiFi\n");
}

void loop(){
    handleSerial();
    if(millis()-lastWeather>=WEATHER_INTERVAL){ lastWeather=millis(); fetchWeather(); }

    // Incoming from displays: raw = [srcAddr][len][cmd...][crc].
    if(radio.available()){ uint8_t raw[NRF_PAYLOAD_SIZE]; radio.read(raw,sizeof(raw)); uint8_t len=raw[1];
        if(len>=1 && (uint8_t)(len+3)<=NRF_PAYLOAD_SIZE && crc8(raw,len+2)==raw[len+2]){
            uint8_t cmd=raw[2];
            if(cmd==CMD_ENROLL_REQ && len>=12){ handleReq(&raw[3],&raw[10]); }
            else if(cmd==CMD_POLL){                       // display woke and polls for an update
                uint8_t src=raw[0]; int idx=findAddr(src);
                if(len>=3) Serial.printf("   [CC-Downlink] RSSI=%ddBm  rx_bad=%u\n",
                                         ((int8_t)raw[3])/2 - 71, raw[4]);
                if(idx>=0){ lastSeen[idx]=millis();
                    if(!online[idx]){ online[idx]=true;
                        Serial.printf("[online] %s Addr 0x%02X\n", hexBytes(reg[idx].label,4,0).c_str(), reg[idx].addr); }
                    if(wxValid){ Serial.printf("[%lu] poll -> stream 0x%02X\n", millis(), src); sendFrame(idx); }
                    else { uint8_t d[1]={CMD_NOUPDATE}; sendCmd(src,d,1); Serial.printf("[%lu] poll 0x%02X -> nix (kein Wetter)\n", millis(), src); }
                }
            }
        }
    }

    // Detect displays that stopped polling (no contact for OFFLINE_MS).
    for(uint8_t i=0;i<regCount;i++)
        if(online[i] && (millis()-lastSeen[i] > OFFLINE_MS)){
            online[i]=false;
            Serial.printf("[offline] %s Addr 0x%02X\n", hexBytes(reg[i].label,4,0).c_str(), reg[i].addr);
        }
    // No unsolicited push: the display sleeps and would never hear it. Images
    // go out only in response to a CMD_POLL (above).
}

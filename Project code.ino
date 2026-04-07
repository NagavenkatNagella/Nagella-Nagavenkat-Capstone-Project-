#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP_Mail_Client.h>

// ============================================================
//   CONFIGURATION
// ============================================================

const char* ssid     = "Nani";
const char* password = "1234567 ";

#define SMTP_HOST        "smtp.gmail.com"
#define SMTP_PORT        465
#define AUTHOR_EMAIL     "napariyojana@gmail.com"
#define AUTHOR_PASSWORD  "gwbd naku rgvk xvrr"
#define RECIPIENT_EMAIL  "n.nagavenkat26@gmail.com"

const char* SHEETS_URL =
  "https://script.google.com/macros/s/AKfycbwSMAYhpriGbn_jFsSjzy6WSJjZEkvq7l4gN2VPKHsAqcrCuYyZnPcDpWragGluBUy2/exec";

const unsigned long SHEETS_LOG_INTERVAL = 15000UL;

// ============================================================
//   HARDWARE
// ============================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define DHTPIN   D4
#define DHTTYPE  DHT11
DHT dht(DHTPIN, DHTTYPE);

#define TOUCH1  D5
#define TOUCH2  D6
#define TOUCH3  D7

// ============================================================
//   WEB SERVER & EMAIL
// ============================================================

ESP8266WebServer server(80);
SMTPSession      smtp;

// ============================================================
//   GPS LOCATIONS
// ============================================================

const int   NUM_TURBINES              = 3;
const float TURBINE_LAT[NUM_TURBINES] = {9.575062f, 9.575180f, 9.575310f};
const float TURBINE_LON[NUM_TURBINES] = {77.675734f, 77.675890f, 77.676050f};
const float DEFAULT_LAT               = 9.575062f;
const float DEFAULT_LON               = 77.675734f;

// ============================================================
//   GLOBAL STATE
// ============================================================

float        temperature    = 0.0f;
float        humidity       = 0.0f;
String       statusMessage  = "Initializing";
bool         threatDetected = false;
String       threatTurbines = "";
bool         emailSent      = false;
bool         sheetsLogged   = false;
int          sheetsLogCount = 0;

// pendingThreatAction: set true when a NEW threat is detected.
// The main loop will then: 1) log to Sheets, 2) send email.
bool         pendingThreatAction = false;

const unsigned long EMAIL_COOLDOWN       = 300000UL;  // 5 minutes
unsigned long       lastEmailTime        = 0UL;
const unsigned long SENSOR_READ_INTERVAL = 1000UL;
unsigned long       lastSensorReadTime   = 0UL;
const unsigned long LCD_UPDATE_INTERVAL  = 2000UL;
unsigned long       lastLCDUpdateTime    = 0UL;
unsigned long       lastSheetsLogTime    = 0UL;
bool                sheetsUrlOk          = false;

unsigned long startTime    = 0UL;
int           threatCount  = 0;
float         maxTemp      = -999.0f;
float         minTemp      =  999.0f;
float         avgTemp      = 0.0f;
long          tempReadings = 0;

// ============================================================
//   PROTOTYPES
// ============================================================

void   smtpCallback(SMTP_Status status);
void   sendEmailNotification(const String& st, const String& turb);
void   sendIPAddressEmail();
void   readSensorsAndCheckThreats();
void   updateLCD();
void   logToGoogleSheets();
String getUptimeString();
void   handleRoot();
void   handleData();
void   handleStats();

// ============================================================
//   FULL HTML — single PROGMEM block
// ============================================================
static const char HTML_PAGE[] PROGMEM = R"RAW(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GreenFlow – VAWT Monitor</title>
<link href="https://fonts.googleapis.com/css2?family=DM+Sans:wght@400;600;700&family=DM+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
/* ── HIDE ALL SCROLLBARS ── */
*{margin:0;padding:0;box-sizing:border-box;scrollbar-width:none}
*::-webkit-scrollbar{display:none}
html,body{overflow-x:hidden}

/* ── DESIGN TOKENS ── */
:root{
  --bg:#f0f7fb;
  --sf:#ffffff;
  --bd:#e0eef5;
  --br:#00b4d8;
  --brd:#0096c7;
  --brl:#e0f7fc;
  --tp:#0d2137;
  --ts:#5f8199;
  --tm:#9ab5c5;
  --gr:#00c896;
  --gl:#e0faf3;
  --rd:#f05252;
  --rl:#fdecea;
  --sh0:0 1px 3px rgba(0,70,120,.07);
  --sh1:0 4px 16px rgba(0,70,120,.1);
  --r:16px;--rs:10px;
}

body{font-family:'DM Sans',sans-serif;background:var(--bg);color:var(--tp);min-height:100vh;font-size:15px}

/* ════════════════════════════════════════════
   SPLASH SCREEN
   — full-screen sky-blue gradient
   — each element zooms out + fades in one by one
   — university name types out letter-by-letter, centered
════════════════════════════════════════════ */
#splash{
  position:fixed;inset:0;z-index:9999;
  background:linear-gradient(160deg,
    #e8f9fd 0%,
    #c2edf8 18%,
    #8dddf2 36%,
    #4ec9e8 54%,
    #00b4d8 72%,
    #0096c7 88%,
    #006f99 100%);
  display:flex;flex-direction:column;align-items:center;justify-content:center;
  overflow:hidden;
  /* exit transition */
  transition:opacity 1.4s cubic-bezier(.77,0,.18,1),transform 1.4s cubic-bezier(.77,0,.18,1);
}
#splash.exit{opacity:0;transform:scale(1.06) translateY(-14px);pointer-events:none}

/* subtle grid mesh */
.mesh{position:absolute;inset:0;pointer-events:none;
  background-image:
    linear-gradient(rgba(255,255,255,.08) 1px,transparent 1px),
    linear-gradient(90deg,rgba(255,255,255,.08) 1px,transparent 1px);
  background-size:48px 48px;
  animation:meshScroll 20s linear infinite}
@keyframes meshScroll{from{background-position:0 0}to{background-position:48px 48px}}

/* floating cloud blobs */
.cloud{position:absolute;border-radius:50%;pointer-events:none;animation:cloudDrift var(--cd,18s) ease-in-out infinite alternate}
@keyframes cloudDrift{from{transform:translate(0,0)}to{transform:translate(var(--cx,24px),var(--cy,-18px))}}

/* scan beam */
.scanbeam{position:absolute;width:100%;height:4px;
  background:linear-gradient(90deg,transparent 0%,rgba(255,255,255,.5) 40%,rgba(0,200,150,.6) 60%,transparent 100%);
  animation:scanbeam 5s linear infinite;top:0;opacity:0;pointer-events:none}
@keyframes scanbeam{0%{top:-6px;opacity:0}5%{opacity:1}95%{opacity:.8}100%{top:100vh;opacity:0}}

/* spark particles */
.spark{position:absolute;pointer-events:none;border-radius:50%;
  animation:sparkrise var(--sd,7s) ease-in-out infinite var(--sdl,0s)}
@keyframes sparkrise{
  0%{transform:translate(0,0) scale(1);opacity:0}
  8%{opacity:1}90%{opacity:.5}
  100%{transform:translate(var(--sx,0px),var(--sy,-160px)) scale(0);opacity:0}}

/* ring halos */
.halo{position:absolute;top:50%;left:50%;border-radius:50%;
  border:1.5px solid rgba(255,255,255,.45);
  transform:translate(-50%,-50%) scale(.5);opacity:0;
  animation:haloExpand 3.2s ease-out infinite}
.halo:nth-child(2){animation-delay:1.07s}
.halo:nth-child(3){animation-delay:2.14s}
@keyframes haloExpand{
  0%{transform:translate(-50%,-50%) scale(.4);opacity:.9}
  100%{transform:translate(-50%,-50%) scale(2.6);opacity:0}}

/* blinking cursor */
.cursor{display:inline-block;width:2px;height:1em;background:#00b4d8;
  margin-left:3px;vertical-align:middle;animation:blink .7s step-end infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:0}}

/* ─── SPLASH CONTENT WRAPPER ─── */
.sp-content{
  position:relative;z-index:10;
  display:flex;flex-direction:column;align-items:center;
  text-align:center;padding:24px 20px;width:100%;max-width:700px}

/* ─── ZOOM-OUT REVEAL keyframe ─── */
@keyframes zoomReveal{
  from{opacity:0;transform:scale(1.35)}
  to  {opacity:1;transform:scale(1)}}

/* ─── University name pill — dead-centre of viewport, all sides equal ─── */
/* The pill lives as a direct child of #splash (position:fixed),            */
/* so top:50% + left:50% + translate(-50%,-50%) puts it pixel-perfect       */
/* in the middle regardless of screen size.                                  */
.sp-univ-wrap{
  position:absolute;
  top:50%;left:50%;
  transform:translate(-50%,-50%);
  z-index:20;
  width:90%;max-width:780px;       /* room for the full university name */
  display:flex;align-items:center;justify-content:center;
  pointer-events:none}
.sp-univ{
  font-size:14px;font-weight:700;letter-spacing:2px;text-transform:uppercase;
  color:#0d2137;
  background:rgba(255,255,255,.85);
  border:2px solid rgba(0,180,216,.45);
  border-radius:28px;
  padding:10px 32px;
  backdrop-filter:blur(10px);
  box-shadow:0 4px 24px rgba(0,100,180,.15);
  text-align:center;white-space:nowrap;
  /* starts invisible; JS adds .visible to fade it in */
  opacity:0;
  transition:opacity .35s ease}
.sp-univ.visible{opacity:1}

/* Once typing is done the pill slides upward to make room for the rest */
.sp-univ-wrap.rise{
  transition:top 0.9s cubic-bezier(.4,0,.2,1), transform 0.9s cubic-bezier(.4,0,.2,1), opacity 0.6s ease;
  top:8%;
  transform:translate(-50%,0)}

/* ─── turbine ─── */
#spTurb{
  opacity:0;
  margin-bottom:18px;position:relative}
#spTurb.show{
  animation:zoomReveal .85s cubic-bezier(.34,1.3,.64,1) forwards}

/* ─── title ─── */
.sp-title{
  font-size:52px;font-weight:700;line-height:1.1;
  color:#0d2137;
  margin-bottom:8px;
  opacity:0}
.sp-title.show{
  animation:zoomReveal .85s cubic-bezier(.34,1.2,.64,1) forwards}
.sp-title .accent{color:#00b4d8;position:relative}
.sp-title .accent::after{
  content:'';position:absolute;left:0;bottom:-4px;
  width:0;height:3px;border-radius:2px;
  background:linear-gradient(90deg,#00b4d8,#00c896);
  transition:width 1.1s ease .4s}
.sp-title .accent.show::after{width:100%}

/* ─── subtitle ─── */
.sp-sub{
  font-size:13px;letter-spacing:2.5px;text-transform:uppercase;
  color:#006f99;margin-bottom:30px;
  opacity:0}
.sp-sub.show{
  animation:zoomReveal .8s cubic-bezier(.34,1.2,.64,1) forwards}

/* ─── team card ─── */
.sp-card{
  background:rgba(255,255,255,.82);
  border:1.5px solid rgba(0,180,216,.3);
  border-radius:16px;
  box-shadow:0 8px 32px rgba(0,100,180,.12);
  backdrop-filter:blur(10px);
  padding:22px 32px;max-width:480px;width:100%;
  opacity:0}
.sp-card.show{
  animation:zoomReveal .9s cubic-bezier(.34,1.1,.64,1) forwards}

.sp-card-label{
  font-size:9px;font-weight:700;letter-spacing:3px;text-transform:uppercase;
  color:#0096c7;margin-bottom:8px}
.sp-team-id{
  font-size:16px;font-weight:700;color:#0d2137;
  font-family:'DM Mono',monospace;margin-bottom:14px}
.sp-members{display:flex;flex-wrap:wrap;gap:8px;justify-content:center}
.sp-member{
  background:#e0f7fc;
  border:1.5px solid #00b4d8;
  border-radius:20px;padding:6px 16px;
  font-size:12px;font-weight:600;color:#0096c7;
  opacity:0}
.sp-member.show{
  animation:zoomReveal .55s cubic-bezier(.34,1.56,.64,1) forwards}

.sp-divider{width:60px;height:2px;background:linear-gradient(90deg,#00b4d8,#00c896);
  border-radius:2px;margin:16px auto 0;opacity:0;transition:opacity .6s ease}
.sp-divider.show{opacity:1}

/* ─── progress ─── */
.sp-prog-wrap{
  width:260px;height:4px;background:rgba(0,100,180,.15);
  border-radius:4px;margin-top:30px;overflow:hidden;
  opacity:0;transition:opacity .5s ease}
.sp-prog-wrap.show{opacity:1}
.sp-prog-bar{
  height:100%;width:0%;border-radius:4px;
  background:linear-gradient(90deg,#00b4d8,#00c896);
  box-shadow:0 0 10px rgba(0,180,216,.5);
  transition:width .08s linear}
.sp-launch{
  font-size:10px;letter-spacing:2px;text-transform:uppercase;
  color:#006f99;margin-top:9px;
  opacity:0;transition:opacity .5s ease;min-height:16px}
.sp-launch.show{opacity:1}

/* ════════════════════════════════════════════
   NAV
════════════════════════════════════════════ */
nav{background:var(--sf);border-bottom:1px solid var(--bd);display:flex;align-items:center;
  justify-content:space-between;padding:0 32px;height:58px;
  position:sticky;top:0;z-index:100;box-shadow:var(--sh0)}
.nav-logo{display:flex;align-items:center;gap:8px;font-weight:700;font-size:17px;color:var(--br)}
.nav-links{display:flex;gap:4px;list-style:none}
.nav-links a{display:flex;align-items:center;gap:6px;padding:6px 13px;border-radius:8px;
  text-decoration:none;color:var(--ts);font-weight:500;font-size:13px;transition:all .2s}
.nav-links a:hover,.nav-links a.active{background:var(--brl);color:var(--brd)}
.live-badge{display:flex;align-items:center;gap:6px;background:var(--brl);color:var(--brd);
  border:1.5px solid var(--br);border-radius:20px;padding:5px 14px;font-weight:600;font-size:12px}
.ldot{width:7px;height:7px;background:var(--br);border-radius:50%;animation:lp 1.4s ease-in-out infinite}
@keyframes lp{0%,100%{opacity:1;transform:scale(1)}50%{opacity:.4;transform:scale(1.4)}}

/* ════════════════════════════════════════════
   HERO
════════════════════════════════════════════ */
.hero{display:grid;grid-template-columns:1fr 380px;gap:32px;align-items:center;
  padding:48px 32px;max-width:1200px;margin:0 auto}
.hero h1{font-size:40px;font-weight:700;color:var(--br);line-height:1.15;margin-bottom:14px}
.hero p{color:var(--ts);font-size:15px;line-height:1.7;max-width:440px;margin-bottom:28px}
.hero-btns{display:flex;gap:10px;flex-wrap:wrap}
.btn-p{background:var(--br);color:#fff;border:none;padding:12px 24px;border-radius:9px;
  font-family:'DM Sans',sans-serif;font-size:14px;font-weight:600;cursor:pointer;transition:background .2s,transform .15s}
.btn-p:hover{background:var(--brd);transform:translateY(-1px)}
.btn-s{background:var(--sf);color:var(--tp);border:1.5px solid var(--bd);padding:12px 24px;
  border-radius:9px;font-family:'DM Sans',sans-serif;font-size:14px;font-weight:600;cursor:pointer;transition:all .2s}
.btn-s:hover{border-color:var(--br);color:var(--br)}
.hero-card{background:var(--sf);border:1px solid var(--bd);border-radius:var(--r);
  box-shadow:var(--sh1);overflow:hidden;display:flex;flex-direction:column}
.tc{height:200px;display:flex;align-items:center;justify-content:center;position:relative;
  overflow:hidden;background:linear-gradient(135deg,#f0f7fb,#e0f7fc)}
.turb{width:140px;height:140px;position:relative}
.tctr{width:28px;height:28px;background:linear-gradient(135deg,#e0f7fc,#00b4d8);border-radius:50%;
  position:absolute;top:56px;left:56px;z-index:2;
  box-shadow:0 0 18px rgba(0,180,216,.5);animation:cp 2s ease-in-out infinite}
.tb{position:absolute;width:110px;height:18px;
  background:linear-gradient(90deg,rgba(0,180,216,.9),rgba(0,150,199,.5));
  top:61px;left:15px;transform-origin:55px 9px;border-radius:9px}
.tb:nth-child(1){animation:sf 3s linear infinite}
.tb:nth-child(2){transform:rotate(120deg);animation:sf 3s linear infinite}
.tb:nth-child(3){transform:rotate(240deg);animation:sf 3s linear infinite}
.tpole{width:12px;height:90px;background:linear-gradient(to bottom,#b0cdd9,#8aa8b5);
  position:absolute;bottom:-56px;left:64px;border-radius:6px 6px 0 0}
@keyframes sf{0%{transform:rotate(0)}100%{transform:rotate(360deg)}}
@keyframes cp{0%,100%{box-shadow:0 0 18px rgba(0,180,216,.5)}50%{box-shadow:0 0 32px rgba(0,180,216,.8)}}
.ql{display:grid;grid-template-columns:1fr 1fr;gap:0;border-top:1px solid var(--bd)}
.ql a{display:flex;align-items:center;gap:8px;padding:13px 16px;text-decoration:none;
  font-size:12px;font-weight:600;color:var(--ts);transition:background .18s,color .18s;
  border-right:1px solid var(--bd);border-bottom:1px solid var(--bd)}
.ql a:nth-child(2n){border-right:none}
.ql a:nth-child(3),.ql a:nth-child(4){border-bottom:none}
.ql a:hover{background:var(--bg);color:var(--br)}
.ql a svg{flex-shrink:0;opacity:.65}
.ql a:hover svg{opacity:1}
.ql-sub{font-size:10px;font-weight:400;color:var(--tm);margin-top:1px;display:block}

/* ════════════════════════════════════════════
   DASHBOARD CARDS
════════════════════════════════════════════ */
.main{max-width:1200px;margin:0 auto;padding:0 32px 56px}
.sg{display:grid;grid-template-columns:repeat(5,1fr);gap:18px;margin-bottom:24px}
.sc{background:var(--sf);border:1px solid var(--bd);border-radius:var(--r);padding:22px;
  box-shadow:var(--sh0);transition:box-shadow .2s,transform .2s}
.sc:hover{box-shadow:var(--sh1);transform:translateY(-2px)}
.sc.ta{border-color:var(--rd);background:var(--rl);animation:tap 1s ease-in-out infinite}
@keyframes tap{0%,100%{box-shadow:0 0 0 0 rgba(240,82,82,.3)}50%{box-shadow:0 0 0 7px rgba(240,82,82,0)}}
.sh{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}
.sl{font-size:11px;font-weight:600;letter-spacing:.8px;text-transform:uppercase;color:var(--tm)}
.si{width:34px;height:34px;border-radius:9px;display:flex;align-items:center;justify-content:center}
.si.b{background:var(--brl);color:var(--br)}.si.g{background:var(--gl);color:var(--gr)}.si.r{background:var(--rl);color:var(--rd)}
.sv{font-size:30px;font-weight:700;color:var(--tp);margin-bottom:5px;font-family:'DM Mono',monospace}
.ss{font-size:12px;color:var(--tm)}.ss.up{color:var(--gr)}.ss.dn{color:var(--rd)}
.bg{display:grid;grid-template-columns:1fr 300px;gap:18px;margin-bottom:24px}
.cc{background:var(--sf);border:1px solid var(--bd);border-radius:var(--r);padding:26px;box-shadow:var(--sh0)}
.ch{display:flex;align-items:center;justify-content:space-between;margin-bottom:20px}
.ct{font-size:12px;font-weight:600;letter-spacing:.6px;text-transform:uppercase;color:var(--ts)}
.ca{width:30px;height:30px;border:1px solid var(--bd);border-radius:7px;display:flex;
  align-items:center;justify-content:center;color:var(--tm);cursor:pointer;transition:all .2s}
.ca:hover{border-color:var(--br);color:var(--br)}
.chart-area{position:relative;height:210px}
.cl{display:flex;gap:18px;margin-top:12px;justify-content:center}
.li{display:flex;align-items:center;gap:6px;font-size:11px;color:var(--ts);font-weight:500}
.ld{width:26px;height:3px;border-radius:2px}
.ic{background:var(--sf);border:1px solid var(--bd);border-radius:var(--r);padding:26px;box-shadow:var(--sh0)}
.ir{display:flex;justify-content:space-between;align-items:center;padding:12px 0;border-bottom:1px solid var(--bd)}
.ir:last-of-type{border-bottom:none}
.il{font-size:13px;color:var(--ts);font-weight:500}
.iv{font-size:13px;color:var(--tp);font-weight:600;font-family:'DM Mono',monospace}
.lb{width:100%;margin-top:18px;background:var(--brl);color:var(--brd);border:1.5px solid var(--br);
  border-radius:var(--rs);padding:11px;font-family:'DM Sans',sans-serif;font-weight:600;font-size:13px;
  cursor:pointer;display:flex;align-items:center;justify-content:center;gap:7px;transition:all .2s}
.lb:hover{background:var(--br);color:#fff}
.sb{width:100%;margin-top:8px;background:#e8f5e9;color:#2e7d32;border:1.5px solid #66bb6a;
  border-radius:var(--rs);padding:11px;font-family:'DM Sans',sans-serif;font-weight:600;font-size:13px;
  cursor:pointer;display:flex;align-items:center;justify-content:center;gap:7px;transition:all .2s;text-decoration:none}
.sb:hover{background:#66bb6a;color:#fff}
.cb{display:flex;align-items:center;gap:7px;padding:7px 14px;border-radius:7px;
  margin-bottom:18px;font-size:13px;font-weight:600;transition:all .4s}
.cb.ok{background:var(--gl);color:var(--gr);border:1px solid rgba(0,200,150,.3)}
.cb.err{background:var(--rl);color:var(--rd);border:1px solid rgba(240,82,82,.3)}
.cd{width:7px;height:7px;border-radius:50%;background:currentColor}
.lc{background:var(--sf);border:1px solid var(--bd);border-radius:var(--r);padding:26px;
  box-shadow:var(--sh0);margin-bottom:24px}
table{width:100%;border-collapse:collapse;margin-top:4px}
th{text-align:left;font-size:11px;font-weight:600;text-transform:uppercase;letter-spacing:.6px;
  color:var(--tm);padding:11px 14px;border-bottom:2px solid var(--bd)}
td{padding:13px 14px;font-size:12px;color:var(--ts);border-bottom:1px solid var(--bd);
  font-family:'DM Mono',monospace}
tr:last-child td{border-bottom:none}
tr:hover td{background:var(--bg)}
.pill{display:inline-flex;align-items:center;gap:5px;padding:3px 11px;border-radius:20px;
  font-size:11px;font-weight:600;font-family:'DM Sans',sans-serif}
.pill.ok{background:var(--gl);color:var(--gr)}.pill.th{background:var(--rl);color:var(--rd)}
.nd td{text-align:center;color:var(--tm);padding:36px;font-family:'DM Sans',sans-serif;font-style:italic}

/* ════════════════════════════════════════════
   FOOTER
════════════════════════════════════════════ */
footer{background:var(--sf);border-top:1px solid var(--bd);padding:40px 32px}
.fi{max-width:1200px;margin:0 auto;display:grid;grid-template-columns:1.5fr 1fr 1fr;gap:36px}
.fb p{color:var(--tm);font-size:13px;line-height:1.8;max-width:240px;margin-top:10px}
.fs h4{font-size:12px;font-weight:700;margin-bottom:14px;color:var(--tp)}
.fs ul{list-style:none}.fs li{margin-bottom:9px}
.fs a{color:var(--tm);text-decoration:none;font-size:12px;transition:color .2s}.fs a:hover{color:var(--br)}

/* ════════════════════════════════════════════
   OVERLAYS / TOASTS
════════════════════════════════════════════ */
.ov{display:none;position:fixed;inset:0;background:rgba(0,0,0,.55);z-index:1000;backdrop-filter:blur(4px)}
.pp{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);width:90%;max-width:460px;
  background:var(--sf);border-radius:18px;padding:32px;
  box-shadow:0 20px 60px rgba(0,0,0,.2);border-top:4px solid var(--rd)}
.pi{text-align:center;font-size:52px;margin-bottom:10px}
.pt{text-align:center;font-size:20px;font-weight:700;color:var(--rd);margin-bottom:20px}
.pi2{background:var(--bg);border-radius:11px;padding:18px;margin-bottom:18px}
.pr{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid var(--bd);font-size:13px}
.pr:last-child{border-bottom:none}
.prl{color:var(--tm);font-weight:600;text-transform:uppercase;font-size:10px;letter-spacing:.6px}
.prv{color:var(--tp);font-weight:600}
.ab{width:100%;background:var(--rd);color:#fff;border:none;border-radius:9px;padding:14px;
  font-family:'DM Sans',sans-serif;font-size:14px;font-weight:700;cursor:pointer;transition:all .2s}
.ab:hover{background:#d63a3a}
.et{position:fixed;top:22px;right:22px;background:var(--gr);color:#fff;padding:14px 20px;
  border-radius:11px;z-index:2000;display:none;min-width:260px;box-shadow:0 8px 24px rgba(0,200,150,.3)}
.et.show{display:block;animation:sli .4s ease}
.et-t{font-weight:700;font-size:13px;margin-bottom:3px}
.et-m{font-size:11px;opacity:.9}
.et-x{position:absolute;top:9px;right:12px;background:none;border:none;color:#fff;font-size:17px;cursor:pointer;opacity:.7}
.st{position:fixed;top:80px;right:22px;background:#2e7d32;color:#fff;padding:12px 18px;
  border-radius:11px;z-index:2000;display:none;min-width:220px;
  box-shadow:0 8px 24px rgba(46,125,50,.3);font-size:12px;font-weight:600}
.st.show{display:block;animation:sli .4s ease}
@keyframes sli{from{transform:translateX(120%);opacity:0}to{transform:translateX(0);opacity:1}}

/* responsive */
@media(max-width:1024px){.sg{grid-template-columns:repeat(3,1fr)}.bg{grid-template-columns:1fr}.hero{grid-template-columns:1fr}}
@media(max-width:600px){nav{padding:0 14px}.nav-links{display:none}.hero{padding:26px 14px}.hero h1{font-size:28px}.main{padding:0 14px 36px}.sg{grid-template-columns:1fr 1fr}footer{padding:26px 14px}.fi{grid-template-columns:1fr}}
svg{flex-shrink:0}
</style>
</head>
<body>

<!-- ════════════════════════════════════════════
     SPLASH SCREEN
════════════════════════════════════════════ -->
<div id="splash">

  <div class="mesh"></div>

  <!-- floating cloud blobs -->
  <div class="cloud" style="width:600px;height:600px;background:rgba(255,255,255,.12);top:-200px;left:-150px;--cd:16s;--cx:40px;--cy:24px;border-radius:50%"></div>
  <div class="cloud" style="width:440px;height:440px;background:rgba(0,150,199,.1);bottom:-100px;right:-80px;--cd:13s;--cx:-30px;--cy:-20px;border-radius:50%"></div>
  <div class="cloud" style="width:300px;height:300px;background:rgba(255,255,255,.09);top:25%;right:8%;--cd:10s;--cx:18px;--cy:18px;border-radius:50%"></div>

  <div class="scanbeam"></div>
  <div id="sparkContainer" style="position:absolute;inset:0;pointer-events:none;overflow:hidden"></div>

  <!--
    University name pill — sits as a direct child of #splash so it can be
    positioned absolutely in the exact centre of the viewport (all sides equal).
    After typing finishes JS adds class "rise" which animates it upward,
    then the rest of .sp-content (turbine, title, team …) zooms in below it.
  -->
  <div class="sp-univ-wrap" id="spUnivWrap">
    <div class="sp-univ" id="spUniv">
      <span id="univText"></span><span class="cursor" id="univCursor"></span>
    </div>
  </div>

  <div class="sp-content">

    <!--
      ORDER OF APPEARANCE (each zooms out + fades in):
      1. University name — types out letter-by-letter at dead centre
         → pill then rises to top while rest of content appears
      2. Turbine SVG
      3. Title "GreenFlow"
      4. Subtitle
      5. Team card (members appear one by one)
      6. Progress bar → launch
    -->

    <!-- 2. Turbine SVG -->
    <div id="spTurb">
      <div style="position:relative;width:200px;height:200px">
        <svg width="200" height="200" viewBox="0 0 200 200" xmlns="http://www.w3.org/2000/svg">
          <defs>
            <radialGradient id="hubG" cx="50%" cy="50%" r="50%">
              <stop offset="0%" stop-color="#e0f7fc"/>
              <stop offset="100%" stop-color="#00b4d8"/>
            </radialGradient>
            <linearGradient id="bG1" x1="0%" y1="0%" x2="100%" y2="0%">
              <stop offset="0%" stop-color="#00b4d8" stop-opacity=".95"/>
              <stop offset="100%" stop-color="#0096c7" stop-opacity=".45"/>
            </linearGradient>
            <linearGradient id="bG2" x1="0%" y1="0%" x2="100%" y2="0%">
              <stop offset="0%" stop-color="#00c896" stop-opacity=".9"/>
              <stop offset="100%" stop-color="#00b4d8" stop-opacity=".35"/>
            </linearGradient>
            <filter id="hubGlow" x="-50%" y="-50%" width="200%" height="200%">
              <feGaussianBlur in="SourceGraphic" stdDeviation="3"/>
            </filter>
          </defs>
          <circle cx="100" cy="100" r="90" fill="none" stroke="rgba(0,180,216,.18)" stroke-width="1"/>
          <circle cx="100" cy="100" r="74" fill="none" stroke="rgba(0,180,216,.1)" stroke-width="0.5" stroke-dasharray="5 7"/>
          <g id="spBlades" style="transform-origin:100px 100px">
            <rect x="8" y="91" width="92" height="18" rx="9" fill="url(#bG1)"/>
            <rect x="8" y="91" width="92" height="18" rx="9" fill="url(#bG2)" transform="rotate(120 100 100)"/>
            <rect x="8" y="91" width="92" height="18" rx="9" fill="url(#bG1)" transform="rotate(240 100 100)"/>
          </g>
          <rect x="94" y="100" width="12" height="88" rx="6" fill="rgba(0,150,199,.35)"/>
          <circle cx="100" cy="100" r="20" fill="rgba(0,180,216,.25)" filter="url(#hubGlow)"/>
          <circle cx="100" cy="100" r="17" fill="url(#hubG)"/>
          <circle cx="100" cy="100" r="8"  fill="rgba(255,255,255,.6)"/>
        </svg>
        <div style="position:absolute;inset:0">
          <div class="halo" style="width:200px;height:200px"></div>
          <div class="halo" style="width:200px;height:200px"></div>
          <div class="halo" style="width:200px;height:200px"></div>
        </div>
      </div>
    </div>

    <!-- 3. Title -->
    <div class="sp-title" id="spTitle">Green<span class="accent" id="spAccent">Flow</span></div>

    <!-- 4. Subtitle -->
    <div class="sp-sub" id="spSub">VAWT Smart Monitoring System</div>

    <!-- 5. Team card -->
    <div class="sp-card" id="spCard">
      <div class="sp-card-label">Project Team</div>
      <div class="sp-team-id">Team 02</div>
      <div class="sp-members" id="spMembers"></div>
      <div class="sp-divider" id="spDiv"></div>
    </div>

    <!-- 6. Progress bar -->
    <div class="sp-prog-wrap" id="spProgWrap">
      <div class="sp-prog-bar" id="spProgBar"></div>
    </div>
    <div class="sp-launch" id="spLaunch">Initializing...</div>

  </div>
</div>


<!-- ════════════════════════════════════════════
     MAIN PAGE
════════════════════════════════════════════ -->
<nav>
  <div class="nav-logo">
    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2L8 8H4l4 6H5l7 8 7-8h-3l4-6h-4L12 2z"/></svg>
    GreenFlow
  </div>
  <ul class="nav-links">
    <li><a href="#" class="active">
      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/></svg>Dashboard</a></li>
    <li><a href="#">
      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/></svg>Analytics</a></li>
    <li><a href="#">
      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/></svg>Logs</a></li>
  </ul>
  <div class="live-badge"><div class="ldot"></div>Live</div>
</nav>

<div class="hero">
  <div>
    <h1>Smart Turbine<br>Monitoring</h1>
    <p>Real-time analytics for Vertical Axis Wind Turbines with live Google Sheets logging.</p>
    <div class="hero-btns">
      <button class="btn-p" onclick="document.querySelector('.main').scrollIntoView({behavior:'smooth'})">Dashboard</button>
      <button class="btn-s">View Manual</button>
    </div>
  </div>
  <div class="hero-card">
    <div class="tc">
      <div class="turb">
        <div class="tb"></div>
        <div class="tb"></div>
        <div class="tb"></div>
        <div class="tctr"></div>
        <div class="tpole"></div>
      </div>
    </div>
    <div class="ql">
      <a href="https://www.google.com/maps?q=9.575062,77.675735" target="_blank">
        <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="#00b4d8" stroke-width="2"><path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0 1 18 0z"/><circle cx="12" cy="10" r="3"/></svg>
        <span><b>Location</b><span class="ql-sub">9.575062, 77.675735</span></span>
      </a>
      <a href="https://docs.google.com/spreadsheets/d/1vI1hhfSh3Jy1U-QVmlMLhaWtvdsBknOMou_Hcs5Zt5Q/edit?gid=0#gid=0" target="_blank">
        <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="#00c896" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2"/><line x1="3" y1="9" x2="21" y2="9"/><line x1="3" y1="15" x2="21" y2="15"/><line x1="9" y1="3" x2="9" y2="21"/></svg>
        <span><b>VAWT Sheets</b><span class="ql-sub">Sensor log</span></span>
      </a>
      <a href="http://191.211.20.143" target="_blank">
        <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="#f59e0b" stroke-width="2"><rect x="2" y="7" width="20" height="15" rx="2"/><path d="M16 7V5a2 2 0 0 0-2-2h-4a2 2 0 0 0-2 2v2"/><line x1="12" y1="12" x2="12" y2="16"/><line x1="10" y1="14" x2="14" y2="14"/></svg>
        <span><b>EV Webpage</b><span class="ql-sub">191.211.20.143</span></span>
      </a>
      <a href="https://docs.google.com/spreadsheets/d/1h6nOv1JqRG0wRDfAeUf4oeaSesskI6LgHRsCG4g9xhY/edit?usp=sharing" target="_blank">
        <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="#ec4899" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2"/><line x1="3" y1="9" x2="21" y2="9"/><line x1="3" y1="15" x2="21" y2="15"/><line x1="9" y1="3" x2="9" y2="21"/></svg>
        <span><b>EV Sheets</b><span class="ql-sub">EV sensor log</span></span>
      </a>
    </div>
  </div>
</div>

<div class="main">
<div class="cb ok" id="connBar"><div class="cd"></div><span id="connText">Connected – receiving live sensor data</span></div>
<div class="sg">
  <div class="sc" id="statusCard">
    <div class="sh"><span class="sl">System Status</span><div class="si b"><svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/></svg></div></div>
    <div class="sv" id="statusValue" style="font-size:22px;font-family:'DM Sans',sans-serif">--</div>
    <div class="ss up" id="statusSub">Waiting...</div>
  </div>
  <div class="sc">
    <div class="sh"><span class="sl">Temperature</span><div class="si b"><svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z"/></svg></div></div>
    <div class="sv"><span id="tempValue">--</span>°C</div>
    <div class="ss" id="tempSub">DHT11</div>
  </div>
  <div class="sc">
    <div class="sh"><span class="sl">Humidity</span><div class="si b"><svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2.69l5.66 5.66a8 8 0 1 1-11.31 0z"/></svg></div></div>
    <div class="sv"><span id="humidValue">--</span>%</div>
    <div class="ss">DHT11</div>
  </div>
  <div class="sc">
    <div class="sh"><span class="sl">Threats</span><div class="si r"><svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg></div></div>
    <div class="sv" id="threatCount">0</div>
    <div class="ss">Session total</div>
  </div>
  <div class="sc">
    <div class="sh"><span class="sl">Sheets Rows</span><div class="si g"><svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2"/><line x1="3" y1="9" x2="21" y2="9"/><line x1="3" y1="15" x2="21" y2="15"/><line x1="9" y1="3" x2="9" y2="21"/></svg></div></div>
    <div class="sv" id="sheetsCount">0</div>
    <div class="ss" id="sheetsSub">Logged total</div>
  </div>
</div>

<div class="bg">
  <div class="cc">
    <div class="ch"><span class="ct">Live Metrics</span></div>
    <div class="chart-area"><canvas id="liveChart"></canvas></div>
    <div class="cl">
      <div class="li"><div class="ld" style="background:#00b4d8"></div>Temp °C</div>
      <div class="li"><div class="ld" style="background:#00c896"></div>Humidity %</div>
    </div>
  </div>
  <div class="ic">
    <div class="ch"><span class="ct">Uptime &amp; Info</span></div>
    <div class="ir"><span class="il">Uptime</span><span class="iv" id="uptime">--</span></div>
    <div class="ir"><span class="il">Avg Temp</span><span class="iv" id="avgTemp">--</span></div>
    <div class="ir"><span class="il">Max Temp</span><span class="iv" id="maxTemp">--</span></div>
    <div class="ir"><span class="il">Min Temp</span><span class="iv" id="minTemp">--</span></div>
    <div class="ir"><span class="il">Device IP</span><span class="iv" id="deviceIP">--</span></div>
    <div class="ir"><span class="il">Heap Free</span><span class="iv" id="heapFree">--</span></div>
    <button class="lb" onclick="openMaps()">
      <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0 1 18 0z"/><circle cx="12" cy="10" r="3"/></svg>
      Locate Turbine
    </button>
    <a class="sb" href="https://docs.google.com/spreadsheets/d/1vI1hhfSh3Jy1U-QVmlMLhaWtvdsBknOMou_Hcs5Zt5Q/edit?gid=0#gid=0" target="_blank">
      <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2"/><line x1="3" y1="9" x2="21" y2="9"/><line x1="3" y1="15" x2="21" y2="15"/><line x1="9" y1="3" x2="9" y2="21"/></svg>
      Open Sheets
    </a>
  </div>
</div>

<div class="lc">
  <div class="ch">
    <span class="ct">Event Log</span>
    <div class="ca" onclick="clearLog()">
      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"/><path d="M19 6l-1 14H6L5 6"/><path d="M10 11v6"/><path d="M14 11v6"/><path d="M9 6V4h6v2"/></svg>
    </div>
  </div>
  <table>
    <thead><tr><th>Time</th><th>Event</th><th>Turbine(s)</th><th>Temp</th><th>Humid</th><th>Status</th></tr></thead>
    <tbody id="logBody"><tr class="nd"><td colspan="6">No events yet – waiting for sensor data</td></tr></tbody>
  </table>
</div>
</div>

<footer>
  <div class="fi">
    <div class="fb">
      <div class="nav-logo" style="font-size:17px">
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2L8 8H4l4 6H5l7 8 7-8h-3l4-6h-4L12 2z"/></svg>
        GreenFlow
      </div>
      <p>Sustainable energy monitoring for Vertical Axis Wind Turbines.</p>
    </div>
    <div class="fs"><h4>Quick Links</h4><ul><li><a href="#">Dashboard</a></li><li><a href="#">Analytics</a></li><li><a href="#">Logs</a></li></ul></div>
    <div class="fs"><h4>Support</h4><ul><li><a href="#">Docs</a></li><li><a href="#">Contact</a></li><li><a href="#">Privacy</a></li></ul></div>
  </div>
</footer>

<!-- Threat overlay -->
<div class="ov" id="threatOverlay">
  <div class="pp">
    <div class="pi">⚠️</div>
    <div class="pt">Threat Detected!</div>
    <div class="pi2">
      <div class="pr"><span class="prl">Status</span><span class="prv" id="popupStatus">--</span></div>
      <div class="pr"><span class="prl">Turbine(s)</span><span class="prv" id="popupTurbines">--</span></div>
      <div class="pr"><span class="prl">Temperature</span><span class="prv" id="popupTemp">--</span></div>
      <div class="pr"><span class="prl">Humidity</span><span class="prv" id="popupHumid">--</span></div>
      <div class="pr"><span class="prl">Location</span><span class="prv" id="popupLoc" style="cursor:pointer;color:#00b4d8;text-decoration:underline" onclick="openMaps()">--</span></div>
      <div class="pr"><span class="prl">Time</span><span class="prv" id="popupTime">--</span></div>
    </div>
    <button class="ab" onclick="acknowledgeAlert()">✓ Acknowledge</button>
  </div>
</div>

<div class="et" id="emailToast">
  <button class="et-x" onclick="closeET()">×</button>
  <div class="et-t">✉ Email Alert Sent!</div>
  <div class="et-m" id="emailMsg">--</div>
</div>
<div class="st" id="sheetsToast">📊 Row logged to Google Sheets</div>

<script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.0/chart.umd.min.js"></script>
<script>
// ════════════════════════════════════════════
//  SPLASH ENGINE
//  Sequence:
//    t=0ms    : university pill appears; typing begins
//    typing   : each character at ~70ms, humanised
//    done     : cursor disappears after 800ms
//    +150ms   : turbine zooms in
//    +450ms   : title zooms in + underline grows
//    +700ms   : subtitle zooms in
//    +950ms   : team card zooms in; members pop in one-by-one
//    +1200ms  : progress bar appears, fills, then launches
// ════════════════════════════════════════════
(function(){
  var splash   = document.getElementById('splash');
  var univWrap = document.getElementById('spUnivWrap');
  var univEl   = document.getElementById('spUniv');
  var univText = document.getElementById('univText');
  var cursor   = document.getElementById('univCursor');

  /* spark particles */
  var sc = document.getElementById('sparkContainer');
  var scols = ['rgba(0,180,216,','rgba(0,200,150,','rgba(0,150,200,','rgba(180,230,255,'];
  for(var i=0;i<45;i++){
    var p=document.createElement('div'); p.className='spark';
    var sz=Math.random()*5+2;
    var col=scols[i%4];
    var sx=(Math.random()-.5)*180;
    var sy=-(Math.random()*180+60);
    p.style.cssText='width:'+sz+'px;height:'+sz+'px;background:'+col+'.75);'
      +'bottom:'+(Math.random()*40+5)+'%;left:'+(Math.random()*88+6)+'%;'
      +'--sd:'+(Math.random()*5+4)+'s;--sdl:'+(Math.random()*4)+'s;'
      +'--sx:'+sx+'px;--sy:'+sy+'px';
    sc.appendChild(p);
  }

  /* blade spin */
  var bg=document.getElementById('spBlades'), ang=0;
  function spinBlade(){ ang+=2.2; bg.style.transform='rotate('+ang+'deg)'; requestAnimationFrame(spinBlade); }
  spinBlade();

  /* ─── helper: zoom-reveal an element ─── */
  function zoomIn(el, delay){
    setTimeout(function(){
      el.style.animation = 'zoomReveal 0.85s cubic-bezier(0.34,1.3,0.64,1) forwards';
    }, delay);
  }

  /* ─── STEP 1: show pill immediately, then type university name ─── */
  var univFull = 'Kalasalingam Academy of Research and Education';
  var charIdx  = 0;
  univEl.classList.add('visible');   /* pill border fades in */

  function typeNextChar(){
    if(charIdx < univFull.length){
      univText.textContent = univFull.slice(0, ++charIdx);
      var delay = (charIdx === 1) ? 180 : 62 + Math.random()*26;
      setTimeout(typeNextChar, delay);
    } else {
      /* typing done — blink cursor for a beat then hide it */
      setTimeout(function(){ cursor.style.display='none'; }, 800);
      /* pill rises from dead-centre toward the top */
      setTimeout(function(){ univWrap.classList.add('rise'); }, 900);
      /* rest of content zooms in while pill is rising */
      setTimeout(startRestSequence, 1050);
    }
  }
  setTimeout(typeNextChar, 300);

  /* ─── STEP 2: everything else zooms in one-by-one ─── */
  function startRestSequence(){
    var members = [
      'Nagella Nagavenkat',
      'Kommineni Kavya Sree',
      'Maruprolu Jyothsna',
      'Munugu Tejasree',
      'Kumaran Gobika'
    ];

    /* turbine */
    zoomIn(document.getElementById('spTurb'), 0);

    /* title */
    setTimeout(function(){
      var t = document.getElementById('spTitle');
      t.style.animation = 'zoomReveal 0.85s cubic-bezier(0.34,1.2,0.64,1) forwards';
      document.getElementById('spAccent').classList.add('show');
    }, 280);

    /* subtitle */
    setTimeout(function(){
      var s = document.getElementById('spSub');
      s.style.animation = 'zoomReveal 0.8s cubic-bezier(0.34,1.2,0.64,1) forwards';
    }, 500);

    /* team card */
    setTimeout(function(){
      var card = document.getElementById('spCard');
      card.style.animation = 'zoomReveal 0.9s cubic-bezier(0.34,1.1,0.64,1) forwards';

      var mc = document.getElementById('spMembers');
      members.forEach(function(m, i){
        var el = document.createElement('div');
        el.className = 'sp-member';
        el.textContent = m;
        mc.appendChild(el);
        setTimeout(function(){
          el.style.animation = 'zoomReveal 0.55s cubic-bezier(0.34,1.56,0.64,1) forwards';
        }, 120 + i * 120);
      });

      setTimeout(function(){
        document.getElementById('spDiv').classList.add('show');
      }, 120 + members.length * 120 + 80);
    }, 720);

    /* progress bar */
    setTimeout(function(){
      var pw  = document.getElementById('spProgWrap');
      var pb  = document.getElementById('spProgBar');
      var lbl = document.getElementById('spLaunch');
      pw.classList.add('show');
      lbl.classList.add('show');

      var pct = 0;
      var stages = [
        'Connecting to sensors...',
        'Loading dashboard...',
        'Syncing Google Sheets...',
        'Ready to launch!'
      ];
      var si = 0;
      lbl.textContent = stages[0];

      var iv = setInterval(function(){
        pct += 1.0;
        pb.style.width = Math.min(pct, 100) + '%';
        var nsi = Math.floor(pct / 25);
        if(nsi !== si && nsi < stages.length){ si = nsi; lbl.textContent = stages[si]; }
        if(pct >= 100){
          clearInterval(iv);
          lbl.textContent = 'Launching GreenFlow...';
          setTimeout(function(){
            splash.classList.add('exit');
            setTimeout(function(){ splash.style.display='none'; }, 1500);
          }, 500);
        }
      }, 55);
    }, 980);
  }
})();

// ════════════════════════════════════════════
//  DASHBOARD
// ════════════════════════════════════════════
var MAX_PTS=20, labels=[], tData=[], hData=[];
var chart = new Chart(document.getElementById('liveChart').getContext('2d'),{
  type:'line',
  data:{labels:labels,datasets:[
    {label:'Temp',data:tData,borderColor:'#00b4d8',backgroundColor:'rgba(0,180,216,.07)',
      borderWidth:2,pointRadius:2,tension:.4,fill:true},
    {label:'Humid',data:hData,borderColor:'#00c896',backgroundColor:'rgba(0,200,150,.07)',
      borderWidth:2,pointRadius:2,tension:.4,fill:true}
  ]},
  options:{responsive:true,maintainAspectRatio:false,animation:{duration:300},
    plugins:{legend:{display:false},
      tooltip:{backgroundColor:'#fff',titleColor:'#0d2137',bodyColor:'#5f8199',
        borderColor:'#e0eef5',borderWidth:1,padding:9,cornerRadius:7}},
    scales:{
      x:{ticks:{color:'#9ab5c5',font:{size:9,family:'DM Mono'},maxRotation:0,maxTicksLimit:7},
        grid:{color:'#f0f7fb'},border:{display:false}},
      y:{ticks:{color:'#9ab5c5',font:{size:9,family:'DM Mono'}},
        grid:{color:'#f0f7fb'},border:{display:false},min:0,max:100}
    }
  }
});

var evHist=[],lastThr=false,curLat=9.575062,curLon=77.675734;
var etTimer=null,stTimer=null,fetchErr=0;
document.getElementById('deviceIP').textContent=window.location.hostname||'--';

var statTick=0;
function poll(){
  fetch('/data',{cache:'no-store'})
    .then(function(r){if(!r.ok)throw 0;return r.json();})
    .then(function(d){fetchErr=0;setConn(true);onData(d);})
    .catch(function(){if(++fetchErr>=5)setConn(false);});
  if(++statTick>=3){
    statTick=0;
    fetch('/stats',{cache:'no-store'})
      .then(function(r){return r.json();})
      .then(function(s){
        document.getElementById('uptime').textContent=s.uptime||'--';
        document.getElementById('threatCount').textContent=s.threats!==undefined?s.threats:'--';
        document.getElementById('maxTemp').textContent=s.maxTemp!==undefined?s.maxTemp.toFixed(1)+'°C':'--';
        document.getElementById('minTemp').textContent=s.minTemp!==undefined?s.minTemp.toFixed(1)+'°C':'--';
        document.getElementById('avgTemp').textContent=s.avgTemp!==undefined?s.avgTemp.toFixed(1)+'°C':'--';
        document.getElementById('heapFree').textContent=s.heap!==undefined?(s.heap/1024).toFixed(1)+' KB':'--';
        if(s.sheetsCount!==undefined) document.getElementById('sheetsCount').textContent=s.sheetsCount;
      }).catch(function(){});
  }
}
function onData(d){
  var sEl=document.getElementById('statusValue'),sCard=document.getElementById('statusCard'),sSub=document.getElementById('statusSub');
  sEl.textContent=d.status||'Unknown';
  if(d.threat){
    sEl.style.color='var(--rd)';sCard.classList.add('ta');
    sSub.className='ss dn';sSub.innerHTML='&#9888; Threat active';
  }else{
    sEl.style.color='var(--tp)';sCard.classList.remove('ta');
    sSub.className='ss up';sSub.innerHTML='&#10003; Normal';
  }
  var t=parseFloat(d.temp),h=parseFloat(d.humid);
  document.getElementById('tempValue').textContent=isNaN(t)?'--':t.toFixed(1);
  document.getElementById('humidValue').textContent=isNaN(h)?'--':h.toFixed(1);
  var ts=document.getElementById('tempSub');
  if(!isNaN(t)){
    if(t>40){ts.className='ss dn';ts.textContent='High!';}
    else if(t<10){ts.className='ss';ts.textContent='Low';}
    else{ts.className='ss up';ts.textContent='Optimal';}
  }
  if(d.lat)curLat=d.lat; if(d.lon)curLon=d.lon;
  if(d.emailSent)   showET(d.turbines);
  if(d.sheetsLogged) showST();
  if(d.threat&&!lastThr){ showPop(d.status,d.turbines,t,h); addLog(d.status,d.turbines||'--',t,h,true); }
  else if(!d.threat&&lastThr){ addLog('Normal','--',t,h,false); }
  lastThr=d.threat;
  if(!isNaN(t)&&!isNaN(h)){
    var lbl=new Date().toLocaleTimeString('en-US',{hour12:false,hour:'2-digit',minute:'2-digit',second:'2-digit'});
    labels.push(lbl);tData.push(t);hData.push(h);
    if(labels.length>MAX_PTS){labels.shift();tData.shift();hData.shift();}
    chart.update('active');
  }
}
function setConn(ok){
  var b=document.getElementById('connBar'),tx=document.getElementById('connText');
  b.className='cb '+(ok?'ok':'err');
  tx.textContent=ok?'Connected – live data':'Connection lost – retrying…';
}
function showPop(st,turb,t,h){
  document.getElementById('popupStatus').textContent=st||'--';
  document.getElementById('popupTurbines').textContent=turb||'--';
  document.getElementById('popupTemp').textContent=isNaN(t)?'--':t.toFixed(1)+'°C';
  document.getElementById('popupHumid').textContent=isNaN(h)?'--':h.toFixed(1)+'%';
  document.getElementById('popupLoc').textContent=curLat.toFixed(6)+', '+curLon.toFixed(6);
  document.getElementById('popupTime').textContent=new Date().toLocaleString();
  document.getElementById('threatOverlay').style.display='block';
}
function acknowledgeAlert(){document.getElementById('threatOverlay').style.display='none';}
function openMaps(){window.open('https://maps.google.com/?q='+curLat+','+curLon,'_blank');}
function showET(turb){
  document.getElementById('emailMsg').textContent='Alert sent for Turbine(s): '+(turb||'--');
  var t=document.getElementById('emailToast');
  t.classList.add('show');if(etTimer)clearTimeout(etTimer);
  etTimer=setTimeout(function(){t.classList.remove('show');},6000);
}
function closeET(){document.getElementById('emailToast').classList.remove('show');}
function showST(){
  var t=document.getElementById('sheetsToast');
  document.getElementById('sheetsSub').textContent='Last: '+new Date().toLocaleTimeString();
  t.classList.add('show');if(stTimer)clearTimeout(stTimer);
  stTimer=setTimeout(function(){t.classList.remove('show');},3000);
}
function addLog(st,turb,t,h,isThr){
  evHist.unshift({time:new Date().toLocaleString(),status:st,turbines:turb,
    temp:isNaN(t)?'--':t.toFixed(1)+'°C',humid:isNaN(h)?'--':h.toFixed(1)+'%',isThr:isThr});
  if(evHist.length>50)evHist.pop();
  renderLog();
}
function renderLog(){
  var tb=document.getElementById('logBody');
  if(!evHist.length){tb.innerHTML='<tr class="nd"><td colspan="6">No events yet</td></tr>';return;}
  tb.innerHTML=evHist.map(function(x){
    return '<tr><td>'+x.time+'</td><td>'+x.status+'</td><td>'+x.turbines+'</td>'
      +'<td>'+x.temp+'</td><td>'+x.humid+'</td>'
      +'<td><span class="pill '+(x.isThr?'th':'ok')+'">'+(x.isThr?'⚠ Threat':'✓ Normal')+'</span></td></tr>';
  }).join('');
}
function clearLog(){evHist=[];renderLog();}
setInterval(poll,2000);
poll();
</script>
</body></html>
)RAW";

// ============================================================
//   SMTP CALLBACK
// ============================================================
void smtpCallback(SMTP_Status status) {
    if (status.success()) Serial.println(F("[EMAIL] Sent OK."));
    else Serial.println("[EMAIL] Error: " + String(status.info()));
}

// ============================================================
//   UPTIME
// ============================================================
String getUptimeString() {
    unsigned long s = (millis() - startTime) / 1000UL;
    return String(s / 3600) + "h " + String((s % 3600) / 60) + "m " + String(s % 60) + "s";
}

// ============================================================
//   LOG TO GOOGLE SHEETS
// ============================================================
void logToGoogleSheets() {
    if (!sheetsUrlOk) return;
    if (WiFi.status() != WL_CONNECTED) return;

    uint32_t maxBlk = ESP.getMaxFreeBlockSize();
    Serial.printf("[SHEETS] max-block %u bytes\n", maxBlk);

    if (maxBlk < 12000) {
        Serial.printf("[SHEETS] Heap too low (%u) — skip\n", maxBlk);
        return;
    }

    String url  = String(SHEETS_URL);
    int hs      = url.indexOf("://") + 3;
    int ps      = url.indexOf('/', hs);
    String host = url.substring(hs, ps);
    String path = url.substring(ps);

    char qs[320];
    snprintf(qs, sizeof(qs),
        "?temp=%.1f&humid=%.1f&status=%s&threat=%s"
        "&turbines=%s&emailSent=%s"
        "&uptime=%s&lat=%.6f&lon=%.6f",
        temperature, humidity,
        statusMessage.c_str(),
        threatDetected ? "true" : "false",
        threatTurbines.length() > 0 ? threatTurbines.c_str() : "--",
        emailSent ? "true" : "false",
        getUptimeString().c_str(),
        DEFAULT_LAT, DEFAULT_LON
    );
    // replace spaces with + for URL encoding
    for (int i = 0; qs[i]; i++) if (qs[i] == ' ') qs[i] = '+';

    Serial.printf("[SHEETS] Logging — threat=%s turbines=%s\n",
        threatDetected ? "YES" : "no",
        threatTurbines.length() > 0 ? threatTurbines.c_str() : "--");

    {
        BearSSL::WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(15000);

        if (!client.connect(host.c_str(), 443)) {
            Serial.println(F("[SHEETS] Connection failed"));
            return;
        }

        client.print(F("GET "));
        client.print(path);
        client.print(qs);
        client.println(F(" HTTP/1.0"));
        client.print(F("Host: ")); client.println(host);
        client.println(F("User-Agent: ESP8266"));
        client.println(F("Connection: close"));
        client.println();

        unsigned long t0 = millis();
        while (!client.available() && (millis() - t0) < 15000) { delay(5); yield(); }

        if (!client.available()) {
            Serial.println(F("[SHEETS] Timeout"));
            client.stop(); return;
        }

        String sl = client.readStringUntil('\n');
        sl.trim();
        client.stop();
        Serial.println("[SHEETS] " + sl);

        if (sl.indexOf("200") >= 0) {
            sheetsLogged = true;
            sheetsLogCount++;
            Serial.printf("[SHEETS] Row %d OK\n", sheetsLogCount);
        } else {
            Serial.println(F("[SHEETS] Unexpected response"));
        }
    }
    yield();
}

// ============================================================
//   SEND IP EMAIL
// ============================================================
void sendIPAddressEmail() {
    smtp.closeSession(); smtp.debug(0);
    Session_Config cfg;
    cfg.server.host_name = SMTP_HOST; cfg.server.port = SMTP_PORT;
    cfg.login.email = AUTHOR_EMAIL; cfg.login.password = AUTHOR_PASSWORD;
    cfg.login.user_domain = F(""); cfg.secure.startTLS = false;
    smtp.callback(smtpCallback);

    SMTP_Message msg;
    msg.sender.name = F("VAWT System"); msg.sender.email = AUTHOR_EMAIL;
    msg.subject = F("VAWT System Started");
    msg.addRecipient(F("Admin"), RECIPIENT_EMAIL);

    String body = "VAWT System Online\n\nIP: " + WiFi.localIP().toString()
                + "\nDashboard: http://" + WiFi.localIP().toString() + "/\n"
                + "Location: " + String(DEFAULT_LAT,6) + ", " + String(DEFAULT_LON,6) + "\n";
    msg.text.content = body.c_str();

    if (!smtp.connect(&cfg)) { smtp.closeSession(); return; }
    MailClient.sendMail(&smtp, &msg);
    smtp.closeSession();
}

// ============================================================
//   SEND THREAT EMAIL
// ============================================================
void sendEmailNotification(const String& st, const String& turb) {
    smtp.closeSession(); smtp.debug(0);
    Session_Config cfg;
    cfg.server.host_name = SMTP_HOST; cfg.server.port = SMTP_PORT;
    cfg.login.email = AUTHOR_EMAIL; cfg.login.password = AUTHOR_PASSWORD;
    cfg.login.user_domain = F(""); cfg.secure.startTLS = false;
    smtp.callback(smtpCallback);

    SMTP_Message msg;
    msg.sender.name = F("VAWT Alert"); msg.sender.email = AUTHOR_EMAIL;
    msg.subject = "ALERT: Threat at T" + turb;
    msg.addRecipient(F("Admin"), RECIPIENT_EMAIL);

    String body = "VAWT THREAT ALERT\n========================\n\n";
    body += "Status   : " + st + "\nTurbine  : " + turb + "\n";
    body += "Temp     : " + String(temperature,1) + " C\n";
    body += "Humidity : " + String(humidity,1) + " %\n";
    body += "Uptime   : " + getUptimeString() + "\n\n";

    bool s[3] = {
        digitalRead(TOUCH1) == HIGH,
        digitalRead(TOUCH2) == HIGH,
        digitalRead(TOUCH3) == HIGH
    };
    for (int i = 0; i < NUM_TURBINES; i++) {
        if (s[i]) {
            body += "Turbine " + String(i+1) + ": https://maps.google.com/?q=";
            body += String(TURBINE_LAT[i],6) + "," + String(TURBINE_LON[i],6) + "\n";
        }
    }
    body += "\nDashboard: http://" + WiFi.localIP().toString() + "/\n";
    msg.text.content = body.c_str();

    if (!smtp.connect(&cfg)) { smtp.closeSession(); return; }
    if (!MailClient.sendMail(&smtp, &msg)) Serial.println(F("[EMAIL] Send failed"));
    else { Serial.println(F("[EMAIL] Alert sent.")); emailSent = true; }
    smtp.closeSession();
}

// ============================================================
//   WEB HANDLERS
// ============================================================
void handleRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, F("text/html"), "");
    server.sendContent_P(HTML_PAGE);
    server.sendContent("");
}

void handleData() {
    char buf[220];
    snprintf(buf, sizeof(buf),
        "{\"status\":\"%s\",\"temp\":%.1f,\"humid\":%.1f,"
        "\"threat\":%s,\"turbines\":\"%s\","
        "\"emailSent\":%s,\"sheetsLogged\":%s,"
        "\"lat\":%.6f,\"lon\":%.6f}",
        statusMessage.c_str(), temperature, humidity,
        threatDetected ? "true" : "false",
        threatTurbines.c_str(),
        emailSent     ? "true" : "false",
        sheetsLogged  ? "true" : "false",
        DEFAULT_LAT, DEFAULT_LON
    );
    server.sendHeader(F("Cache-Control"), F("no-cache,no-store"));
    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.send(200, F("application/json"), buf);
    // clear one-shot flags after they have been read by the browser
    if (emailSent)    emailSent    = false;
    if (sheetsLogged) sheetsLogged = false;
}

void handleStats() {
    float safeMax = (tempReadings > 0) ? maxTemp : 0.0f;
    float safeMin = (tempReadings > 0) ? minTemp : 0.0f;
    float safeAvg = (tempReadings > 0) ? avgTemp : 0.0f;
    char buf[220];
    snprintf(buf, sizeof(buf),
        "{\"uptime\":\"%s\",\"threats\":%d,"
        "\"maxTemp\":%.1f,\"minTemp\":%.1f,\"avgTemp\":%.1f,"
        "\"sheetsCount\":%d,\"heap\":%u}",
        getUptimeString().c_str(), threatCount,
        safeMax, safeMin, safeAvg,
        sheetsLogCount, (uint32_t)ESP.getFreeHeap()
    );
    server.sendHeader(F("Cache-Control"), F("no-cache,no-store"));
    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.send(200, F("application/json"), buf);
}

// ============================================================
//   SENSORS + THREAT DETECTION
// ============================================================
void readSensorsAndCheckThreats() {
    float nh = NAN, nt = NAN;
    for (int attempt = 0; attempt < 3 && (isnan(nh) || isnan(nt)); attempt++) {
        if (attempt > 0) delay(250);
        nh = dht.readHumidity();
        nt = dht.readTemperature();
    }
    if (isnan(nh) || isnan(nt)) { Serial.println(F("[DHT] read failed")); return; }

    humidity    = nh;
    temperature = nt;
    if (tempReadings == 0) { maxTemp = nt; minTemp = nt; avgTemp = nt; }
    else {
        if (nt > maxTemp) maxTemp = nt;
        if (nt < minTemp) minTemp = nt;
        avgTemp = ((avgTemp * (float)tempReadings) + nt) / (float)(tempReadings + 1);
    }
    tempReadings++;

    bool t1 = (digitalRead(TOUCH1) == HIGH);
    bool t2 = (digitalRead(TOUCH2) == HIGH);
    bool t3 = (digitalRead(TOUCH3) == HIGH);
    bool prev = threatDetected;

    if (!t1 && !t2 && !t3) {
        statusMessage  = F("System Normal");
        threatDetected = false;
        threatTurbines = "";
    } else {
        threatDetected = true;
        threatTurbines = "";
        if (t1) threatTurbines += "1 ";
        if (t2) threatTurbines += "2 ";
        if (t3) threatTurbines += "3 ";
        threatTurbines.trim();
        statusMessage = "Threat at T" + threatTurbines;

        // Rising edge — new threat just started
        if (!prev) {
            threatCount++;
            Serial.println(F("[THREAT] New threat detected!"));
            // Check email cooldown before setting the pending flag
            unsigned long now = millis();
            if ((now - lastEmailTime) >= EMAIL_COOLDOWN) {
                lastEmailTime = now;   // reserve the slot now
                pendingThreatAction = true;
                Serial.println(F("[THREAT] Will log to Sheets then send email."));
            } else {
                // Cooldown active: still log to Sheets, but skip email
                pendingThreatAction = true;
                Serial.println(F("[THREAT] Email cooldown active — will log only."));
            }
        }
    }
}

// ============================================================
//   LCD
// ============================================================
void updateLCD() {
    lcd.clear();
    String r0 = statusMessage;
    if (r0.length() > 16) r0 = r0.substring(0, 16);
    lcd.setCursor(0, 0); lcd.print(r0);
    String r1 = "T:" + String(temperature, 1) + "C H:" + String(humidity, 0) + "%";
    if (r1.length() > 16) r1 = r1.substring(0, 16);
    lcd.setCursor(0, 1); lcd.print(r1);
}

// ============================================================
//   SETUP
// ============================================================
void setup() {
    Serial.begin(115200); delay(300);
    Serial.println(F("\n[BOOT] VAWT GreenFlow — Corrected threat+splash build"));

    sheetsUrlOk = (String(SHEETS_URL).indexOf("YOUR_DEPLOYMENT_ID") == -1);

    Wire.begin(D2, D1);
    lcd.init(); lcd.backlight();
    lcd.setCursor(0, 0); lcd.print(F(" ---- VAWT ---- "));
    lcd.setCursor(0, 1); lcd.print(F("connecting......"));

    dht.begin(); delay(2000);

    pinMode(TOUCH1, INPUT);
    pinMode(TOUCH2, INPUT);
    pinMode(TOUCH3, INPUT);

    WiFi.begin(ssid, password);
    Serial.print(F("[WIFI] Connecting"));
    int att = 0;
    while (WiFi.status() != WL_CONNECTED && att < 40) { delay(500); Serial.print("."); att++; }

    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        Serial.println("\n[WIFI] IP: " + ip);

        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        Serial.print(F("[NTP] Syncing"));
        time_t now = time(nullptr);
        while (now < 8 * 3600 * 2) { delay(500); Serial.print("."); now = time(nullptr); }
        Serial.println(F(" OK"));

        lcd.clear();
        lcd.setCursor(0, 0); lcd.print(F("-WIFI- Connected"));
        lcd.setCursor(0, 1); lcd.print("IP: "); lcd.print(ip);

        // Set lastEmailTime so first email is always allowed immediately
        lastEmailTime     = millis() - EMAIL_COOLDOWN;
        lastSheetsLogTime = millis();

        sendIPAddressEmail();
        delay(500);

        server.on("/",      handleRoot);
        server.on("/data",  handleData);
        server.on("/stats", handleStats);
        server.begin();

        lcd.clear();
        lcd.setCursor(0, 0); lcd.print(F("Ready"));
        lcd.setCursor(0, 1); lcd.print(ip);
    } else {
        lcd.clear(); lcd.setCursor(0, 0); lcd.print(F("WiFi FAILED"));
    }
    startTime = millis();
}

// ============================================================
//   LOOP
// ============================================================
void loop() {
    server.handleClient();
    yield();

    unsigned long now = millis();

    // ── Read sensors every second
    if (now - lastSensorReadTime >= SENSOR_READ_INTERVAL) {
        lastSensorReadTime = now;
        readSensorsAndCheckThreats();
    }

    // ── Update LCD every 2 seconds
    if (now - lastLCDUpdateTime >= LCD_UPDATE_INTERVAL) {
        lastLCDUpdateTime = now;
        updateLCD();
    }

    // ── THREAT ACTION: log to Sheets FIRST, then send email
    //    This runs before the periodic Sheets log so it always executes.
    if (pendingThreatAction) {
        pendingThreatAction = false;

        Serial.println(F("[THREAT-ACTION] Step 1: Logging threat to Google Sheets..."));
        logToGoogleSheets();                 // always log
        lastSheetsLogTime = millis();        // reset periodic timer

        // Only send email if this threat was within the cooldown window
        // (lastEmailTime was set at rising edge in readSensorsAndCheckThreats)
        unsigned long elapsed = millis() - lastEmailTime;
        if (elapsed < 5000UL) {             // within 5 s of when we reserved the slot
            Serial.println(F("[THREAT-ACTION] Step 2: Sending email alert..."));
            sendEmailNotification(statusMessage, threatTurbines);
            // emailSent flag is set inside sendEmailNotification on success
        } else {
            Serial.println(F("[THREAT-ACTION] Step 2: Email skipped (cooldown)."));
        }
        return;   // skip periodic log this iteration
    }

    // ── Periodic Sheets log every 15 seconds
    if (now - lastSheetsLogTime >= SHEETS_LOG_INTERVAL) {
        lastSheetsLogTime = now;
        logToGoogleSheets();
    }
}

// ================================================================
//  Security/Portal.h  —  Portal v2 Web Dashboard
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  Read-only foundation layer. Serves a mobile-first SPA with:
//    Overview, Devices, Threats, Settings
//
//  API: GET /api/status, /api/devices, /api/threats, /api/settings
//       POST /api/settings (JSON body: device_name, ap_ssid, ap_pass, threat_sensitivity, homeguard_enabled, aviation_enabled)
// ================================================================
#pragma once

#include <math.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "AeroSnifferOS.h"
#include "Sniffer.h"
#include "ThreatEngine.h"

// ── Extern globals (from AeroSniffer.ino) ───────────────────────
extern volatile bool g_touch_tap;

// ── Static globals ──────────────────────────────────────────────
static TFT_eSprite* _stft = nullptr;
static WebServer*   _webserver = nullptr;

// Radar sweep animation
static float    _sweep_angle   = 0.0f;
static uint32_t _sweep_last_ms = 0;

// PPS calculation
static uint32_t _pps_last_calc = 0;
static uint32_t _threat_count  = 0;
static uint32_t _prev_total    = 0;

// ── LCD Display ─────────────────────────────────────────────────
static void portal_draw_display() {
    _stft->fillSprite(TFT_BLACK);

    if (sec_hud_mode) {
        uint32_t now = millis();
        uint32_t pps   = pkt_per_sec;
        uint32_t total = pkt_total;
        uint32_t deauths = pkt_deauth;

        // Header bar
        _stft->fillRect(0, 0, 240, 14, 0x0010);
        _stft->setTextColor(0x07FF);
        _stft->setTextSize(1);
        _stft->setCursor(4, 3);
        _stft->print("PORTAL v2");

        const char* status = (pkt_beacon > 0) ? "SCAN" : "IDLE";
        _stft->setCursor(170, 3);
        _stft->setTextColor((pps > 0) ? 0x07E0 : 0x5AEB);
        _stft->print(status);

        _stft->drawFastHLine(0, 15, 240, 0x1c2b22);

        // Radar sweep
        int cx = 120, cy = 75, r = 55;
        _stft->drawCircle(cx, cy, r, 0x1c2b22);
        _stft->drawCircle(cx, cy, r * 2 / 3, 0x1c2b22);
        _stft->drawCircle(cx, cy, r * 1 / 3, 0x1c2b22);
        _stft->drawLine(cx - r, cy, cx + r, cy, 0x1c2b22);
        _stft->drawLine(cx, cy - r, cx, cy + r, 0x1c2b22);

        if (_sweep_last_ms == 0) _sweep_last_ms = now;
        uint32_t dt = now - _sweep_last_ms;
        _sweep_last_ms = now;
        _sweep_angle += dt * 0.06f;
        if (_sweep_angle >= 360.0f) _sweep_angle -= 360.0f;

        float rad = _sweep_angle * 3.14159f / 180.0f;
        int ex = cx + (int)(cosf(rad) * r);
        int ey = cy + (int)(sinf(rad) * r);
        _stft->drawLine(cx, cy, ex, ey, 0x07FF);
        _stft->fillCircle(ex, ey, 3, 0x07FF);

        // PPS
        _stft->setTextColor(0xFFFF);
        _stft->setCursor(10, 140);
        _stft->print("PPS: ");
        _stft->setTextColor((pps > 0) ? 0x07E0 : 0x5AEB);
        _stft->print(pps);

        // Total packets
        _stft->setTextColor(0xFFFF);
        _stft->setCursor(100, 140);
        _stft->print("Total: ");
        _stft->setTextColor(0x07E0);
        _stft->print(total);

        // Deauths / threats
        _stft->setTextColor(0xFFFF);
        _stft->setCursor(10, 158);
        _stft->print("Deauth: ");
        _stft->setTextColor((deauths > 0) ? 0xF05050 : 0x5AEB);
        _stft->print(deauths);
        if (_threat_count > 0) {
            _stft->fillCircle(110, 154, 4, 0xF05050);
        }

        // SSID
        _stft->setTextColor(0xFFFF);
        _stft->setCursor(10, 174);
        _stft->print("SSID: ");
        _stft->setTextColor(0x07FF);
        _stft->print(sys_ap_ssid.c_str());

        // IP + clients
        _stft->setTextColor(0xFFFF);
        _stft->setCursor(140, 174);
        _stft->print("Clients: ");
        _stft->setTextColor(0xFFE0);
        _stft->print(WiFi.softAPgetStationNum());

        // Footer
        _stft->fillRect(0, 226, 240, 14, 0x0008);
        _stft->setTextColor(0x2CA0);
        _stft->setCursor(4, 228);
        _stft->print("http://192.168.4.1");

        _stft->setCursor(140, 228);
        _stft->print("h:");
        _stft->print(ESP.getFreeHeap() / 1024);
        _stft->print("k");
    } else {
        // Character Face Mode
        static int frame = 0;
        frame++;

        // Draw header bar
        _stft->fillRect(0, 0, 240, 40, 0x0010);
        _stft->setTextColor(0x39FF88); // Neon Green
        _stft->setTextSize(1);
        _stft->setCursor(10, 8);
        _stft->print("SECURITY SENTINEL");

        _stft->setCursor(10, 24);
        if (_threat_count > 0) {
            _stft->setTextColor(TFT_RED);
            _stft->printf("ALERT: %d THREATS DETECTED", _threat_count);
        } else {
            _stft->setTextColor(TFT_GREEN);
            _stft->print("SYSTEM: MONITORING ACTIVE");
        }

        _stft->setTextColor(0x2CA0);
        _stft->setCursor(180, 15);
        _stft->printf("CH: %d", current_ch);
        _stft->drawFastHLine(0, 39, 240, 0x1c2b22);

        // Status lines
        char l1[48];
        char l2[48];
        snprintf(l1, sizeof(l1), "Threats: %d | PPS: %d", _threat_count, pkt_per_sec);
        snprintf(l2, sizeof(l2), "http://192.168.4.1");
        FaceEngine.setStatusLines(l1, l2);

        FaceEngine.render(_stft, frame);
    }
}

// ── SPA HTML ─────────────────────────────────────────────────────
static const char portal_html[] PROGMEM = R"HTMLTEXT(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>AeroSniffer // SEC</title>
<style>
:root{
  --bg:#070a08;
  --panel:#0e1611;
  --line:#1c2b22;
  --green:#39ff88;
  --green-dim:#1e8a52;
  --amber:#ffb000;
  --purple:#c084fc;
  --red:#f05050;
  --text:#d7e8da;
  --muted:#5c7468;
  --font:'JetBrains Mono','IBM Plex Mono',ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
}
*{margin:0;padding:0;box-sizing:border-box}
html,body{height:100%;overflow:hidden;background:var(--bg);color:var(--text);font-family:var(--font);font-size:12px;display:flex;flex-direction:column}

/* Status bar */
.status-bar{height:40px;min-height:40px;display:flex;align-items:center;padding:0 10px;border-bottom:1px solid var(--line);gap:6px;font-size:11px}
.logo{color:var(--purple);font-weight:bold}
.status-dot{font-size:14px;margin-left:auto}.status-dot.on{color:var(--green)}.status-dot.off{color:var(--muted)}
.pps-val{color:var(--amber);font-weight:bold}.pps-label{color:var(--muted);font-size:10px}

/* Content */
.content{flex:1;overflow-y:auto;padding:8px 10px;-webkit-overflow-scrolling:touch}

/* Tab bar */
.tab-bar{height:48px;min-height:48px;display:flex;border-top:1px solid var(--line);background:var(--bg)}
.tab-btn{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;cursor:pointer;color:var(--muted);font-size:9px;gap:1px;border:none;background:none;padding:4px 0;-webkit-tap-highlight-color:transparent;touch-action:manipulation;user-select:none}
.tab-btn .tab-icon{font-size:16px;line-height:1}
.tab-btn.active{color:var(--green)}.tab-btn.active .tab-indicator{width:20px;height:2px;background:var(--green);border-radius:1px;margin-top:2px}
.tab-indicator{width:0;height:2px;transition:width .15s}

/* Sections */
.section{display:none}.section.active{display:block}

/* Cards */
.card{background:var(--panel);border:1px solid var(--line);border-radius:6px;padding:10px 12px;margin-bottom:8px}
.card-title{color:var(--purple);font-size:10px;text-transform:uppercase;letter-spacing:1px;margin-bottom:6px}
.row{display:flex;justify-content:space-between;padding:3px 0;border-bottom:1px solid rgba(28,43,34,.5)}
.row:last-child{border-bottom:none}
.row .label{color:var(--muted)}.row .value{color:var(--text);text-align:right}
.highlight{color:var(--green)}.warn{color:var(--amber)}.danger{color:var(--red)}

/* Stat grid */
.stat-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.stat-cell{background:rgba(28,43,34,.3);border-radius:4px;padding:8px;text-align:center}
.stat-cell .num{font-size:18px;font-weight:bold;color:var(--green);display:block}
.stat-cell .lbl{font-size:9px;color:var(--muted)}

/* Tables */
table{width:100%;border-collapse:collapse;font-size:10px}
th{text-align:left;color:var(--muted);padding:4px 2px;border-bottom:1px solid var(--line);font-weight:normal}
td{padding:4px 2px;border-bottom:1px solid rgba(28,43,34,.3);vertical-align:middle}
tr:nth-child(even) td{background:rgba(0,0,0,.15)}

/* RSSI bar */
.rssi-bar{display:inline-flex;gap:2px;align-items:center}
.rssi-bar span{display:block;width:4px;border-radius:1px;height:8px;background:var(--line)}
.rssi-bar span.on{background:var(--green)}.rssi-bar span.mid{background:var(--amber)}.rssi-bar span.low{background:var(--red)}

/* Severity dot */
.sev-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px;flex-shrink:0}
.sev-dot.info{background:var(--green)}.sev-dot.warn{background:var(--amber)}.sev-dot.crit{background:var(--red)}

/* Type badge */
.badge{display:inline-block;font-size:9px;padding:1px 5px;border-radius:3px;border:1px solid var(--line);color:var(--muted)}
.badge.deauth{border-color:var(--red);color:var(--red)}
.badge.eviltwin{border-color:var(--amber);color:var(--amber)}
.badge.intruder{border-color:var(--purple);color:var(--purple)}

/* Severity badge (filled background) */
.badge-sev{display:inline-block;font-size:9px;padding:1px 6px;border-radius:3px;font-weight:bold;line-height:1.4}
.badge-sev.crit{background:rgba(240,80,80,.2);color:var(--red);border:1px solid var(--red)}
.badge-sev.warn{background:rgba(255,176,0,.15);color:var(--amber);border:1px solid var(--amber)}
.badge-sev.info{background:rgba(30,138,82,.15);color:var(--green);border:1px solid var(--green-dim)}

/* Timeline */
.timeline{position:relative;padding-left:22px}
.timeline::before{content:'';position:absolute;left:10px;top:8px;bottom:8px;width:2px;background:var(--line);border-radius:1px}
.tl-dot{position:absolute;left:6px;top:12px;width:10px;height:10px;border-radius:50%;z-index:1;border:2px solid var(--bg)}
.tl-dot.crit{background:var(--red);box-shadow:0 0 6px var(--red)}
.tl-dot.warn{background:var(--amber)}
.tl-dot.info{background:var(--green-dim)}

/* Alerts */
.alert-row{position:relative;padding:8px 8px 8px 0;border-bottom:1px solid rgba(28,43,34,.3)}
.alert-row:last-child{border-bottom:none}
.alert-ts{color:var(--muted);font-size:9px;white-space:nowrap;min-width:52px}
.alert-body{flex:1;min-width:0}.alert-desc{font-size:10px;line-height:1.3}
.alert-src{font-size:9px;color:var(--muted);margin-top:1px}

/* Filter pills */
.pill-group{display:flex;gap:4px;margin-bottom:8px;flex-wrap:wrap;align-items:center}
.pill{font-size:9px;padding:3px 8px;border:1px solid var(--line);border-radius:12px;background:transparent;color:var(--muted);cursor:pointer}
.pill.active{color:var(--green);border-color:var(--green)}
.pill .cnt{color:var(--muted);font-size:8px;margin-left:3px;opacity:.6}
.pill.active .cnt{color:var(--green)}
.pill-clear{font-size:9px;padding:2px 6px;border:1px solid var(--muted);border-radius:12px;background:transparent;color:var(--muted);cursor:pointer;margin-left:2px}
.pill-clear:hover{color:var(--text);border-color:var(--text)}

/* Type breakdown */
.type-row{display:flex;justify-content:space-between;padding:3px 0;border-bottom:1px solid rgba(28,43,34,.3)}
.type-row:last-child{border-bottom:none}
.type-row .lbl{color:var(--muted);font-size:10px}
.type-row .val{font-weight:bold;font-size:11px}

/* Last threat indicator */
.last-threat{font-size:9px;color:var(--muted);text-align:center;margin-top:4px}

/* Buttons */
.btn{font-family:var(--font);font-size:10px;padding:6px 14px;border:1px solid var(--line);border-radius:4px;background:transparent;color:var(--text);cursor:pointer;touch-action:manipulation}
.btn-primary{border-color:var(--green);color:var(--green)}
.btn:disabled{opacity:.4;cursor:default}

/* Form inputs */
input,select{font-family:var(--font);font-size:11px;background:var(--bg);border:1px solid var(--line);border-radius:3px;color:var(--text);padding:5px 8px;width:100%;outline:none}
input:focus{border-color:var(--green)}input:disabled{opacity:.4}
.s-inp{text-align:right;width:auto;min-width:120px;max-width:180px}

/* Toggle switch */
.switch{position:relative;display:inline-block;width:36px;height:20px;flex-shrink:0}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;inset:0;background:var(--line);border-radius:20px;transition:.2s}
.slider::before{content:"";position:absolute;height:14px;width:14px;left:3px;bottom:3px;background:var(--muted);border-radius:50%;transition:.2s}
input:checked+.slider{background:var(--green-dim)}
input:checked+.slider::before{background:var(--green);transform:translateX(16px)}

/* Empty state */
.empty-state{text-align:center;padding:24px 12px;color:var(--muted);font-size:11px}
.empty-state .icon{font-size:24px;display:block;margin-bottom:8px}

/* Misc */
.gap-4{height:4px}.mt-4{margin-top:4px}.mb-8{margin-bottom:8px}.flex{display:flex}.flex-col{flex-direction:column}.gap-6{gap:6px}.items-center{align-items:center}.justify-between{justify-content:space-between}

/* Sub-tabs for devices */
.sub-tabs{display:flex;gap:0;margin-bottom:8px;border-bottom:1px solid var(--line)}
.sub-tab{font-size:10px;padding:4px 12px;cursor:pointer;color:var(--muted);border-bottom:2px solid transparent}
.sub-tab.active{color:var(--green);border-bottom-color:var(--green)}

@media(max-width:360px){
  .stat-grid .num{font-size:15px}
  .card{padding:8px 10px}
  .tab-btn{font-size:8px}
}
</style>
</head>
<body>

<div class="status-bar">
  <span class="logo">AeroSniffer</span>
  <span class="status-dot off" id="dot">●</span>
  <span id="statusLabel">IDLE</span>
  <span class="pps-val" id="ppsVal">0</span>
  <span class="pps-label">pps</span>
</div>

<div class="content" id="content">

<!-- ═══ OVERVIEW ═══ -->
<div class="section active" id="section-overview">
  <div class="card">
    <div class="card-title">Status</div>
    <div class="row"><span class="label">Scan</span><span class="value" id="ov-scan">IDLE</span></div>
    <div class="row"><span class="label">Channel</span><span class="value" id="ov-ch">6</span></div>
    <div class="row"><span class="label">AP Clients</span><span class="value" id="ov-clients">0</span></div>
    <div class="row"><span class="label">Uptime</span><span class="value" id="ov-uptime">0s</span></div>
    <div class="row"><span class="label">Heap</span><span class="value" id="ov-heap">0</span></div>
  </div>

  <div class="stat-grid">
    <div class="stat-cell"><span class="num" id="ov-stats-pps">0</span><span class="lbl">PPS</span></div>
    <div class="stat-cell"><span class="num" id="ov-stats-total">0</span><span class="lbl">Total</span></div>
    <div class="stat-cell"><span class="num" id="ov-stats-beacon">0</span><span class="lbl">Beacon</span></div>
    <div class="stat-cell"><span class="num" id="ov-stats-deauth">0</span><span class="lbl">Deauth</span></div>
  </div>

  <div class="card">
    <div class="card-title">Access Point</div>
    <div class="row"><span class="label">SSID</span><span class="value highlight" id="ov-ap-ssid">--</span></div>
    <div class="row"><span class="label">IP</span><span class="value" id="ov-ap-ip">--</span></div>
  </div>
</div>

<!-- ═══ DEVICES ═══ -->
<div class="section" id="section-devices">
  <div class="sub-tabs">
    <div class="sub-tab active" onclick="switchDeviceTab('aps')">APs</div>
    <div class="sub-tab" onclick="switchDeviceTab('clients')">Clients</div>
  </div>
  <div id="dev-aps">
    <div class="empty-state"><span class="icon">⌥</span>No APs detected yet.<br>Start scanning to discover networks.</div>
  </div>
  <div id="dev-clients" style="display:none">
    <div class="empty-state"><span class="icon">◈</span>No clients detected yet.<br>Client data appears during active scanning.</div>
  </div>
</div>

<!-- ═══ THREATS ═══ -->
<div class="section" id="section-threats">
  <div id="threat-summary"></div>
  <div class="pill-group" id="threat-pills">
    <span class="pill active" data-filter="all" onclick="filterThreats('all')">All<span class="cnt"></span></span>
    <span class="pill" data-filter="deauth" onclick="filterThreats('deauth')">Deauth<span class="cnt"></span></span>
    <span class="pill" data-filter="eviltwin" onclick="filterThreats('eviltwin')">Evil Twin<span class="cnt"></span></span>
    <span class="pill" data-filter="intruder" onclick="filterThreats('intruder')">Intruder<span class="cnt"></span></span>
    <span class="pill-clear" id="threat-clear-btn" onclick="filterThreats('all')" style="display:none">× Clear</span>
  </div>
  <div class="card" id="threat-feed">
    <div class="empty-state"><span class="icon">✓</span>No threats detected.<br>All clear.</div>
  </div>
</div>

<!-- ═══ SETTINGS ═══ -->
<div class="section" id="section-settings">

  <div class="card">
    <div class="card-title">Device</div>
    <div class="row"><span class="label">Device Name</span><input type="text" id="s-dev-name" class="s-inp" placeholder="AeroSniffer"></div>
  </div>

  <div class="card">
    <div class="card-title">Security</div>
    <div class="row"><span class="label">AP SSID</span><input type="text" id="s-ap-ssid" class="s-inp"></div>
    <div class="row"><span class="label">AP Password</span><input type="text" id="s-ap-pass" class="s-inp" placeholder="min 8 chars"></div>
    <div class="row"><span class="label">Threat Sensitivity</span>
      <select id="s-thr-sens" class="s-inp" style="width:auto;min-width:100px">
        <option value="0">Low</option>
        <option value="1">Medium</option>
        <option value="2">High</option>
      </select>
    </div>
  </div>

  <div class="card">
    <div class="card-title">Home Guard</div>
    <div class="row"><span class="label">Enable device tracking &amp; alerts</span><label class="switch"><input type="checkbox" id="s-hg-enable"><span class="slider"></span></label></div>
  </div>

  <div class="card">
    <div class="card-title">Aviation</div>
    <div class="row"><span class="label">Enable aviation threat integration</span><label class="switch"><input type="checkbox" id="s-avi-enable"><span class="slider"></span></label></div>
  </div>

  <div class="card">
    <div class="card-title">System</div>
    <div class="row"><span class="label">Mode</span><span class="value">Security v2</span></div>
    <div class="row"><span class="label">Firmware</span><span class="value">2.0</span></div>
    <div class="row"><span class="label">Uptime</span><span class="value" id="set-uptime">0s</span></div>
    <div class="row"><span class="label">Heap</span><span class="value" id="set-heap">0</span></div>
  </div>

  <button class="btn btn-primary" style="width:100%;margin-top:4px;margin-bottom:8px" onclick="saveSettings()">Save Settings</button>
  <div id="s-msg" style="font-size:10px;color:var(--green);text-align:center;margin-bottom:8px;display:none"></div>
</div>

</div><!-- .content -->

<div class="tab-bar">
  <button class="tab-btn active" data-tab="overview" onclick="navigate('overview')"><span class="tab-icon">⊞</span>Overview<span class="tab-indicator"></span></button>
  <button class="tab-btn" data-tab="devices" onclick="navigate('devices')"><span class="tab-icon">⌥</span>Devices<span class="tab-indicator"></span></button>
  <button class="tab-btn" data-tab="threats" onclick="navigate('threats')"><span class="tab-icon">⚠</span>Threats<span class="tab-indicator"></span></button>
  <button class="tab-btn" data-tab="settings" onclick="navigate('settings')"><span class="tab-icon">⚙</span>Settings<span class="tab-indicator"></span></button>
</div>

<script>
(function(){
'use strict';

var activeTab = 'overview';
var activeThreatFilter = 'all';
var deviceSubTab = 'aps';
var polls = {};

// ── Navigation ──
function navigate(tab) {
  activeTab = tab;
  document.querySelectorAll('.section').forEach(function(s){ s.classList.remove('active'); });
  var el = document.getElementById('section-' + tab);
  if (el) el.classList.add('active');
  document.querySelectorAll('.tab-btn').forEach(function(b){
    b.classList.toggle('active', b.dataset.tab === tab);
  });
  stopAllPolls();
  location.hash = '#' + tab;
}
window.navigate = navigate;

function startPoll(name, fn, ms) {
  stopPoll(name);
  polls[name] = setInterval(fn, ms);
  fn();
}
function stopPoll(name) { if (polls[name]) { clearInterval(polls[name]); delete polls[name]; } }
function stopAllPolls() { for (var k in polls) stopPoll(k); }

// ── Router from hash ──
function routeFromHash() {
  var h = location.hash.replace('#','');
  if (['overview','devices','threats','settings'].indexOf(h) >= 0) navigate(h);
}
window.addEventListener('hashchange', routeFromHash);
if (location.hash) routeFromHash();

// ── Filter / sub-tab helpers ──
function filterThreats(f) {
  activeThreatFilter = f;
  document.querySelectorAll('#threat-pills .pill').forEach(function(p){
    p.classList.toggle('active', p.dataset.filter === f);
  });
  var cb = document.getElementById('threat-clear-btn');
  if (cb) cb.style.display = (f === 'all') ? 'none' : 'inline';
  fetchThreats();
}
window.filterThreats = filterThreats;

function switchDeviceTab(t) {
  deviceSubTab = t;
  document.querySelectorAll('.sub-tab').forEach(function(s){
    s.classList.toggle('active', s.textContent.trim().toLowerCase() === t);
  });
  document.getElementById('dev-aps').style.display = (t === 'aps') ? 'block' : 'none';
  document.getElementById('dev-clients').style.display = (t === 'clients') ? 'block' : 'none';
}
window.switchDeviceTab = switchDeviceTab;
window.saveSettings = saveSettings;
window.toggleTrust = toggleTrust;

// ── Utils ──
function $(id) { return document.getElementById(id); }
function fmtTime(s) {
  var h = Math.floor(s / 3600);
  var m = Math.floor((s % 3600) / 60);
  var sec = s % 60;
  return (h > 0 ? h + 'h ' : '') + (m > 0 ? m + 'm ' : '') + sec + 's';
}
function fmtMac(m) { return m || '--'; }
function fmtAge(ts, now) {
  if (ts == null || ts === undefined) return '--';
  var sec = (now || Math.floor(Date.now()/1000)) - ts;
  if (sec < 0) return 'now';
  if (sec < 5) return 'just now';
  if (sec < 60) return sec + 's ago';
  var m = Math.floor(sec / 60);
  if (m < 60) return m + 'm ago';
  var h = Math.floor(m / 60);
  return h + 'h ago';
}
function empty(msg) { return '<div class="empty-state"><span class="icon">◌</span>' + msg + '</div>'; }

// ── Renderers ──
function renderStatus(d) {
  var scan = d.scanning ? 'SCANNING' : 'IDLE';
  $('ov-scan').textContent = scan;
  $('ov-ch').textContent = d.channel || 6;
  $('ov-clients').textContent = d.ap_stations || 0;
  $('ov-uptime').textContent = fmtTime(d.uptime_sec || 0);
  $('ov-heap').textContent = (d.heap || 0) + ' B';
  $('ov-ap-ssid').textContent = d.ap_ssid || '--';
  $('ov-ap-ip').textContent = d.ap_ip || '--';

  $('ov-stats-pps').textContent = d.pps || 0;
  $('ov-stats-total').textContent = d.total || 0;
  $('ov-stats-beacon').textContent = d.beacons || 0;
  $('ov-stats-deauth').textContent = d.deauths || 0;

  // Status bar
  var dot = $('dot');
  dot.className = 'status-dot ' + (d.scanning ? 'on' : 'off');
  $('statusLabel').textContent = scan;
  $('ppsVal').textContent = d.pps || 0;
}

function renderSettings(d) {
  $('s-dev-name').value = d.device_name || '';
  $('s-ap-ssid').value = d.ap_ssid || '';
  $('s-ap-pass').value = d.ap_pass || '';
  $('s-thr-sens').value = d.threat_sensitivity != null ? String(d.threat_sensitivity) : '1';
  $('s-hg-enable').checked = !!d.homeguard_enabled;
  $('s-avi-enable').checked = !!d.aviation_enabled;
  $('s-msg').style.display = 'none';
}

function saveSettings() {
  var btn = document.querySelector('#section-settings .btn-primary');
  var msg = $('s-msg');
  btn.disabled = true;
  btn.textContent = 'Saving...';
  msg.textContent = 'Saving...';
  msg.style.color = 'var(--muted)';
  msg.style.display = 'block';
  var body = JSON.stringify({
    device_name: $('s-dev-name').value,
    ap_ssid: $('s-ap-ssid').value,
    ap_pass: $('s-ap-pass').value,
    threat_sensitivity: parseInt($('s-thr-sens').value),
    homeguard_enabled: $('s-hg-enable').checked,
    aviation_enabled: $('s-avi-enable').checked
  });
  fetch('/api/settings', {method:'POST', body:body, headers:{'Content-Type':'application/json'}})
    .then(function(r){ return r.json(); })
    .then(function(d){
      btn.disabled = false;
      btn.textContent = 'Save Settings';
      if (d.ok) {
        msg.textContent = 'Settings saved' + (d.reboot_required ? ' — reboot required' : '');
        msg.style.color = 'var(--green)';
      } else {
        msg.textContent = 'Error: ' + (d.error || 'unknown');
        msg.style.color = 'var(--red)';
      }
      msg.style.display = 'block';
      if (d.ok) fetchSettings();
    })
    .catch(function(){
      btn.disabled = false;
      btn.textContent = 'Save Settings';
      msg.textContent = 'Save failed';
      msg.style.color = 'var(--red)';
      msg.style.display = 'block';
    });
}

// ── Fetchers ──
function fetchStatus() {
  fetch('/api/status').then(function(r){ return r.json(); }).then(renderStatus).catch(function(){});
}
function renderAPs(aps) {
  if (!aps || aps.length === 0) return empty('No APs detected yet.\nStart scanning to discover networks.');
  var html = '<div class="card" style="padding:0 12px"><table><tr><th>SSID</th><th>Ch</th><th>RSSI</th><th>Enc</th></tr>';
  aps.forEach(function(a){
    var enc = a.enc ? 'WPA2' : 'OPEN';
    var rssiCls = a.rssi >= -70 ? 'on' : a.rssi >= -85 ? 'mid' : 'low';
    html += '<tr><td>' + (a.ssid || '--') + '</td><td>' + a.ch + '</td>'
         + '<td><div class="rssi-bar"><span class="' + rssiCls + '"></span><span class="' + rssiCls + '"></span><span class="' + rssiCls + '"></span></div></td>'
         + '<td>' + enc + '</td></tr>';
  });
  html += '</table></div>';
  return html;
}
function renderDeviceSummary(clients) {
  if (!clients || clients.length === 0) return '';
  var total = clients.length;
  var trusted = 0, present = 0, newDev = 0, famDev = 0;
  clients.forEach(function(c){
    if (c.trusted) trusted++;
    if (c.present) present++;
    if (c.type === 'new') newDev++;
    if (c.type === 'familiar') famDev++;
  });
  return '<div class="stat-grid" style="margin-bottom:8px">'
    + '<div class="stat-cell"><span class="num">' + total + '</span><span class="lbl">Total</span></div>'
    + '<div class="stat-cell"><span class="num">' + trusted + '</span><span class="lbl">Trusted</span></div>'
    + '<div class="stat-cell"><span class="num">' + present + '</span><span class="lbl">Present</span></div>'
    + '<div class="stat-cell"><span class="num">' + newDev + '</span><span class="lbl">New</span></div>'
    + '</div>';
}

function renderClients(clients, probes, uptime) {
  var html = '';
  if (clients && clients.length > 0) {
    html += renderDeviceSummary(clients);
    html += '<div class="card" style="padding:0 12px"><table style="width:100%"><tr><th>Device</th><th></th></tr>';
    clients.forEach(function(c){
      var dotCls = c.present ? 'on' : 'off';
      var cls = c.type === 'trusted' ? 'TRUSTED' : (c.type === 'familiar' ? 'FAMILIAR' : 'NEW');
      var clsColor = c.type === 'trusted' ? 'var(--green)' : (c.type === 'familiar' ? 'var(--amber)' : 'var(--muted)');
      var dispName = c.name || 'Unknown';
      var btnLabel = c.trusted ? 'UNTRUST' : 'TRUST';
      html += '<tr><td style="padding:6px 0">';
      html += '<div class="flex items-center" style="gap:6px">';
      html += '<span class="status-dot ' + dotCls + '" style="display:inline-block"></span>';
      html += '<span style="font-weight:600;font-size:11px">' + dispName + '</span>';
      html += '</div>';
      html += '<div style="font-size:9px;color:var(--muted);margin-top:1px">' + c.mac + '</div>';
      html += '<div style="font-size:9px;margin-top:2px">';
      html += '<span style="color:' + clsColor + ';font-weight:600">' + cls + '</span>';
      html += '<span style="color:var(--muted)"> · </span>';
      html += '<span>' + (c.present ? 'Present' : 'Away') + '</span>';
      html += '<span style="color:var(--muted)"> · </span>';
      html += '<span>' + c.sightings + 'x</span>';
      html += '<span style="color:var(--muted)"> · </span>';
      html += '<span>Seen: ' + fmtAge(c.lastSeen, uptime) + '</span>';
      html += '<span style="color:var(--muted)"> · </span>';
      html += '<span>First: ' + fmtAge(c.firstSeen, uptime) + '</span>';
      html += '</div>';
      html += '</td><td style="vertical-align:middle;width:60px;text-align:right">';
      html += '<button class="btn btn-primary" style="font-size:9px;padding:2px 8px" onclick="toggleTrust(\'' + c.mac + '\',this)">' + btnLabel + '</button>';
      html += '</td></tr>';
    });
    html += '</table></div>';
  }
  if (probes && probes.length > 0) {
    html += '<div class="card"><div class="card-title">Probe Leaks</div>';
    probes.forEach(function(p){ html += '<div class="row"><span class="label">' + p.mac + '</span><span class="value warn">' + (p.ssid || '?') + '</span></div>'; });
    html += '</div>';
  }
  if (!html) html = empty('No clients detected yet.\nClient data appears during active scanning.');
  return html;
}

function toggleTrust(mac, btn) {
  btn.disabled = true;
  fetch('/api/trust?mac=' + encodeURIComponent(mac)).then(function(r){ return r.json(); }).then(function(d){
    if (d.ok) { fetchDevices(); }
    else { btn.disabled = false; }
  }).catch(function(){ btn.disabled = false; });
}
function fetchDevices() {
  fetch('/api/devices').then(function(r){ return r.json(); }).then(function(d){
    $('dev-aps').innerHTML = renderAPs(d.aps);
    $('dev-clients').innerHTML = renderClients(d.clients, d.probes, d.uptime_sec);
  }).catch(function(){});
}
function renderThreatSummary(threats, uptime) {
  if (!threats || threats.length === 0) return '';
  var total = threats.length;
  var crit = 0, warn = 0, info = 0;
  var typeCounts = {};
  threats.forEach(function(t){
    if (t.severity === 2) crit++;
    else if (t.severity === 1) warn++;
    else info++;
    typeCounts[t.type] = (typeCounts[t.type] || 0) + 1;
  });
  var typeHtml = '';
  var typeLabels = {deauth:'Deauth', eviltwin:'Evil Twin', intruder:'Intruder'};
  var typeColors = {deauth:'var(--red)', eviltwin:'var(--amber)', intruder:'var(--purple)'};
  for (var k in typeCounts) {
    var lbl = typeLabels[k] || k;
    var col = typeColors[k] || 'var(--muted)';
    typeHtml += '<div class="type-row"><span class="lbl" style="color:' + col + '">' + lbl + '</span><span class="val" style="color:' + col + '">' + typeCounts[k] + '</span></div>';
  }
  var lastHtml = '';
  if (threats.length > 0 && threats[0].ts != null) {
    lastHtml = '<div class="last-threat">Latest: ' + fmtAge(threats[0].ts, uptime) + '</div>';
  }
  return '<div class="stat-grid" style="margin-bottom:6px">'
    + '<div class="stat-cell"><span class="num" style="color:var(--red)">' + crit + '</span><span class="lbl">Critical</span></div>'
    + '<div class="stat-cell"><span class="num" style="color:var(--amber)">' + warn + '</span><span class="lbl">Warning</span></div>'
    + '<div class="stat-cell"><span class="num" style="color:var(--muted)">' + info + '</span><span class="lbl">Info</span></div>'
    + '<div class="stat-cell"><span class="num" style="color:var(--green)">' + total + '</span><span class="lbl">Total</span></div>'
    + '</div>'
    + (typeHtml ? '<div class="card" style="padding:6px 12px;margin-bottom:8px"><div class="card-title" style="margin-bottom:2px">By Type</div>' + typeHtml + '</div>' : '')
    + lastHtml;
}

function renderThreats(d) {
  var summary = $('threat-summary');
  var feed = $('threat-feed');
  if (!d.threats || d.threats.length === 0) {
    summary.innerHTML = '';
    feed.innerHTML = '<div class="empty-state"><span class="icon">✓</span>No threats detected.<br>All clear.</div>';
    return;
  }
  summary.innerHTML = renderThreatSummary(d.threats, d.uptime_sec);
  var filtered = activeThreatFilter === 'all' ? d.threats : d.threats.filter(function(t){ return t.type === activeThreatFilter; });
  if (filtered.length === 0) {
    feed.innerHTML = '<div class="empty-state"><span class="icon">◌</span>No ' + activeThreatFilter + ' threats.</div>';
    return;
  }
  var html = '<div class="card" style="padding:0;overflow:hidden"><div class="timeline">';
  filtered.forEach(function(t){
    var sevCls = t.severity === 2 ? 'crit' : t.severity === 1 ? 'warn' : 'info';
    var sevLbl = t.severity === 2 ? 'CRIT' : t.severity === 1 ? 'WARN' : 'INFO';
    var sym = t.severity === 2 ? '◆' : t.severity === 1 ? '⚠' : '●';
    var typ = t.type || 'unknown';
    html += '<div class="alert-row">';
    html += '<span class="tl-dot ' + sevCls + '"></span>';
    html += '<div style="display:flex;align-items:flex-start;gap:6px">';
    html += '<span class="badge-sev ' + sevCls + '">' + sym + ' ' + sevLbl + '</span>';
    html += '<div style="flex:1;min-width:0">';
    html += '<div style="display:flex;align-items:center;gap:4px;flex-wrap:wrap">';
    html += '<span class="badge ' + typ + '" style="font-size:8px;padding:1px 4px">' + typ.toUpperCase() + '</span>';
    html += '<span style="font-size:9px;color:var(--muted)">' + fmtAge(t.ts, d.uptime_sec) + '</span>';
    html += '</div>';
    html += '<div class="alert-desc">' + (t.desc || '') + '</div>';
    html += '<div class="alert-src">' + fmtMac(t.src) + '</div>';
    html += '</div></div></div>';
  });
  html += '</div></div>';
  feed.innerHTML = html;
}

function fetchThreats() {
  fetch('/api/threats').then(function(r){ return r.json(); }).then(function(d){
    // Update pill counts
    if (d.threats) {
      var counts = {};
      d.threats.forEach(function(t){ counts[t.type] = (counts[t.type] || 0) + 1; });
      counts['all'] = d.threats.length;
      document.querySelectorAll('#threat-pills .pill').forEach(function(p){
        var f = p.dataset.filter;
        var c = p.querySelector('.cnt');
        if (c) c.textContent = (counts[f] != null) ? ' (' + counts[f] + ')' : '';
      });
    }
    renderThreats(d);
  }).catch(function(){});
}
function fetchSettings() {
  fetch('/api/settings').then(function(r){ return r.json(); }).then(renderSettings).catch(function(){});
}

// ── Poll manager ──
function startPolling() {
  stopAllPolls();
  switch (activeTab) {
    case 'overview': startPoll('status', fetchStatus, 1000); break;
    case 'devices':  startPoll('devices', fetchDevices, 3000); break;
    case 'threats':  startPoll('threats', fetchThreats, 2000); break;
    case 'settings': fetchSettings(); break;
  }
}

// Override navigate to restart polls
var _origNav = navigate;
navigate = function(tab) {
  _origNav(tab);
  setTimeout(startPolling, 50);
};
window.navigate = navigate;

// ── Boot ──
startPolling();

})();
</script>
</body>
</html>
)HTMLTEXT";

// ── API Handlers ─────────────────────────────────────────────────
static void handle_root() {
    _webserver->send(200, "text/html", String(portal_html));
}

static void handle_api_status() {
    String json = "{";
    json += "\"scanning\":true,";
    json += "\"pps\":" + String(pkt_per_sec) + ",";
    json += "\"total\":" + String(pkt_total) + ",";
    json += "\"beacons\":" + String(pkt_beacon) + ",";
    json += "\"probes\":" + String(pkt_probe) + ",";
    json += "\"deauths\":" + String(pkt_deauth) + ",";
    json += "\"channel\":" + String(WiFi.channel()) + ",";
    json += "\"ap_stations\":" + String(WiFi.softAPgetStationNum()) + ",";
    json += "\"ap_ssid\":\"" + sys_ap_ssid + "\",";
    json += "\"ap_ip\":\"" + WiFiService.getSoftAPIP() + "\",";
    json += "\"uptime_sec\":" + String(millis() / 1000) + ",";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += "}";
    _webserver->send(200, "application/json", json);
}

static void handle_api_devices() {
    String json = "{\"aps\":[";
    bool first = true;
    for (int i = 0; i < ap_count && i < MAX_AP_TABLE; i++) {
        if (!first) json += ",";
        first = false;
        uint8_t* b = ap_table[i].bssid;
        char mac[18];
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 b[0], b[1], b[2], b[3], b[4], b[5]);
        json += "{";
        json += "\"bssid\":\"" + String(mac) + "\",";
        json += "\"ssid\":\"" + String(ap_table[i].ssid) + "\",";
        json += "\"ch\":" + String(ap_table[i].channel) + ",";
        json += "\"rssi\":" + String(ap_table[i].rssi) + ",";
        json += "\"enc\":" + String(ap_table[i].encryption);
        json += "}";
    }
    json += "],\"clients\":[";
    first = true;
    uint32_t now_sec = millis() / 1000;
    for (int i = 0; i < known_device_count && i < 50; i++) {
        if (!first) json += ",";
        first = false;
        bool present = (now_sec - known_devices[i].lastSeen < 120);
        bool familiar = (!known_devices[i].trusted && known_devices[i].sightings > 1);
        const char* type_str = known_devices[i].trusted ? "trusted" : (familiar ? "familiar" : "new");
        json += "{";
        json += "\"mac\":\"" + String(known_devices[i].mac) + "\",";
        json += "\"name\":\"" + String(known_devices[i].name[0] ? known_devices[i].name : "") + "\",";
        json += "\"type\":\"" + String(type_str) + "\",";
        json += "\"trusted\":" + String(known_devices[i].trusted ? "true" : "false") + ",";
        json += "\"firstSeen\":" + String(known_devices[i].firstSeen) + ",";
        json += "\"lastSeen\":" + String(known_devices[i].lastSeen) + ",";
        json += "\"sightings\":" + String(known_devices[i].sightings) + ",";
        json += "\"present\":" + String(present ? "true" : "false");
        json += "}";
    }
    json += "],\"probes\":[";
    first = true;
    for (int i = 0; i < probe_leak_count && i < MAX_PROBE_LEAKS; i++) {
        if (!first) json += ",";
        first = false;
        json += "{\"mac\":\"";
        json += String(probe_leaks[i].client_mac);
        json += "\",\"ssid\":\"";
        json += String(probe_leaks[i].ssid);
        json += "\"}";
    }
    json += "],\"uptime_sec\":" + String(now_sec);
    json += "}";
    _webserver->send(200, "application/json", json);
}

static void handle_api_threats() {
    int tc = threat_count();
    uint32_t now_sec = millis() / 1000;
    String json = "{\"threats\":[";
    for (int i = tc - 1; i >= 0; i--) {
        const ThreatAlert* a = threat_get(i);
        if (!a) continue;
        if (i < tc - 1) json += ",";
        json += "{\"type\":\"";
        json += THREAT_TYPE_LABELS[a->type];
        json += "\",\"severity\":";
        json += String(a->severity);
        json += ",\"ts\":";
        json += String(a->timestamp);
        json += ",\"desc\":\"";
        json += String(a->desc);
        json += "\",\"src\":\"";
        json += String(a->src_mac);
        json += "\"}";
    }
    json += "],\"count\":" + String(tc);
    json += ",\"uptime_sec\":" + String(now_sec);
    json += "}";
    _webserver->send(200, "application/json", json);
}

static void handle_api_settings() {
    String json = "{";
    json += "\"device_name\":\"" + sys_device_name + "\",";
    json += "\"ap_ssid\":\"" + sys_ap_ssid + "\",";
    json += "\"ap_pass\":\"" + sys_ap_pass + "\",";
    json += "\"threat_sensitivity\":" + String(sys_threat_sensitivity) + ",";
    json += "\"homeguard_enabled\":" + String(sys_homeguard_enabled ? "true" : "false") + ",";
    json += "\"aviation_enabled\":" + String(sys_aviation_enabled ? "true" : "false");
    json += "}";
    _webserver->send(200, "application/json", json);
}

static void handle_api_save_settings() {
    Serial.printf("[SAVE] method=%d hasPlain=%d\n", _webserver->method(), _webserver->hasArg("plain"));
    if (!_webserver->hasArg("plain")) {
        String bodyFallback = _webserver->arg("plain");
        Serial.printf("[SAVE] fallback len=%d '%s'\n", bodyFallback.length(), bodyFallback.c_str());
        if (bodyFallback.length() == 0) {
            _webserver->send(400, "application/json", "{\"ok\":false,\"error\":\"empty body\"}");
            return;
        }
    }
    String body = _webserver->arg("plain");
    Serial.printf("[SAVE] body len=%d '%.100s'\n", body.length(), body.c_str());
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[SAVE] JSON err: %s\n", err.c_str());
        _webserver->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid JSON\"}");
        return;
    }

    bool changed = false;
    bool reboot_required = false;

    if (!doc["device_name"].isNull()) {
        String val = doc["device_name"].as<String>();
        if (val.length() > 0 && val != sys_device_name) {
            sys_device_name = val;
            StorageService.saveDeviceName(val);
            changed = true;
        }
    }
    if (!doc["ap_ssid"].isNull() && !doc["ap_pass"].isNull()) {
        String ssid = doc["ap_ssid"].as<String>();
        String pass = doc["ap_pass"].as<String>();
        if (ssid.length() > 0 && pass.length() >= 8) {
            if (ssid != sys_ap_ssid || pass != sys_ap_pass) {
                sys_ap_ssid = ssid;
                sys_ap_pass = pass;
                StorageService.saveAPConfig(ssid, pass);
                changed = true;
                reboot_required = true;
            }
        }
    }
    if (!doc["threat_sensitivity"].isNull()) {
        uint8_t val = doc["threat_sensitivity"].as<uint8_t>();
        if (val <= 2 && val != sys_threat_sensitivity) {
            sys_threat_sensitivity = val;
            StorageService.saveThreatSensitivity(val);
            changed = true;
        }
    }
    if (!doc["homeguard_enabled"].isNull()) {
        bool val = doc["homeguard_enabled"].as<bool>();
        if (val != sys_homeguard_enabled) {
            sys_homeguard_enabled = val;
            StorageService.saveHomeGuardEnabled(val);
            changed = true;
        }
    }
    if (!doc["aviation_enabled"].isNull()) {
        bool val = doc["aviation_enabled"].as<bool>();
        Serial.printf("[SAVE] aviation_enabled=%d sys=%d\n", val, sys_aviation_enabled);
        if (val != sys_aviation_enabled) {
            sys_aviation_enabled = val;
            StorageService.saveAviationEnabled(val);
            changed = true;
            Serial.printf("[SAVE] aviation changed to %d\n", val);
        }
    }

    String json = "{\"ok\":true";
    json += ",\"reboot_required\":" + String(reboot_required ? "true" : "false");
    json += ",\"changed\":" + String(changed ? "true" : "false");
    json += "}";
    Serial.printf("[SAVE] response: %s\n", json.c_str());
    _webserver->send(200, "application/json", json);
}

static void handle_api_trust() {
    if (!_webserver->hasArg("mac")) {
        _webserver->send(400, "application/json", "{\"ok\":false,\"error\":\"missing mac\"}");
        return;
    }
    String mac = _webserver->arg("mac");
    mac.toUpperCase();
    Serial.printf("[TRUST] toggle mac=%s count=%d\n", mac.c_str(), known_device_count);
    bool found = false;
    for (int i = 0; i < known_device_count; i++) {
        if (mac == String(known_devices[i].mac)) {
            known_devices[i].trusted = !known_devices[i].trusted;
            save_known_devices();
            found = true;
            Serial.printf("[TRUST] toggled %s -> %d\n", mac.c_str(), known_devices[i].trusted);
            break;
        }
    }
    if (found) {
        _webserver->send(200, "application/json", "{\"ok\":true}");
    } else {
        _webserver->send(404, "application/json", "{\"ok\":false,\"error\":\"device not found\"}");
    }
}

// ── Lifecycle ────────────────────────────────────────────────────
// Scanner must be started from within a task (not in setup context)
static void portal_start_scanner_task(void*) {
    WiFiService.setPromiscuous(true, sec_sniffer_cb);
    ch_hopping = true;
    vTaskDelete(nullptr);
}

void portal_setup(TFT_eSprite* tft) {
    _stft = tft;
    _threat_count = 0;
    threat_clear();

    Serial.println("[PORTAL] Starting WebServer...");
    _webserver = new WebServer(SEC_WEB_PORT);

    _webserver->on("/",               HTTP_GET, handle_root);
    _webserver->on("/api/status",     HTTP_GET, handle_api_status);
    _webserver->on("/api/devices",    HTTP_GET, handle_api_devices);
    _webserver->on("/api/threats",    HTTP_GET, handle_api_threats);
    _webserver->on("/api/settings",   HTTP_GET,  handle_api_settings);
    _webserver->on("/api/settings",   HTTP_POST, handle_api_save_settings);
    _webserver->on("/api/trust",      HTTP_GET,  handle_api_trust);

    _webserver->begin();
    Serial.printf("[PORTAL] WebServer on port %d\n", SEC_WEB_PORT);
    Serial.printf("[PORTAL] Open http://%s\n", WiFiService.getSoftAPIP().c_str());

    xTaskCreatePinnedToCore(portal_start_scanner_task, "scnStart", 4096, nullptr, 1, nullptr, 1);

    _pps_last_calc = millis();
    portal_draw_display();
}

void portal_teardown() {
    ch_hopping = false;
    WiFiService.setPromiscuous(false);
    if (_webserver) {
        _webserver->stop();
        delete _webserver;
        _webserver = nullptr;
    }
    _stft = nullptr;
}

void portal_core0_task() {
    // Channel hopping
    static uint32_t last_ch_hop = 0;
    uint32_t now = millis();
    if (now - last_ch_hop >= CHANNEL_HOP_MS) {
        last_ch_hop = now;
        current_ch = (current_ch % 13) + 1;
        WiFiService.setChannel(current_ch);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
}

void portal_core1_task() {
    if (!_stft) return;

    uint32_t now = millis();

    // PPS calculation every 1s
    if (now - _pps_last_calc >= 1000) {
        pkt_per_sec = pkt_total - _prev_total;
        _prev_total = pkt_total;
        _pps_last_calc = now;
    }

    // ── Threat detection (every 2s) ──
    static uint32_t _threat_check_ms = 0;
    static uint32_t _prev_deauth     = 0;
    static bool     _prev_evil_twin  = false;
    static int      _prev_known_dev  = 0;
    static int      _prev_probes     = 0;
    static int      _prev_ap_count   = 0;
    static uint32_t _flood_start_ms  = 0;

    if (now - _threat_check_ms >= 2000) {
        _threat_check_ms = now;

        // 1. New AP discovered
        if (ap_count > _prev_ap_count) {
            for (int i = _prev_ap_count; i < ap_count; i++) {
                char desc[48];
                snprintf(desc, sizeof(desc), "New AP detected: %s", ap_table[i].ssid);
                (void)threat_add_dedup(THREAT_NEW_AP, THREAT_INFO,
                                       ap_table[i].ssid, desc);
            }
        }

        // 2. Probe leak detection
        if (probe_leak_count > _prev_probes) {
            for (int i = _prev_probes; i < probe_leak_count; i++) {
                char desc[48];
                snprintf(desc, sizeof(desc), "Probe leak: %s", probe_leaks[i].ssid);
                (void)threat_add_dedup(THREAT_PROBE_LEAK, THREAT_WARNING,
                                       probe_leaks[i].client_mac, desc);
            }
        }

        // 3. Unknown device / randomized MAC detection
        if (known_device_count > _prev_known_dev) {
            for (int i = _prev_known_dev; i < known_device_count; i++) {
                uint32_t age = (now / 1000) - known_devices[i].firstSeen;
                if (age > 3) continue;
                bool random = (strcmp(known_devices[i].name, "Android Private MAC") == 0);
                if (random) {
                    (void)threat_add_dedup(THREAT_RANDOMIZED_MAC, THREAT_INFO,
                                           known_devices[i].mac, "Randomized MAC observed");
                } else {
                    (void)threat_add_dedup(THREAT_UNKNOWN_DEVICE, THREAT_WARNING,
                                           known_devices[i].mac, "Unknown device detected");
                }
            }
        }

        // 4. Deauth burst detection
        if (pkt_deauth > _prev_deauth + getDeauthThreshold()) {
            char desc[32];
            snprintf(desc, sizeof(desc), "%lu deauth frames", pkt_deauth - _prev_deauth);
            (void)threat_add_dedup(THREAT_DEAUTH, THREAT_CRITICAL, "", desc);
        }

        // 5. Evil twin detection
        if (evil_twin_detected && !_prev_evil_twin) {
            (void)threat_add_dedup(THREAT_EVIL_TWIN, THREAT_CRITICAL,
                                   evil_twin_attacker_bssid, "Rogue AP detected");
        }

        // 6. Channel flood detection (sustained high PPS)
        if (pkt_per_sec > 150) {
            if (_flood_start_ms == 0) _flood_start_ms = now;
            else if (now - _flood_start_ms >= 4000) {
                (void)threat_add_dedup(THREAT_CHANNEL_FLOOD, THREAT_WARNING,
                                       "", "High traffic on channel");
                _flood_start_ms = now;
            }
        } else {
            _flood_start_ms = 0;
        }

        // 7. Trusted device return detection
        {
            struct TdEntry { char mac[18]; bool was_away; };
            static TdEntry td[16] = {};
            static int td_count = 0;

            for (int i = 0; i < known_device_count; i++) {
                if (!known_devices[i].trusted) continue;

                uint32_t sec = now / 1000;
                bool present = (sec - known_devices[i].lastSeen < 120);

                int idx = -1;
                for (int j = 0; j < td_count; j++) {
                    if (strcmp(td[j].mac, known_devices[i].mac) == 0) { idx = j; break; }
                }

                if (idx < 0) {
                    if (td_count < 16) {
                        idx = td_count++;
                        strncpy(td[idx].mac, known_devices[i].mac, 17);
                        td[idx].mac[17] = '\0';
                        td[idx].was_away = !present;
                    }
                } else {
                    if (td[idx].was_away && present) {
                        char desc[48];
                        snprintf(desc, sizeof(desc), "Trusted device returned: %s",
                                 known_devices[i].name[0]
                                     ? known_devices[i].name
                                     : known_devices[i].mac);
                        (void)threat_add_dedup(THREAT_TRUSTED_RETURN, THREAT_INFO,
                                               known_devices[i].mac, desc);
                    }
                    td[idx].was_away = !present;
                }
            }
        }

        _prev_deauth = pkt_deauth;
        _prev_evil_twin = evil_twin_detected;
        _prev_known_dev = known_device_count;
        _prev_probes = probe_leak_count;
        _prev_ap_count = ap_count;
    }

    // Update TFT threat count from engine
    _threat_count = threat_count();

    // Toggle HUD display on tap
    if (g_touch_tap) {
        g_touch_tap = false;
        sec_hud_mode = !sec_hud_mode;
        Serial.printf("[SEC] Touch tap detected, sec_hud_mode=%d\n", sec_hud_mode);
    }

    // Dynamic Emotion Mapping based on security state:
    // If evil twin or deauth active, set EMOTION_ANGRY/EMOTION_ALERT.
    // If unknown device/probe leaks set EMOTION_CURIOUS.
    // If trusted return set EMOTION_LOVE.
    if (!sec_hud_mode) {
        static uint32_t last_emotion_check = 0;
        if (now - last_emotion_check >= 1000) {
            last_emotion_check = now;
            int tc = threat_count();
            if (tc > 0) {
                const ThreatAlert* last_alert = threat_get(tc - 1);
                if (last_alert) {
                    uint32_t alert_age = (now / 1000) - last_alert->timestamp;
                    if (alert_age < 15) { // Only change emotion for relatively fresh alerts
                        if (last_alert->severity == THREAT_CRITICAL) {
                            EmotionEngine.setEmotion(EMOTION_ANGRY);
                        } else if (last_alert->type == THREAT_TRUSTED_RETURN) {
                            EmotionEngine.setEmotion(EMOTION_LOVE);
                        } else {
                            EmotionEngine.setEmotion(EMOTION_CURIOUS);
                        }
                    } else {
                        // Fall back to calm monitoring if alerts are old
                        EmotionEngine.setEmotion(EMOTION_CALM);
                    }
                }
            } else {
                EmotionEngine.setEmotion(EMOTION_CALM);
            }
        }
    }

    if (_webserver) _webserver->handleClient();
    portal_draw_display();
    vTaskDelay(pdMS_TO_TICKS(50));
}

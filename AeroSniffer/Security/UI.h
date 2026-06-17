// ================================================================
//  Security/UI.h  —  Web Dashboard & TFT Display Renderer
//  AeroSniffer Security Layer | Modular Subcomponent
// ================================================================
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
using fs::FS;
#include <WebServer.h>
#include <TFT_eSPI.h>
#include "AeroSnifferOS.h"
#include "Statistics.h"
#include "HomeGuard.h"
#include "EvilTwin.h"

#include "Sniffer.h"

static TFT_eSprite* _stft = nullptr;
static WebServer* _webserver = nullptr;

// ── Radar sweep animation state ──────────────────────────────────
static float    _sweep_angle    = 0.0f;
static uint32_t _sweep_last_ms  = 0;

// Embed the raw client HTML dashboard
static void sec_handle_root() {
  Serial.printf("[SEC] WEB REQ client=%s uri=%s\n",
    _webserver->client().remoteIP().toString().c_str(), _webserver->uri().c_str());
  String html = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AeroSniffer // SOC</title>
<style>
  :root{
    --bg:#070a08;
    --panel:#0e1611;
    --line:#1c2b22;
    --green:#39ff88;
    --green-dim:#1e8a52;
    --amber:#ffb000;
    --purple:#c084fc;
    --text:#d7e8da;
    --muted:#5c7468;
    --mono: 'JetBrains Mono','IBM Plex Mono',ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
  }
  *{margin:0;padding:0;box-sizing:border-box;}
  html,body{background:var(--bg);color:var(--text);font-family:var(--mono);}
  body{
    padding:18px;
    line-height:1.5;
    background-image:
      repeating-linear-gradient(transparent 0 2px, rgba(57,255,136,0.012) 2px 4px);
    font-size:14px;
  }

  /* ---------- HEADER ---------- */
  header{
    position:relative;
    border:1px solid var(--line);
    border-radius:4px;
    padding:14px 16px;
    margin-bottom:16px;
    overflow:hidden;
    background:linear-gradient(180deg, rgba(57,255,136,0.05), transparent 60%);
  }
  header::after{
    content:"";
    position:absolute; left:0; right:0; top:0; height:2px;
    background:linear-gradient(90deg, transparent, var(--green), transparent);
    animation: sweep 3.4s linear infinite;
    opacity:.55;
  }
  @keyframes sweep{
    0%{transform:translateX(-100%);}
    100%{transform:translateX(100%);}
  }
  .hdr-row{display:flex; align-items:baseline; justify-content:space-between; flex-wrap:wrap; gap:8px;}
  h1{
    font-size:1.15em;
    font-weight:700;
    letter-spacing:.18em;
    color:var(--green);
    text-transform:uppercase;
    display:flex; align-items:center; gap:10px;
  }
  h1 .tag{
    font-size:.6em;
    letter-spacing:.1em;
    color:var(--muted);
    border:1px solid var(--line);
    padding:2px 6px;
    border-radius:3px;
    font-weight:400;
  }
  .status-line{
    font-size:.8em;
    color:var(--muted);
    display:flex; align-items:center; gap:8px;
    letter-spacing:.05em;
  }
  .status-dot{
    width:8px; height:8px; border-radius:50%;
    background:#5a2222;
    box-shadow:0 0 0 1px rgba(239,68,68,.25);
    animation: blink 1.1s infinite alternate;
  }
  .status-dot.active{
    background:var(--green);
    box-shadow:0 0 8px var(--green);
    animation: solidpulse 1.6s infinite alternate;
  }
  @keyframes blink{from{opacity:.35;}to{opacity:1;}}
  @keyframes solidpulse{from{opacity:.6;}to{opacity:1;}}

  /* ---------- NAV ---------- */
  .nav-tabs{
    display:grid;
    grid-template-columns: repeat(5, 1fr);
    gap:1px;
    margin-bottom:16px;
    border:1px solid var(--line);
    border-radius:4px;
    overflow:hidden;
    background:var(--line);
  }
  .tab-btn{
    background:var(--panel);
    border:none;
    color:var(--muted);
    padding:11px 6px;
    font-size:.72em;
    font-family:var(--mono);
    letter-spacing:.12em;
    text-transform:uppercase;
    cursor:pointer;
    transition:.15s;
    text-align:center;
    position:relative;
  }
  .tab-btn .freq{
    display:block;
    font-size:.85em;
    color:var(--muted);
    margin-top:3px;
    letter-spacing:.05em;
  }
  .tab-btn:hover{ color:var(--text); background:#121d16; }
  .tab-btn.active{
    color:var(--green);
    background:#0c1a13;
    box-shadow: inset 0 -2px 0 var(--green);
  }
  .tab-btn.active .freq{ color:var(--green-dim); }

  .tab-content{display:none;}
  .tab-content.active{display:block; animation:fadein .25s ease;}
  @keyframes fadein{from{opacity:0;transform:translateY(4px);}to{opacity:1;transform:translateY(0);}}

  /* ---------- CARDS ---------- */
  .card{
    background:var(--panel);
    border:1px solid var(--line);
    border-radius:4px;
    padding:16px;
    margin-bottom:14px;
  }
  .card-title{
    color:var(--muted);
    font-size:.72em;
    text-transform:uppercase;
    letter-spacing:.2em;
    margin-bottom:12px;
    font-weight:700;
    display:flex; align-items:center; gap:8px;
  }
  .card-title::before{
    content:"";
    width:8px; height:8px;
    border:1px solid var(--green-dim);
    transform:rotate(45deg);
    flex-shrink:0;
  }
  .card-note{
    font-size:.78em;
    color:var(--muted);
    margin-bottom:12px;
    line-height:1.6;
  }

  /* ---------- STATS ---------- */
  .stat-grid{
    display:grid;
    grid-template-columns: repeat(auto-fit, minmax(110px,1fr));
    gap:10px;
  }
  .stat-card{
    border:1px solid var(--line);
    border-radius:3px;
    padding:10px 12px;
    display:flex; flex-direction:column; gap:4px;
    background:#0a120d;
  }
  .stat-label{
    font-size:.68em;
    color:var(--muted);
    text-transform:uppercase;
    letter-spacing:.15em;
  }
  .stat-value{
    font-size:1.5em;
    font-weight:700;
    color:var(--text);
    font-family:var(--mono);
    letter-spacing:.02em;
  }
  .stat-value.cyan{ color:var(--green); }
  .stat-value.purple{ color:var(--purple); }
  .stat-value.red{ color:#ff5d5d; }

  /* ---------- BUTTONS ---------- */
  .btn{
    display:inline-flex;
    align-items:center;
    justify-content:center;
    gap:6px;
    padding:10px 16px;
    background:transparent;
    border:1px solid var(--green-dim);
    color:var(--green);
    font-size:.78em;
    font-weight:700;
    letter-spacing:.1em;
    text-transform:uppercase;
    border-radius:3px;
    cursor:pointer;
    transition:.15s;
    font-family:var(--mono);
  }
  .btn:hover{ background:rgba(57,255,136,.08); border-color:var(--green); }
  .btn.danger{ border-color:#7a3030; color:#ff5d5d; }
  .btn.danger:hover{ background:rgba(255,93,93,.08); border-color:#ff5d5d; }
  .btn.action-btn{ padding:5px 10px; font-size:.7em; letter-spacing:.05em; }
  .btn-group{ display:flex; gap:8px; flex-wrap:wrap; margin-top:2px; }

  /* ---------- ALARM ---------- */
  .alarm-banner{
    display:none;
    background:repeating-linear-gradient(135deg, rgba(255,93,93,.08) 0 10px, rgba(255,93,93,.03) 10px 20px);
    border:1px solid #ff5d5d;
    border-radius:4px;
    padding:16px;
    margin-bottom:16px;
    box-shadow:0 0 16px rgba(255,93,93,.15);
  }
  .alarm-title{
    color:#ff5d5d;
    font-size:.95em;
    font-weight:700;
    letter-spacing:.15em;
    text-transform:uppercase;
    margin-bottom:8px;
    display:flex; align-items:center; gap:8px;
  }
  .alarm-title::before{ content:"▲"; font-size:.85em; }
  #alarm-details{
    font-size:.82em;
    color:#ffd0d0;
    white-space:pre-line;
    margin-bottom:12px;
    border-left:2px solid #ff5d5d;
    padding-left:10px;
  }

  /* ---------- TABLES ---------- */
  table{ width:100%; border-collapse:collapse; font-size:.82em; text-align:left; }
  thead th{
    color:var(--muted);
    font-weight:700;
    padding:6px 8px;
    border-bottom:1px solid var(--line);
    text-transform:uppercase;
    font-size:.78em;
    letter-spacing:.1em;
    position:sticky; top:0; background:var(--panel);
  }
  td{
    padding:7px 8px;
    border-bottom:1px solid var(--line);
    color:var(--text);
    font-family:var(--mono);
  }
  tbody tr:hover td{ background:rgba(57,255,136,.03); }
  .scrollable-table{ max-height:260px; overflow-y:auto; border:1px solid var(--line); border-radius:3px; }
  .scrollable-table::-webkit-scrollbar{ width:6px; }
  .scrollable-table::-webkit-scrollbar-thumb{ background:var(--green-dim); border-radius:3px; }

  /* protected info row */
  .protect-row{
    display:flex; gap:10px; flex-wrap:wrap; margin-bottom:14px;
  }

  footer{
    text-align:center;
    color:var(--muted);
    font-size:.7em;
    letter-spacing:.15em;
    text-transform:uppercase;
    margin-top:24px;
    padding-top:14px;
    border-top:1px solid var(--line);
  }

  @media (prefers-reduced-motion: reduce){
    header::after, .status-dot{ animation:none; }
  }
</style>
</head>
<body>

  <header>
    <div class="hdr-row">
      <h1>AeroSniffer <span class="tag">SOC v2.0</span></h1>
      <div class="status-line">
        <span id="device-status-lbl">IDLE</span>
        <span class="status-dot" id="status-indicator"></span>
      </div>
    </div>
  </header>

  <div class="alarm-banner" id="evil-twin-banner">
    <div class="alarm-title">Rogue Access Point Detected</div>
    <p id="alarm-details"></p>
    <button class="btn danger action-btn" onclick="clearEvilTwinAlarm()">Dismiss / Reset Alarm</button>
  </div>

  <div class="nav-tabs">
    <button class="tab-btn active" onclick="switchTab('tab-dashboard')">Dashboard<span class="freq">2.4G</span></button>
    <button class="tab-btn" onclick="switchTab('tab-twin')">Evil Twin<span class="freq">AP-GUARD</span></button>
    <button class="tab-btn" onclick="switchTab('tab-probes')">Probes<span class="freq">CLIENT-RX</span></button>
    <button class="tab-btn" onclick="switchTab('tab-homeguard')">Home Guard<span class="freq">DEFENSE</span></button>
  </div>

  <!-- DASHBOARD -->
  <div class="tab-content active" id="tab-dashboard">
    <div class="card">
      <div class="card-title">Console Controls</div>
      <div class="btn-group">
        <button class="btn" onclick="startScan()">▶ Start Wi-Fi Scan</button>
        <button class="btn danger" onclick="stopScan()">■ Stop Wi-Fi Scan</button>
      </div>
    </div>
    <div class="card">
      <div class="card-title">Packet Capture Statistics</div>
      <div class="stat-grid">
        <div class="stat-card"><span class="stat-label">PPS</span><span class="stat-value cyan" id="stat-pps">0</span></div>
        <div class="stat-card"><span class="stat-label">Total</span><span class="stat-value" id="stat-total">0</span></div>
        <div class="stat-card"><span class="stat-label">Beacons</span><span class="stat-value" id="stat-beacons">0</span></div>
        <div class="stat-card"><span class="stat-label">Probes</span><span class="stat-value" id="stat-probes">0</span></div>
        <div class="stat-card"><span class="stat-label">Deauths</span><span class="stat-value red" id="stat-deauths">0</span></div>
      </div>
    </div>
  </div>

  <!-- EVIL TWIN -->
  <div class="tab-content" id="tab-twin">
    <div class="card">
      <div class="card-title">Active Protection Whitelist</div>
      <div class="stat-grid protect-row">
        <div class="stat-card"><span class="stat-label">Protected SSID</span><span class="stat-value cyan" id="protected-ssid-lbl">None</span></div>
        <div class="stat-card"><span class="stat-label">Trusted BSSID</span><span class="stat-value" id="protected-bssid-lbl">00:00:00:00:00:00</span></div>
      </div>
      <p class="card-note">Select any detected access point from the list below to set it as your trusted protected network.</p>
    </div>
    <div class="card">
      <div class="card-title">Detected Access Points</div>
      <div class="scrollable-table">
        <table>
          <thead>
            <tr><th>SSID</th><th>BSSID</th><th>Ch</th><th>RSSI</th><th>Action</th></tr>
          </thead>
          <tbody id="ap-list-body">
            <tr><td colspan="5" style="text-align:center;color:var(--muted)">No networks detected yet. Start scanning.</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- PROBES -->
  <div class="tab-content" id="tab-probes">
    <div class="card">
      <div class="card-title">Probe Request Leaks (Nearby Devices)</div>
      <p class="card-note">Shows nearby devices (phones, tablets) broadcasting SSIDs of networks they have previously connected to.</p>
      <div class="scrollable-table">
        <table>
          <thead>
            <tr><th>Client MAC</th><th>Leaked Network SSID</th><th>Last Seen</th></tr>
          </thead>
          <tbody id="probe-list-body">
            <tr><td colspan="3" style="text-align:center;color:var(--muted)">No probe requests captured yet.</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- HOME GUARD -->
  <div class="tab-content" id="tab-homeguard">
    <div class="card">
      <div class="card-title">Welcome Home Device Detector</div>
      <p class="card-note">Enter your personal phone/smartwatch MAC address. When AeroSniffer detects this MAC, it will flash a warm greeting message on its screen.</p>
      <div style="display:flex;gap:10px;margin-bottom:10px;flex-wrap:wrap;">
        <input type="text" id="welcome-mac-input" placeholder="MAC Address (XX:XX:XX:XX:XX:XX)" style="background:#0a120d;border:1px solid var(--line);color:var(--text);padding:8px;border-radius:3px;font-family:var(--mono);flex:1;min-width:180px;">
        <input type="text" id="welcome-name-input" placeholder="Your Name" style="background:#0a120d;border:1px solid var(--line);color:var(--text);padding:8px;border-radius:3px;font-family:var(--mono);flex:1;min-width:120px;">
        <button class="btn" onclick="saveWelcomeSettings()">Save Settings</button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">Wi-Fi Hot & Cold Signal Finder</div>
      <p class="card-note">Lock onto a specific SSID to measure real-time signal strength (RSSI) on the AeroSniffer display and map out home dead zones.</p>
      <div style="display:flex;gap:10px;margin-bottom:10px;flex-wrap:wrap;">
        <input type="text" id="find-ssid-input" placeholder="SSID to Find" style="background:#0a120d;border:1px solid var(--line);color:var(--text);padding:8px;border-radius:3px;font-family:var(--mono);flex:1;min-width:180px;">
        <button class="btn" id="find-btn" onclick="toggleFindMode()">Start Signal Finder</button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">Home Network Whitelist (Intruder Alert)</div>
      <p class="card-note">Add MAC addresses of trusted family/guest devices. Any other device connecting to your Home Router BSSID will trigger an immediate Intruder alert.</p>
      <div style="display:flex;gap:10px;margin-bottom:12px;flex-wrap:wrap;">
        <input type="text" id="wl-mac-input" placeholder="MAC Address" style="background:#0a120d;border:1px solid var(--line);color:var(--text);padding:8px;border-radius:3px;font-family:var(--mono);flex:1;min-width:180px;">
        <input type="text" id="wl-name-input" placeholder="Device Name (e.g. Mom's iPhone)" style="background:#0a120d;border:1px solid var(--line);color:var(--text);padding:8px;border-radius:3px;font-family:var(--mono);flex:1;min-width:180px;">
        <button class="btn" onclick="addWhitelistedDevice()">Add Device</button>
      </div>
      <div class="scrollable-table">
        <table>
          <thead>
            <tr><th>MAC Address</th><th>Device Name</th><th>Action</th></tr>
          </thead>
          <tbody id="whitelist-body">
            <tr><td colspan="3" style="text-align:center;color:var(--muted)">No whitelisted devices yet.</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <footer>AeroSniffer v2.0 :: DeskBuddy 2.0 SOC :: 802.11 Sensor</footer>

  <script>
    function switchTab(tabId){
      document.querySelectorAll('.tab-btn').forEach(btn=>btn.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(cont=>cont.classList.remove('active'));
      const tabEl=document.getElementById(tabId);
      tabEl.classList.add('active');
      const buttons=document.querySelectorAll('.tab-btn');
      if(tabId==='tab-dashboard')buttons[0].classList.add('active');
      else if(tabId==='tab-twin')buttons[1].classList.add('active');
      else if(tabId==='tab-probes')buttons[2].classList.add('active');
      else if(tabId==='tab-homeguard')buttons[3].classList.add('active');
      loadTabData(tabId);
    }
    function startScan(){fetch('/api/scan/start')}
    function stopScan(){fetch('/api/scan/stop')}
    function setProtected(ssid,bssid){
      fetch(`/api/set_protected?ssid=${encodeURIComponent(ssid)}&bssid=${encodeURIComponent(bssid)}`).then(()=>alert(`Protected target set: ${ssid}`));
    }
    function clearEvilTwinAlarm(){fetch('/api/clear_evil_twin')}

    function saveWelcomeSettings() {
      const mac = document.getElementById('welcome-mac-input').value.trim();
      const name = document.getElementById('welcome-name-input').value.trim();
      fetch(`/api/homeguard/set_welcome?mac=${encodeURIComponent(mac)}&name=${encodeURIComponent(name)}`)
        .then(r => {
          if (r.ok) alert('Welcome Home settings saved!');
          else alert('Failed to save settings.');
        });
    }

    function toggleFindMode() {
      const ssid = document.getElementById('find-ssid-input').value.trim();
      const btn = document.getElementById('find-btn');
      const active = btn.innerText.includes('Stop');
      const targetSsid = active ? '' : ssid;
      fetch(`/api/homeguard/find/toggle?ssid=${encodeURIComponent(targetSsid)}`)
        .then(r => {
          if (r.ok) {
            if (active) {
              btn.innerText = 'Start Signal Finder';
              btn.classList.remove('danger');
            } else {
              btn.innerText = 'Stop Signal Finder';
              btn.classList.add('danger');
            }
          }
        });
    }

    function addWhitelistedDevice() {
      const mac = document.getElementById('wl-mac-input').value.trim();
      const name = document.getElementById('wl-name-input').value.trim();
      fetch(`/api/homeguard/whitelist/add?mac=${encodeURIComponent(mac)}&name=${encodeURIComponent(name)}`)
        .then(r => {
          if (r.ok) {
            alert('Device added to whitelist!');
            loadHomeGuardConfig();
            document.getElementById('wl-mac-input').value = '';
            document.getElementById('wl-name-input').value = '';
          }
        });
    }

    function removeWhitelistedDevice(mac) {
      fetch(`/api/homeguard/whitelist/remove?mac=${encodeURIComponent(mac)}`)
        .then(r => {
          if (r.ok) {
            alert('Device removed!');
            loadHomeGuardConfig();
          }
        });
    }

    function loadHomeGuardConfig() {
      fetch('/api/homeguard/config').then(r => r.json()).then(d => {
        document.getElementById('welcome-mac-input').value = d.welcome_mac || '';
        document.getElementById('welcome-name-input').value = d.welcome_name || '';
        document.getElementById('find-ssid-input').value = d.find_ssid || '';
        
        const btn = document.getElementById('find-btn');
        if (d.find_active) {
          btn.innerText = `Stop Signal Finder (${d.find_rssi > -100 ? d.find_rssi + ' dBm' : 'Searching...'})`;
          btn.classList.add('danger');
        } else {
          btn.innerText = 'Start Signal Finder';
          btn.classList.remove('danger');
        }
        
        let html = '';
        if (!d.whitelist || d.whitelist.length === 0) {
          html = '<tr><td colspan="3" style="text-align:center;color:var(--muted)">No whitelisted devices yet.</td></tr>';
        } else {
          d.whitelist.forEach(dev => {
            html += `<tr>
              <td>${dev.mac}</td>
              <td>${dev.name}</td>
              <td><button class="btn danger action-btn" onclick="removeWhitelistedDevice('${dev.mac}')">Remove</button></td>
            </tr>`;
          });
        }
        document.getElementById('whitelist-body').innerHTML = html;
      });
    }

    function loadTabData(tabId){
      if(tabId==='tab-twin'){
        fetch('/api/aps').then(r=>r.json()).then(d=>{
          let html='';
          if(!d.aps||d.aps.length===0){
            html='<tr><td colspan="5" style="text-align:center;color:var(--muted)">No networks detected yet.</td></tr>';
          }else{
            d.aps.forEach(ap=>{
              html+=`<tr>
                <td>${ap.ssid||'<i>Hidden SSID</i>'}</td>
                <td>${ap.bssid}</td>
                <td>${ap.ch}</td>
                <td>${ap.rssi} dBm</td>
                <td><button class="btn action-btn" onclick="setProtected('${ap.ssid}','${ap.bssid}')">Protect</button></td>
              </tr>`;
            });
          }
          document.getElementById('ap-list-body').innerHTML=html;
        });
      }else if(tabId==='tab-probes'){
        fetch('/api/probes').then(r=>r.json()).then(d=>{
          let html='';
          if(!d.probes||d.probes.length===0){
            html='<tr><td colspan="3" style="text-align:center;color:var(--muted)">No probe requests captured.</td></tr>';
          }else{
            d.probes.forEach(pr=>{
              html+=`<tr>
                <td>${pr.mac}</td>
                <td style="color:var(--green)">${pr.ssid}</td>
                <td>${pr.seen}s ago</td>
              </tr>`;
            });
          }
          document.getElementById('probe-list-body').innerHTML=html;
        });
      }else if(tabId==='tab-homeguard'){
        loadHomeGuardConfig();
      }
    }
    setInterval(()=>{
      fetch('/api/stats').then(r=>r.json()).then(d=>{
        document.getElementById('device-status-lbl').innerText=d.scanning?'SCANNING':'IDLE';
        const ind=document.getElementById('status-indicator');
        if(d.scanning)ind.classList.add('active');else ind.classList.remove('active');
        document.getElementById('stat-pps').innerText=d.pps;
        document.getElementById('stat-total').innerText=d.total;
        document.getElementById('stat-beacons').innerText=d.beacons;
        document.getElementById('stat-probes').innerText=d.probes;
        document.getElementById('stat-deauths').innerText=d.deauths;
        document.getElementById('protected-ssid-lbl').innerText=d.evil_twin_target||'None';
        document.getElementById('protected-bssid-lbl').innerText=d.evil_twin_trusted||'00:00:00:00:00:00';
        const banner=document.getElementById('evil-twin-banner');
        if(d.evil_twin_detected){
          banner.style.display='block';
          document.getElementById('alarm-details').innerText=`Target Network SSID: ${d.evil_twin_target}\nTrusted Router MAC: ${d.evil_twin_trusted}\nATTACKER ROUTER MAC: ${d.evil_twin_attacker}`;
        }else{
          banner.style.display='none';
        }
      });
      const activeTab=document.querySelector('.tab-content.active');
      if(activeTab)loadTabData(activeTab.id);
    },1000);
  </script>
</body>
</html>)rawliteral";
  _webserver->send(200, "text/html", html);
}

static void sec_handle_stats() {
  Serial.printf("[SEC] WEB REQ client=%s uri=%s\n",
    _webserver->client().remoteIP().toString().c_str(), _webserver->uri().c_str());
  String bssid_str = "";
  char bssid_buf[20];
  snprintf(bssid_buf, sizeof(bssid_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           evil_twin_trusted_bssid[0], evil_twin_trusted_bssid[1], evil_twin_trusted_bssid[2],
           evil_twin_trusted_bssid[3], evil_twin_trusted_bssid[4], evil_twin_trusted_bssid[5]);
  bssid_str = bssid_buf;

  char json[512];
  snprintf(json, sizeof(json),
    "{\"scanning\":%s,\"ch\":%d,\"pps\":%lu,\"total\":%lu,"
    "\"beacons\":%lu,\"probes\":%lu,\"deauths\":%lu,"
    "\"evil_twin_detected\":%s,\"evil_twin_target\":\"%s\",\"evil_twin_trusted\":\"%s\",\"evil_twin_attacker\":\"%s\"}",
    sec_scanning ? "true" : "false", current_ch,
    pkt_per_sec, pkt_total, pkt_beacon, pkt_probe, pkt_deauth,
    evil_twin_detected ? "true" : "false", evil_twin_target_ssid, bssid_str.c_str(), evil_twin_attacker_bssid);
  _webserver->send(200, "application/json", json);
}

static void sec_handle_aps() {
  String json = "{\"aps\":[";
  for (int i = 0; i < ap_count; i++) {
    if (i > 0) json += ",";
    char ap_json[128];
    snprintf(ap_json, sizeof(ap_json),
             "{\"ssid\":\"%s\",\"bssid\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"rssi\":%d,\"ch\":%d}",
             ap_table[i].ssid,
             ap_table[i].bssid[0], ap_table[i].bssid[1], ap_table[i].bssid[2],
             ap_table[i].bssid[3], ap_table[i].bssid[4], ap_table[i].bssid[5],
             ap_table[i].rssi, ap_table[i].channel);
    json += ap_json;
  }
  json += "]}";
  _webserver->send(200, "application/json", json);
}

static void sec_handle_probes() {
  String json = "{\"probes\":[";
  for (int i = 0; i < probe_leak_count; i++) {
    if (i > 0) json += ",";
    char pr_json[128];
    snprintf(pr_json, sizeof(pr_json),
             "{\"mac\":\"%s\",\"ssid\":\"%s\",\"seen\":%lu}",
             probe_leaks[i].client_mac, probe_leaks[i].ssid, (millis() - probe_leaks[i].last_seen) / 1000);
    json += pr_json;
  }
  json += "]}";
  _webserver->send(200, "application/json", json);
}

static void sec_handle_set_protected() {
  if (_webserver->hasArg("ssid") && _webserver->hasArg("bssid")) {
    String ssid = _webserver->arg("ssid");
    String bssid_str = _webserver->arg("bssid");
    
    uint8_t bssid[6] = {0};
    int values[6];
    if (sscanf(bssid_str.c_str(), "%x:%x:%x:%x:%x:%x", 
               &values[0], &values[1], &values[2], 
               &values[3], &values[4], &values[5]) == 6) {
      for (int i = 0; i < 6; i++) {
        bssid[i] = (uint8_t)values[i];
      }
    }
    
    strcpy(evil_twin_target_ssid, ssid.c_str());
    memcpy(evil_twin_trusted_bssid, bssid, 6);
    evil_twin_detected = false;
    evil_twin_attacker_bssid[0] = '\0';
    
    Preferences prefs;
    prefs.begin("aerosniffer", false);
    prefs.putString("tr_ssid", ssid);
    prefs.putBytes("tr_bssid", bssid, 6);
    prefs.end();
    
    _webserver->send(200, "text/plain", "OK");
  } else {
    _webserver->send(400, "text/plain", "Missing ssid or bssid");
  }
}

static void sec_handle_clear_evil_twin() {
  evil_twin_detected = false;
  evil_twin_attacker_bssid[0] = '\0';
  _webserver->send(200, "text/plain", "OK");
}

static void sec_handle_hg_config() {
  String json = "{";
  json += "\"welcome_mac\":\"" + format_mac_bytes(welcome_mac) + "\",";
  json += "\"welcome_name\":\"" + String(welcome_name) + "\",";
  json += "\"find_ssid\":\"" + String(find_ssid) + "\",";
  json += "\"find_active\":" + String(find_mode_active ? "true" : "false") + ",";
  json += "\"find_rssi\":" + String(find_rssi) + ",";
  
  json += "\"whitelist\":[";
  for (int i = 0; i < known_device_count; i++) {
    if (i > 0) json += ",";
    json += "{\"mac\":\"" + String(known_devices[i].mac) + "\",";
    json += "\"name\":\"" + String(known_devices[i].name) + "\",";
    json += "\"firstSeen\":" + String(known_devices[i].firstSeen) + ",";
    json += "\"lastSeen\":" + String(known_devices[i].lastSeen) + ",";
    json += "\"sightings\":" + String(known_devices[i].sightings) + ",";
    json += "\"trusted\":" + String(known_devices[i].trusted ? "true" : "false") + "}";
  }
  json += "]}";
  
  _webserver->send(200, "application/json", json);
}

static void sec_handle_hg_set_welcome() {
  if (_webserver->hasArg("mac") && _webserver->hasArg("name")) {
    String mac_str = _webserver->arg("mac");
    String name_str = _webserver->arg("name");
    
    uint8_t parsed_mac[6] = {0};
    if (parse_mac_string(mac_str, parsed_mac)) {
      memcpy(welcome_mac, parsed_mac, 6);
      strncpy(welcome_name, name_str.c_str(), sizeof(welcome_name));
      
      Preferences prefs;
      prefs.begin("aerosniffer", false);
      prefs.putBytes("wl_mac", welcome_mac, 6);
      prefs.putString("wl_name", name_str);
      prefs.end();
      
      _webserver->send(200, "text/plain", "OK");
      return;
    }
  }
  _webserver->send(400, "text/plain", "Invalid arguments");
}

static void sec_handle_hg_wl_add() {
  if (_webserver->hasArg("mac") && _webserver->hasArg("name")) {
    String mac_str = _webserver->arg("mac");
    String name_str = _webserver->arg("name");
    
    uint8_t parsed_mac[6] = {0};
    if (parse_mac_string(mac_str, parsed_mac)) {
      char norm_mac[18];
      snprintf(norm_mac, sizeof(norm_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
               parsed_mac[0], parsed_mac[1], parsed_mac[2],
               parsed_mac[3], parsed_mac[4], parsed_mac[5]);
               
      int idx = find_device_index(norm_mac);
      if (idx == -1) {
        if (known_device_count < MAX_KNOWN_DEVICES) {
          idx = known_device_count++;
        } else {
          int oldest_idx = -1;
          uint32_t oldest_time = 0xFFFFFFFF;
          for (int i = 0; i < known_device_count; i++) {
            if (!known_devices[i].trusted && known_devices[i].lastSeen < oldest_time) {
              oldest_time = known_devices[i].lastSeen;
              oldest_idx = i;
            }
          }
          if (oldest_idx != -1) idx = oldest_idx;
        }
      }
      if (idx != -1) {
        strcpy(known_devices[idx].mac, norm_mac);
        strncpy(known_devices[idx].name, name_str.c_str(), sizeof(known_devices[idx].name));
        known_devices[idx].trusted = true;
        known_devices[idx].lastSeen = millis() / 1000;
        if (known_devices[idx].sightings == 0) {
          known_devices[idx].sightings = 1;
          known_devices[idx].firstSeen = millis() / 1000;
        }
        save_known_devices();
        _webserver->send(200, "text/plain", "OK");
        return;
      }
    }
  }
  _webserver->send(400, "text/plain", "Invalid arguments");
}

static void sec_handle_hg_wl_remove() {
  if (_webserver->hasArg("mac")) {
    String mac_str = _webserver->arg("mac");
    uint8_t parsed_mac[6] = {0};
    if (parse_mac_string(mac_str, parsed_mac)) {
      char norm_mac[18];
      snprintf(norm_mac, sizeof(norm_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
               parsed_mac[0], parsed_mac[1], parsed_mac[2],
               parsed_mac[3], parsed_mac[4], parsed_mac[5]);
      int idx = find_device_index(norm_mac);
      if (idx != -1) {
        for (int i = idx; i < known_device_count - 1; i++) {
          known_devices[i] = known_devices[i + 1];
        }
        known_device_count--;
        save_known_devices();
        _webserver->send(200, "text/plain", "OK");
        return;
      }
    }
  }
  _webserver->send(400, "text/plain", "Invalid arguments");
}

static void sec_handle_hg_find_toggle() {
  if (_webserver->hasArg("ssid")) {
    String ssid_str = _webserver->arg("ssid");
    if (ssid_str.length() > 0) {
      strncpy(find_ssid, ssid_str.c_str(), sizeof(find_ssid));
      find_mode_active = true;
      find_rssi = -100;
      ch_hopping = true;
    } else {
      find_mode_active = false;
      find_ssid[0] = '\0';
    }
    _webserver->send(200, "text/plain", "OK");
  } else {
    _webserver->send(400, "text/plain", "Missing ssid");
  }
}

static void sec_handle_scan_start() {
  Serial.printf("[SEC] WEB REQ client=%s uri=%s\n",
    _webserver->client().remoteIP().toString().c_str(), _webserver->uri().c_str());
  if (!sec_scanning) {
    sec_scanning = true;
    WiFiService.setPromiscuous(true, sec_sniffer_cb);
  }
  _webserver->send(200, "text/plain", "OK");
}

static void sec_handle_scan_stop() {
  Serial.printf("[SEC] WEB REQ client=%s uri=%s\n",
    _webserver->client().remoteIP().toString().c_str(), _webserver->uri().c_str());
  if (sec_scanning) {
    sec_scanning = false;
    WiFiService.setPromiscuous(false);
  }
  _webserver->send(200, "text/plain", "OK");
}

static void sec_draw_display() {
  if (!_stft) return;

  if (welcome_triggered) {
    if (millis() - welcome_triggered_ms < 10000) {
      _stft->fillScreen(0x000F); // Sleek dark blue
      _stft->drawRect(5, 5, TFT_W - 10, TFT_H - 10, 0x07FF); // Cyan border
      _stft->setTextColor(TFT_YELLOW);
      _stft->setTextSize(2);
      _stft->setCursor(20, 40);
      _stft->print("WELCOME");
      _stft->setCursor(20, 65);
      _stft->print("HOME,");
      
      _stft->setTextColor(TFT_WHITE);
      _stft->setCursor(20, 105);
      _stft->print(welcome_name);
      
      _stft->setTextSize(1);
      _stft->setTextColor(TFT_GREEN);
      _stft->setCursor(50, 180);
      _stft->print("(^__^)");
      return;
    } else {
      welcome_triggered = false;
    }
  }

  if (find_mode_active) {
    _stft->fillScreen(TFT_BLACK);
    _stft->drawRect(5, 5, TFT_W - 10, TFT_H - 10, 0x07E0); // Green border
    _stft->setTextColor(TFT_GREEN);
    _stft->setTextSize(1);
    _stft->setCursor(15, 20);
    _stft->print("FINDING NETWORK:");
    _stft->setCursor(15, 35);
    _stft->setTextColor(TFT_WHITE);
    _stft->print(find_ssid);
    
    _stft->setCursor(15, 70);
    _stft->setTextSize(3);
    if (find_rssi > -100) {
      _stft->setTextColor(find_rssi > -60 ? TFT_GREEN : (find_rssi > -80 ? TFT_YELLOW : TFT_RED));
      _stft->printf("%d dBm", find_rssi);
    } else {
      _stft->setTextColor(TFT_DARKGREY);
      _stft->print("SEARCHING");
    }
    
    int bar_val = map(find_rssi, -100, -30, 0, TFT_W - 40);
    bar_val = constrain(bar_val, 0, TFT_W - 40);
    _stft->fillRect(20, 120, TFT_W - 40, 20, 0x000F);
    _stft->fillRect(20, 120, bar_val, 20, 0x07E0);
    
    _stft->setTextSize(1);
    _stft->setTextColor(0x528A);
    _stft->setCursor(15, 170);
    _stft->print("Move the AeroSniffer");
    _stft->setCursor(15, 185);
    _stft->print("to locate dead zones.");
    
    _stft->setCursor(15, 215);
    _stft->setTextColor(TFT_YELLOW);
    _stft->print("Exit via companion app");
    return;
  }

  _stft->fillScreen(TFT_BLACK);

  _stft->fillRect(0, 0, TFT_W, 14, evil_twin_detected ? TFT_RED : 0x000F);
  _stft->setTextColor(evil_twin_detected ? TFT_WHITE : 0x07FF);
  _stft->setTextSize(1);
  _stft->setCursor(2, 3);
  _stft->print(evil_twin_detected ? "!! ROGUE AP DETECTED !!" : "MODE 2: SECURITY MONITOR");

  int rcx = TFT_W / 2, rcy = 90, rr = 54;
  _stft->fillCircle(rcx, rcy, rr, 0x0841);
  _stft->drawCircle(rcx, rcy, rr, 0x0340);
  _stft->drawCircle(rcx, rcy, rr / 2, 0x0220);
  _stft->drawFastHLine(rcx - rr, rcy, rr * 2, 0x0220);
  _stft->drawFastVLine(rcx, rcy - rr, rr * 2, 0x0220);

  float rad = _sweep_angle * DEG_TO_RAD;
  int sx = rcx + (int)((rr - 2) * cosf(rad));
  int sy = rcy + (int)((rr - 2) * sinf(rad));
  _stft->drawLine(rcx, rcy, sx, sy, 0x07E0);
  _stft->fillCircle(rcx, rcy, 3, TFT_WHITE);

  uint16_t badge_col = sec_scanning ? 0x07E0 : 0x4208;
  _stft->fillRoundRect(TFT_W - 68, 18, 64, 12, 3, badge_col);
  _stft->setTextColor(TFT_BLACK);
  _stft->setTextSize(1);
  _stft->setCursor(TFT_W - 62, 20);
  _stft->print(sec_scanning ? "SCANNING" : "  IDLE  ");

  int sy_base = 154;
  _stft->setTextColor(0x528A);
  _stft->setTextSize(1);

  _stft->setCursor(4, sy_base);
  _stft->print("PKT/s:");
  int bar_w = std::min<int>((int)(pkt_per_sec / 5), TFT_W - 60);
  _stft->fillRect(48, sy_base, bar_w, 8, 0x07E0);
  _stft->setTextColor(TFT_YELLOW);
  _stft->setCursor(TFT_W - 42, sy_base);
  _stft->printf("%4lu", pkt_per_sec);

  _stft->setTextColor(0x07FF);
  _stft->setTextSize(1);
  _stft->setCursor(4, sy_base + 16);
  _stft->printf("TOTAL: %lu", pkt_total);

  _stft->setCursor(4, sy_base + 28);
  _stft->printf("BCN: %lu  PRB: %lu", pkt_beacon, pkt_probe);

  _stft->setCursor(4, sy_base + 40);
  _stft->setTextColor(pkt_deauth > 0 ? TFT_RED : 0x07FF);
  _stft->printf("DEAUTH: %lu", pkt_deauth);
  if (pkt_deauth >= getDeauthThreshold()) {
    _stft->setTextColor(TFT_RED);
    _stft->print("  !! ALERT");
  }

  _stft->setTextColor(0x528A);
  _stft->setCursor(4, sy_base + 52);
  _stft->printf("CH: %d", current_ch);

  _stft->fillRect(0, TFT_H - 14, TFT_W, 14, 0x0008);
  _stft->setTextColor(0x2CA0);
  _stft->setTextSize(1);
  _stft->setCursor(4, TFT_H - 12);
  _stft->print("http://192.168.4.1");
}

static void sec_stream_events() {
  if (_deauth_evt_pending) {
    _deauth_evt_pending = false;
    Serial.printf("EVT:{\"type\":\"deauth_alert\",\"count\":%lu,\"threshold\":%d}\n",
                  pkt_deauth, getDeauthThreshold());
  }

  if (welcome_triggered) {
    welcome_triggered = false;
    Serial.printf("EVT:{\"type\":\"welcome_hello\",\"name\":\"%s\"}\n", welcome_name);
    if (_stft) {
      _stft->setTextColor(TFT_GREEN);
      _stft->setTextSize(1);
      _stft->setCursor(4, 4);
      char buf[48];
      snprintf(buf, sizeof(buf), "\xF0\x9F\x91\x8B WELCOME HOME: %s!", welcome_name);
      _stft->print(buf);
    }
  }
}

import { useState, useEffect, useRef } from "react";
import { serialAPI } from "../lib/serial";

export function LiveRadarPanel() {
  const [connected, setConnected] = useState(false);
  const [deviceInfo, setDeviceInfo] = useState<any>(null);
  const [status, setStatus] = useState({
    scanning: false,
    ch: 1,
    pps: 0,
    total: 0,
    beacons: 0,
    probes: 0,
    deauths: 0,
    hopping: true,
    aps: 0,
  });
  const [events, setEvents] = useState<any[]>([]);
  const [apList, setApList] = useState<any[]>([]);
  const [errorMsg, setErrorMsg] = useState("");
  const [bbox, setBbox] = useState({ lamin: "", lomin: "", lamax: "", lomax: "" });
  const [showSettings, setShowSettings] = useState(false);
  const [wifi, setWifi] = useState({ ssid: "", pass: "" });

  const eventsEndRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    eventsEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [events]);

  useEffect(() => {
    serialAPI.onMessage = (type: string, data: any) => {
      if (type === "RES") {
        if (data.action === "scan_start") setStatus((s) => ({ ...s, scanning: true }));
        if (data.action === "scan_stop") setStatus((s) => ({ ...s, scanning: false }));
        if (data.pps !== undefined) {
          setStatus(data);
        }
        if (data.fw || data.lamin !== undefined) {
          setDeviceInfo(data);
          if (data.lamin !== undefined && !bbox.lamin) {
            setBbox({
              lamin: data.lamin.toString(),
              lomin: data.lomin.toString(),
              lamax: data.lamax.toString(),
              lomax: data.lomax.toString(),
            });
          }
          if (data.ssid && data.ssid !== "YOUR_WIFI_SSID") {
            setWifi(w => ({ ...w, ssid: data.ssid }));
          }
          if (data.ssid === "YOUR_WIFI_SSID") {
            setShowSettings(true);
          }
        }
        if (data.aps && Array.isArray(data.aps)) {
          setApList(data.aps);
        }
      } else if (type === "EVT") {
        setEvents((prev) => [...prev.slice(-49), { time: new Date().toLocaleTimeString(), ...data }]);
      }
    };

    serialAPI.onDisconnect = () => {
      setConnected(false);
      setDeviceInfo(null);
    };

    const pollInterval = setInterval(() => {
      if (connected && status.scanning) {
        serialAPI.sendCommand("STATUS");
      }
    }, 1000);

    const apInterval = setInterval(() => {
      if (connected && status.scanning) {
        serialAPI.sendCommand("GET_APS");
      }
    }, 5000);

    return () => {
      clearInterval(pollInterval);
      clearInterval(apInterval);
      serialAPI.onMessage = null;
      serialAPI.onDisconnect = null;
    };
  }, [connected, status.scanning]);

  const connectDevice = async () => {
    try {
      setErrorMsg("");
      await serialAPI.connect();
      setConnected(true);
      await serialAPI.sendCommand("PING");
      setTimeout(() => serialAPI.sendCommand("STATUS"), 500);
      setTimeout(() => serialAPI.sendCommand("GET_CFG"), 1000);
    } catch (e: any) {
      setErrorMsg(e.message || "Failed to connect.");
    }
  };

  const disconnectDevice = async () => {
    await serialAPI.disconnect();
    setConnected(false);
  };

  const startScan = () => serialAPI.sendCommand("SCAN_START");
  const stopScan = () => serialAPI.sendCommand("SCAN_STOP");
  const toggleHop = () => serialAPI.sendCommand(status.hopping ? "HOP_OFF" : "HOP_ON");

  const handleAutoLocate = () => {
    if (navigator.geolocation) {
      navigator.geolocation.getCurrentPosition((pos) => {
        const lat = pos.coords.latitude;
        const lon = pos.coords.longitude;
        setBbox({
          lamin: (lat - 0.6).toFixed(2),
          lamax: (lat + 0.6).toFixed(2),
          lomin: (lon - 0.9).toFixed(2),
          lomax: (lon + 0.9).toFixed(2)
        });
      }, () => alert("Location access denied or failed."));
    } else {
      alert("Geolocation is not supported by this browser.");
    }
  };

  const saveSettings = () => {
    if (wifi.ssid) {
      serialAPI.sendCommand(`SET_WIFI:${wifi.ssid}:${wifi.pass}`);
    }
    serialAPI.sendCommand(`SET_BBOX:${bbox.lamin}:${bbox.lomin}:${bbox.lamax}:${bbox.lomax}`);
    setTimeout(() => serialAPI.sendCommand("GET_CFG"), 200);
    setShowSettings(false);
  };

  const rebootDevice = () => {
    serialAPI.sendCommand("REBOOT");
    disconnectDevice();
  };

  if (!connected) {
    return (
      <div className="max-w-6xl mx-auto mt-10 pixel-card p-6 flex flex-col items-center justify-center min-h-[300px]">
        <div className="font-pixel text-[10px] text-[color:var(--as-orange)] mb-4">
          ▲ MODE 2 · NETWORK AUDITOR · disconnected
        </div>
        <p className="font-mono-pixel text-[color:var(--as-neon)]/70 mb-6">
          Plug your AeroSniffer into a USB port and switch to Mode 2 to connect.
        </p>
        <button onClick={connectDevice} className="pixel-btn">
          CONNECT VIA WEB SERIAL
        </button>
        {errorMsg && <p className="mt-4 font-mono-pixel text-[color:var(--as-pink)]">{errorMsg}</p>}
      </div>
    );
  }

  return (
    <div className="max-w-6xl mx-auto mt-10 pixel-card p-6">
      <div className="flex flex-wrap items-center justify-between mb-6 border-b border-[color:var(--as-neon)]/20 pb-4">
        <div className="font-pixel text-[10px] text-[color:var(--as-orange)]">
          ▲ MODE 2 · NETWORK AUDITOR · <span className="text-[color:var(--as-neon)]">LIVE</span>
        </div>
        <div className="flex items-center gap-4">
          <button onClick={() => setShowSettings(true)} className="font-pixel text-[12px] text-[color:var(--as-yellow)] hover:scale-110 transition-transform">
            ⚙️ SETTINGS
          </button>
          <button onClick={disconnectDevice} className="font-pixel text-[10px] text-[color:var(--as-pink)] hover:underline">
            DISCONNECT
          </button>
        </div>
      </div>

      {showSettings && (
        <div className="fixed inset-0 z-50 bg-black/80 flex items-center justify-center p-4">
          <div className="bg-[#06080e] border-2 border-[color:var(--as-neon)] p-6 max-w-md w-full pixel-card">
            <div className="font-pixel text-lg text-[color:var(--as-neon)] mb-6 text-center">
              AEROSNIFFER SETUP
            </div>
            
            <div className="space-y-4 mb-6">
              <div>
                <label className="block font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-2">Wi-Fi SSID</label>
                <input type="text" value={wifi.ssid} onChange={e => setWifi(w => ({...w, ssid: e.target.value}))} className="w-full bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)]" placeholder="Network Name" />
              </div>
              <div>
                <label className="block font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-2">Wi-Fi Password</label>
                <input type="password" value={wifi.pass} onChange={e => setWifi(w => ({...w, pass: e.target.value}))} className="w-full bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)]" placeholder="Password" />
              </div>
            </div>

            <div className="border-t border-[color:var(--as-neon)]/20 pt-4 mb-6">
              <div className="flex justify-between items-end mb-2">
                <label className="block font-pixel text-[10px] text-[color:var(--as-yellow)]/70">Mode 3 Radar Bounds</label>
                <button onClick={handleAutoLocate} className="text-[10px] font-pixel text-[color:var(--as-yellow)] hover:underline border border-[color:var(--as-yellow)] px-2 py-1">
                  [ AUTO DETECT ]
                </button>
              </div>
              <div className="grid grid-cols-2 gap-2 mt-3">
                <input type="number" step="0.1" value={bbox.lamin} onChange={e => setBbox(b => ({...b, lamin: e.target.value}))} placeholder="Min Lat" className="bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)]" />
                <input type="number" step="0.1" value={bbox.lamax} onChange={e => setBbox(b => ({...b, lamax: e.target.value}))} placeholder="Max Lat" className="bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)]" />
                <input type="number" step="0.1" value={bbox.lomin} onChange={e => setBbox(b => ({...b, lomin: e.target.value}))} placeholder="Min Lon" className="bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)]" />
                <input type="number" step="0.1" value={bbox.lomax} onChange={e => setBbox(b => ({...b, lomax: e.target.value}))} placeholder="Max Lon" className="bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)]" />
              </div>
            </div>

            <div className="flex gap-3">
              <button onClick={saveSettings} className="pixel-btn flex-1 py-2 text-xs">
                SAVE CONFIG
              </button>
              {deviceInfo?.ssid !== "YOUR_WIFI_SSID" && (
                <button onClick={() => setShowSettings(false)} className="pixel-btn pixel-btn-ghost py-2 text-xs px-4 border-[color:var(--as-neon)]/50 text-[color:var(--as-neon)]/50">
                  CANCEL
                </button>
              )}
            </div>
            {deviceInfo?.ssid !== "YOUR_WIFI_SSID" && (
              <div className="mt-4 pt-4 border-t border-red-500/20 text-center">
                <button onClick={rebootDevice} className="font-pixel text-[10px] text-red-500 hover:underline">
                  [ RESTART ROBOT ]
                </button>
              </div>
            )}
          </div>
        </div>
      )}

      <div className="grid md:grid-cols-2 gap-8 items-start">
        {/* Radar Visual / Stats */}
        <div className="space-y-6">
          <div className="relative aspect-square max-w-[280px] mx-auto mb-6">
            <div className="absolute inset-0 rounded-full border-2 border-[color:var(--as-neon)]/40" />
            <div className="absolute inset-6 rounded-full border border-[color:var(--as-neon)]/30" />
            <div className="absolute inset-12 rounded-full border border-[color:var(--as-neon)]/20" />
            {status.scanning && (
              <div className="absolute inset-0 radar-sweep">
                <div
                  className="absolute top-1/2 left-1/2 h-1/2 w-1/2 origin-top-left"
                  style={{
                    background: "conic-gradient(from 0deg, rgba(74,240,188,0.55), transparent 30%)",
                  }}
                />
              </div>
            )}
          </div>

          <div className="flex gap-2 justify-center">
            <button onClick={startScan} disabled={status.scanning} className="pixel-btn px-4 py-2 text-xs">START</button>
            <button onClick={stopScan} disabled={!status.scanning} className="pixel-btn px-4 py-2 text-xs" style={{ borderColor: 'var(--as-pink)', color: 'var(--as-pink)' }}>STOP</button>
            <button onClick={toggleHop} disabled={!status.scanning} className="pixel-btn pixel-btn-ghost px-4 py-2 text-xs">
              {status.hopping ? "LOCK CH" : "HOP"}
            </button>
          </div>

          <div className="grid grid-cols-2 gap-4 font-mono-pixel text-[color:var(--as-neon)]/80 text-sm">
            <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/20">
              <div className="text-[color:var(--as-neon)]/50 text-xs mb-1">CH</div>
              <div>{status.hopping ? "HOP" : status.ch}</div>
            </div>
            <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/20">
              <div className="text-[color:var(--as-neon)]/50 text-xs mb-1">PKT/S</div>
              <div className={status.pps > 100 ? "text-[color:var(--as-violet)]" : ""}>{status.pps}</div>
            </div>
            <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/20">
              <div className="text-[color:var(--as-neon)]/50 text-xs mb-1">DEAUTHS</div>
              <div className={status.deauths > 0 ? "text-[color:var(--as-pink)] alert-pulse" : ""}>{status.deauths}</div>
            </div>
            <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/20">
              <div className="text-[color:var(--as-neon)]/50 text-xs mb-1">APS</div>
              <div>{status.aps}</div>
            </div>
          </div>
        </div>

        {/* Feeds */}
        <div className="space-y-6 h-full flex flex-col">
          <div className="flex-1 bg-[#06080e] border border-[color:var(--as-neon)]/20 p-4 font-mono-pixel overflow-hidden flex flex-col h-[250px]">
            <div className="text-[color:var(--as-neon)]/50 text-xs mb-3">LATEST ACCESS POINTS</div>
            <div className="overflow-y-auto flex-1 space-y-2 text-sm text-[color:var(--as-neon)]/80 pr-2 custom-scrollbar">
              {apList.slice(0, 8).map((ap, i) => (
                <div key={i} className="flex justify-between border-b border-[color:var(--as-neon)]/10 pb-1">
                  <span className="truncate pr-2">{ap.ssid || "<hidden>"}</span>
                  <span className={ap.rssi > -60 ? "text-[color:var(--as-neon)]" : "text-[color:var(--as-orange)]"}>{ap.rssi}</span>
                </div>
              ))}
            </div>
          </div>

          <div className="flex-1 bg-[#06080e] border border-[color:var(--as-neon)]/20 p-4 font-mono-pixel overflow-hidden flex flex-col h-[250px]">
            <div className="text-[color:var(--as-neon)]/50 text-xs mb-3">EVENT LOG</div>
            <div className="overflow-y-auto flex-1 space-y-1 text-sm pr-2 custom-scrollbar">
              {events.map((evt, i) => (
                <div key={i} className="border-b border-[color:var(--as-neon)]/10 pb-1 break-words">
                  <span className="text-[color:var(--as-neon)]/40">[{evt.time}] </span>
                  {evt.type === "deauth_alert" ? (
                    <span className="text-[color:var(--as-pink)]">⚠️ DEAUTH BURST</span>
                  ) : (
                    <span className="text-[color:var(--as-neon)]/70">{JSON.stringify(evt)}</span>
                  )}
                </div>
              ))}
              <div ref={eventsEndRef} />
            </div>
          </div>
        </div>
      </div>

      {/* Configuration Section is now inside the Settings Modal */}
    </div>
  );
}

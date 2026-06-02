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
        if (data.fw) {
          setDeviceInfo(data);
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
        <button onClick={disconnectDevice} className="font-pixel text-[10px] text-[color:var(--as-pink)] hover:underline">
          DISCONNECT
        </button>
      </div>

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
    </div>
  );
}

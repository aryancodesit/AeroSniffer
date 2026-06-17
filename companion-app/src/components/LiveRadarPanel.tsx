/* eslint-disable @typescript-eslint/no-explicit-any */
/* eslint-disable react-hooks/exhaustive-deps */
import { useState, useEffect, useRef } from "react";
import { serialAPI } from "../lib/serial";

export function LiveRadarPanel() {
  const [connected, setConnected] = useState(serialAPI.port !== null);
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

  const [welcomeMac, setWelcomeMac] = useState("");
  const [welcomeName, setWelcomeName] = useState("");
  const [findSsid, setFindSsid] = useState("");
  const [findActive, setFindActive] = useState(false);
  const [findRssi, setFindRssi] = useState(-100);
  const [whitelist, setWhitelist] = useState<any[]>([]);

  const [wlMac, setWlMac] = useState("");
  const [wlName, setWlName] = useState("");
  const [secPanelTab, setSecPanelTab] = useState<"trusted" | "discovered">("trusted");
  const [suspiciousMacs, setSuspiciousMacs] = useState<string[]>([]);
  const [validationError, setValidationError] = useState("");

  const eventsEndRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (connected) {
      serialAPI.sendCommand("PING");
      setTimeout(() => serialAPI.sendCommand("STATUS"), 500);
      setTimeout(() => serialAPI.sendCommand("GET_CFG"), 1000);
      setTimeout(() => serialAPI.sendCommand("GET_HG_CFG"), 1500);
    }
  }, [connected]);

  useEffect(() => {
    eventsEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [events]);

  useEffect(() => {
    setConnected(serialAPI.port !== null);

    const removeListener = serialAPI.addListener((type: string, data: any) => {
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
        }
        if (data.aps && Array.isArray(data.aps)) {
          setApList(data.aps);
        }
        if (data.welcome_mac !== undefined) {
          setWelcomeMac(data.welcome_mac);
          setWelcomeName(data.welcome_name);
          setFindSsid(data.find_ssid);
          setFindActive(data.find_active);
          setFindRssi(data.find_rssi);
          if (Array.isArray(data.whitelist)) {
            setWhitelist(data.whitelist);
          }
        }
      } else if (type === "EVT") {
        if (data.mac) {
          if (data.classification === "SUSPICIOUS" || data.type === "deauth_alert" || data.type === "intruder_alert") {
            setSuspiciousMacs((prev) => [...new Set([...prev, data.mac.toUpperCase()])]);
          }
        }
        setEvents((prev) => [
          ...prev.slice(-49),
          { time: new Date().toLocaleTimeString(), ...data },
        ]);
      }
    });

    const removeConnect = serialAPI.addConnectListener(() => {
      setConnected(true);
    });

    const removeDisconnect = serialAPI.addDisconnectListener(() => {
      setConnected(false);
      setDeviceInfo(null);
    });

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

    const hgInterval = setInterval(() => {
      if (connected) {
        serialAPI.sendCommand("GET_HG_CFG");
      }
    }, 3000);

    return () => {
      clearInterval(pollInterval);
      clearInterval(apInterval);
      clearInterval(hgInterval);
      removeListener();
      removeConnect();
      removeDisconnect();
    };
  }, [connected, status.scanning]);

  // Simulation effect for Demo Mode (when not connected)
  useEffect(() => {
    if (connected) return;

    setWhitelist([
      { mac: "4A:F2:C3:99:A1:02", name: "Aryan's iPhone", sightings: 48, lastSeen: 120, trusted: true },
      { mac: "8C:E2:B3:77:E1:05", name: "Development Laptop", sightings: 36, lastSeen: 180, trusted: true },
      { mac: "00:1A:2B:3C:4D:5E", name: "Smart TV", sightings: 15, lastSeen: 300, trusted: true },
      { mac: "A4:5E:60:88:B2:C1", name: "Unknown Phone", sightings: 3, lastSeen: 10, trusted: false },
      { mac: "F2:C3:04:88:99:AA", name: "Intruder Client", sightings: 1, lastSeen: 2, trusted: false }
    ]);

    setApList([
      { ssid: "HomeNetwork_5G", rssi: -45 },
      { ssid: "Linksys_Guest", rssi: -72 },
      { ssid: "HP-Print-24", rssi: -78 },
      { ssid: "AeroSniffer-SEC", rssi: -30 },
      { ssid: "CoffeeShop_Free", rssi: -88 }
    ]);

    setStatus({
      scanning: true,
      ch: 1,
      pps: 140,
      total: 1500,
      beacons: 1200,
      probes: 250,
      deauths: 0,
      hopping: true,
      aps: 5
    });

    setEvents([
      { time: new Date().toLocaleTimeString(), type: "welcome_hello", name: "Aryan's iPhone" }
    ]);
  }, [connected]);

  // Periodic simulation loop
  useEffect(() => {
    if (connected || !status.scanning) return;

    const interval = setInterval(() => {
      setStatus((s) => {
        let nextCh = s.ch;
        if (s.hopping) {
          nextCh = (s.ch % 13) + 1;
        }
        const nextPps = 80 + Math.round(Math.random() * 240);
        const totalDelta = Math.round(nextPps * 1.5);
        const beaconsDelta = Math.round(totalDelta * 0.8);
        const probesDelta = totalDelta - beaconsDelta;

        // Occasional simulated threat event (10% chance)
        const rand = Math.random();
        if (rand < 0.08) {
          const deauthCount = 8 + Math.round(Math.random() * 12);
          setEvents((prev) => [
            ...prev.slice(-49),
            {
              time: new Date().toLocaleTimeString(),
              type: "deauth_alert",
              count: deauthCount
            }
          ]);
          return {
            ...s,
            ch: nextCh,
            pps: nextPps,
            total: s.total + totalDelta,
            beacons: s.beacons + beaconsDelta,
            probes: s.probes + probesDelta,
            deauths: s.deauths + 1
          };
        } else if (rand < 0.16) {
          const randomMacs = [
            "B4:18:D1:42:0E:C8",
            "9A:02:C4:F3:11:80",
            "7E:F2:A3:88:B1:0E"
          ];
          const chosenMac = randomMacs[Math.floor(Math.random() * randomMacs.length)];
          setEvents((prev) => [
            ...prev.slice(-49),
            {
              time: new Date().toLocaleTimeString(),
              type: "intruder_alert",
              mac: chosenMac
            }
          ]);
          setWhitelist((w) => {
            if (w.some((d) => d.mac === chosenMac)) return w;
            return [...w, { mac: chosenMac, name: "Unknown Client", sightings: 1, lastSeen: Date.now(), trusted: false }];
          });
        }

        return {
          ...s,
          ch: nextCh,
          pps: nextPps,
          total: s.total + totalDelta,
          beacons: s.beacons + beaconsDelta,
          probes: s.probes + probesDelta
        };
      });

      // Fluctuate RSSIs slightly for realism
      setApList((prev) =>
        prev.map((ap) => {
          const rssiDelta = Math.round((Math.random() - 0.5) * 6);
          const nextRssi = Math.max(-95, Math.min(-20, ap.rssi + rssiDelta));
          return { ...ap, rssi: nextRssi };
        })
      );
    }, 1500);

    return () => clearInterval(interval);
  }, [connected, status.scanning, status.hopping]);

  const connectDevice = async () => {
    try {
      setErrorMsg("");
      await serialAPI.connect();
      setConnected(true);
    } catch (e: any) {
      setErrorMsg(e.message || "Failed to connect.");
    }
  };

  const disconnectDevice = async () => {
    await serialAPI.disconnect();
    setConnected(false);
  };

  const startScan = () => {
    if (!connected) {
      setStatus((s) => ({ ...s, scanning: true }));
      return;
    }
    serialAPI.sendCommand("SCAN_START");
  };

  const stopScan = () => {
    if (!connected) {
      setStatus((s) => ({ ...s, scanning: false }));
      return;
    }
    serialAPI.sendCommand("SCAN_STOP");
  };

  const toggleHop = () => {
    if (!connected) {
      setStatus((s) => ({ ...s, hopping: !s.hopping }));
      return;
    }
    serialAPI.sendCommand(status.hopping ? "HOP_OFF" : "HOP_ON");
  };

  const formatMacInput = (value: string) => {
    let clean = value.toUpperCase().replace(/[^0-9A-F]/g, "");
    if (clean.length > 12) {
      clean = clean.slice(0, 12);
    }
    const parts = clean.match(/.{1,2}/g);
    return parts ? parts.join(":") : clean;
  };

  const saveWelcome = () => {
    if (welcomeMac && welcomeName) {
      const macRegex = /^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/;
      if (!macRegex.test(welcomeMac)) {
        alert("Invalid MAC format for Welcome Home Detector.\nExpected: AA:BB:CC:DD:EE:FF");
        return;
      }
      
      const nameRegex = /^[A-Za-z0-9 ]{3,20}$/;
      if (!nameRegex.test(welcomeName.trim())) {
        alert("Invalid name.\nExpected: 3-20 characters (letters, numbers, spaces).");
        return;
      }

      if (!connected) {
        setEvents((prev) => [
          ...prev,
          { time: new Date().toLocaleTimeString(), type: "welcome_hello", name: welcomeName.trim() }
        ]);
        alert(`Demo Mode: Welcome greeting simulated for ${welcomeName.trim()}!`);
        return;
      }

      serialAPI.sendCommand(`SET_WELCOME:${welcomeMac},${welcomeName.trim()}`);
    }
  };

  const toggleFinder = () => {
    if (!connected) {
      setFindActive(!findActive);
      if (!findActive) {
        setFindRssi(-40 - Math.round(Math.random() * 20));
      }
      return;
    }
    const target = findActive ? "" : findSsid;
    serialAPI.sendCommand(`FIND_TOGGLE:${target}`);
  };

  const getClassification = (dev: any) => {
    if (dev.trusted) return "TRUSTED";
    if (suspiciousMacs.includes(dev.mac.toUpperCase())) return "SUSPICIOUS";
    if (dev.sightings > 1) return "FAMILIAR";
    return "UNKNOWN";
  };

  const isRandomizedMac = (mac: string) => {
    const firstOctet = parseInt(mac.split(":")[0], 16);
    return !isNaN(firstOctet) && (firstOctet & 0x02) !== 0;
  };

  const formatSeenTime = (seenSec: number) => {
    if (!seenSec) return "never";
    const maxUptime = Math.max(...whitelist.map((d: any) => d.lastSeen || 0), 0);
    const diff = Math.max(0, maxUptime - seenSec);
    if (diff < 60) return `${diff}s ago`;
    return `${Math.floor(diff / 60)}m ago`;
  };

  const toggleSuspicious = (mac: string) => {
    const norm = mac.toUpperCase();
    setSuspiciousMacs((prev) =>
      prev.includes(norm) ? prev.filter((m) => m !== norm) : [...prev, norm]
    );
  };

  const addWhitelist = () => {
    setValidationError("");

    // 1. Normalize MAC
    let cleanMac = wlMac.toUpperCase().trim().replace(/[^0-9A-F]/g, "");
    if (cleanMac.length === 12) {
      cleanMac = cleanMac.match(/.{1,2}/g)?.join(":") || cleanMac;
    } else {
      cleanMac = wlMac.toUpperCase().trim();
    }

    // 2. Validate MAC Format
    const macRegex = /^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/;
    if (!macRegex.test(cleanMac)) {
      setValidationError("Invalid MAC format.\nExpected:\nAA:BB:CC:DD:EE:FF");
      return;
    }

    // 3. Sanitize & Validate Name
    const cleanName = wlName.trim();
    const nameRegex = /^[A-Za-z0-9 ]{3,20}$/;
    if (!nameRegex.test(cleanName)) {
      setValidationError("Invalid name format.\nExpected: 3-20 chars (letters, numbers, spaces).");
      return;
    }

    if (!connected) {
      setWhitelist((prev) => {
        if (prev.some((d) => d.mac === cleanMac)) return prev;
        return [...prev, { mac: cleanMac, name: cleanName, sightings: 1, lastSeen: Date.now(), trusted: true }];
      });
      setWlMac("");
      setWlName("");
      return;
    }

    serialAPI.sendCommand(`WL_ADD:${cleanMac},${cleanName}`);
    setWlMac("");
    setWlName("");
    setTimeout(() => serialAPI.sendCommand("GET_HG_CFG"), 250);
  };

  const removeWhitelist = (mac: string) => {
    if (!connected) {
      setWhitelist((prev) => prev.filter((d) => d.mac !== mac));
      return;
    }
    serialAPI.sendCommand(`WL_DEL:${mac}`);
    setTimeout(() => serialAPI.sendCommand("GET_HG_CFG"), 250);
  };

  return (
    <div className="max-w-6xl mx-auto mt-10 pixel-card p-6">
      {!connected && (
        <div className="pixel-card p-3 mb-6 border-[color:var(--as-orange)]/50 bg-[color:var(--as-orange)]/5 font-pixel text-[9px] text-[color:var(--as-orange)] flex flex-wrap items-center justify-between gap-3">
          <span>▲ DEMO MODE: RUNNING SIMULATED 802.11 SECURITY SENTINEL</span>
          <button
            onClick={connectDevice}
            className="border border-[color:var(--as-orange)] hover:bg-[color:var(--as-orange)] hover:text-black px-3 py-1 text-[8px] transition-colors font-pixel"
          >
            CONNECT HARDWARE USB
          </button>
        </div>
      )}
      <div className="flex flex-wrap items-center justify-between mb-6 border-b border-[color:var(--as-neon)]/20 pb-4">
        <div className="font-pixel text-[10px] text-[color:var(--as-orange)] flex items-center gap-3">
          <span>▲ MODE 2 · SECURITY SENTINEL · <span className={connected ? "text-[color:var(--as-neon)]" : "text-[color:var(--as-orange)]/70"}>{connected ? "LIVE" : "SIMULATED"}</span></span>
          <a
            href="http://192.168.4.1"
            target="_blank"
            rel="noopener noreferrer"
            className="border border-[color:var(--as-neon)]/40 hover:border-[color:var(--as-neon)] px-2 py-0.5 text-[8px] hover:text-[color:var(--as-neon)] transition-colors font-pixel"
          >
            OPEN LOCAL PORTAL
          </a>
        </div>
        {connected && (
          <button
            onClick={disconnectDevice}
            className="font-pixel text-[10px] text-[color:var(--as-pink)] hover:underline"
          >
            DISCONNECT
          </button>
        )}
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
            <button
              onClick={startScan}
              disabled={status.scanning}
              className={`pixel-btn px-4 py-2 text-xs ${status.scanning ? "opacity-30 cursor-not-allowed shadow-none" : ""}`}
            >
              START
            </button>
            <button
              onClick={stopScan}
              disabled={!status.scanning}
              className={`pixel-btn px-4 py-2 text-xs ${!status.scanning ? "opacity-30 cursor-not-allowed shadow-none" : ""}`}
              style={{
                backgroundColor: "transparent",
                border: "2px solid var(--as-pink)",
                color: "var(--as-pink)",
                boxShadow: !status.scanning ? "none" : "4px 4px 0 rgba(255, 79, 216, 0.25)"
              }}
            >
              STOP
            </button>
            <button
              onClick={toggleHop}
              disabled={!status.scanning}
              className={`pixel-btn pixel-btn-ghost px-4 py-2 text-xs ${!status.scanning ? "opacity-30 cursor-not-allowed shadow-none" : ""}`}
            >
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
              <div className={status.pps > 100 ? "text-[color:var(--as-violet)]" : ""}>
                {status.pps}
              </div>
            </div>
            <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/20">
              <div className="text-[color:var(--as-neon)]/50 text-xs mb-1">EVENTS</div>
              <div className={status.deauths > 0 ? "text-[color:var(--as-pink)] alert-pulse" : ""}>
                {status.deauths}
              </div>
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
                <div
                  key={i}
                  className="flex justify-between border-b border-[color:var(--as-neon)]/10 pb-1"
                >
                  <span className="truncate pr-2">{ap.ssid || "<hidden>"}</span>
                  <span
                    className={
                      ap.rssi > -60
                        ? "text-[color:var(--as-neon)]"
                        : "text-[color:var(--as-orange)]"
                    }
                  >
                    {ap.rssi}
                  </span>
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
                    <span className="text-[color:var(--as-pink)] font-bold">⚠️ NETWORK BURST: {evt.count} frames</span>
                  ) : evt.type === "intruder_alert" ? (
                    <span className="text-[color:var(--as-pink)] font-bold">🚨 ANOMALY DETECTED: Unknown device {evt.mac}</span>
                  ) : evt.type === "welcome_hello" ? (
                    <span className="text-[color:var(--as-neon)] font-bold">👋 WELCOME HOME: {evt.name} detected!</span>
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

      {/* HOME GUARD CONTROLS */}
      <div className="mt-8 border-t border-[color:var(--as-neon)]/20 pt-6">
        <h3 className="font-pixel text-[10px] text-[color:var(--as-orange)] mb-4">
          ▲ HOME GUARD DEFENSE SYSTEM
        </h3>
        
        <div className="grid md:grid-cols-3 gap-6">
          {/* Welcome Home */}
          <div className="bg-[#06080e] p-4 border border-[color:var(--as-neon)]/20">
            <h4 className="font-pixel text-xs text-[color:var(--as-neon)] mb-2">Welcome Home Detector</h4>
            <p className="font-mono-pixel text-[color:var(--as-neon)]/60 text-xs mb-4">
              Greets you on the TFT screen when your device MAC is detected.
            </p>
            <div className="space-y-2">
              <input
                type="text"
                value={welcomeMac}
                onChange={(e) => setWelcomeMac(formatMacInput(e.target.value))}
                placeholder="MAC (XX:XX:XX:XX:XX:XX)"
                className="w-full bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-xs"
              />
              <input
                type="text"
                value={welcomeName}
                onChange={(e) => setWelcomeName(e.target.value)}
                placeholder="Name (e.g. Aryan)"
                className="w-full bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-xs"
              />
              <button
                onClick={saveWelcome}
                className="w-full pixel-btn text-[9px] py-1.5"
              >
                SAVE WELCOME
              </button>
            </div>
          </div>

          {/* Hot & Cold Signal Finder */}
          <div className="bg-[#06080e] p-4 border border-[color:var(--as-neon)]/20">
            <h4 className="font-pixel text-xs text-[color:var(--as-neon)] mb-2">Signal dead-zone Finder</h4>
            <p className="font-mono-pixel text-[color:var(--as-neon)]/60 text-xs mb-4">
              Locks radio channel to track RSSI of a specific SSID.
            </p>
            <div className="space-y-2">
              <input
                type="text"
                value={findSsid}
                onChange={(e) => setFindSsid(e.target.value)}
                placeholder="SSID to track"
                className="w-full bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-xs"
              />
              <div className="flex gap-2">
                <button
                  onClick={toggleFinder}
                  className={`flex-1 pixel-btn text-[9px] py-1.5 ${findActive ? "bg-red-900 border-red-500 text-white" : ""}`}
                >
                  {findActive ? `STOP FINDER (${findRssi > -100 ? findRssi + ' dBm' : 'SEARCHING...'})` : "START FINDER"}
                </button>
              </div>
            </div>
          </div>

          {/* Device Trust Manager */}
          <div className="bg-[#06080e] p-4 border border-[color:var(--as-neon)]/20 flex flex-col h-[400px]">
            <div className="flex justify-between items-center mb-3 border-b border-[color:var(--as-neon)]/15 pb-2">
              <h4 className="font-pixel text-xs text-[color:var(--as-neon)]">Device Trust Manager</h4>
              <div className="flex gap-2">
                <button
                  onClick={() => setSecPanelTab("trusted")}
                  className={`font-pixel text-[9px] px-2 py-1 border ${secPanelTab === "trusted" ? "border-[color:var(--as-neon)] text-[color:var(--as-neon)]" : "border-transparent text-[color:var(--as-neon)]/40"}`}
                >
                  TRUSTED
                </button>
                <button
                  onClick={() => setSecPanelTab("discovered")}
                  className={`font-pixel text-[9px] px-2 py-1 border ${secPanelTab === "discovered" ? "border-[color:var(--as-neon)] text-[color:var(--as-neon)]" : "border-transparent text-[color:var(--as-neon)]/40"}`}
                >
                  DISCOVERED ({whitelist.filter((d: any) => !d.trusted).length})
                </button>
              </div>
            </div>

            {secPanelTab === "trusted" ? (
              <div className="flex flex-col flex-1 overflow-hidden">
                <p className="font-mono-pixel text-[color:var(--as-neon)]/60 text-xs mb-3">
                  Manually authorize trusted devices or view current whitelist.
                </p>
                <div className="space-y-2 mb-3">
                  <input
                    type="text"
                    value={wlMac}
                    onChange={(e) => setWlMac(formatMacInput(e.target.value))}
                    placeholder="MAC (XX:XX:XX:XX:XX:XX)"
                    className="w-full bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-xs"
                  />
                  <input
                    type="text"
                    value={wlName}
                    onChange={(e) => setWlName(e.target.value)}
                    placeholder="Name (e.g. iPad)"
                    className="w-full bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-xs"
                  />
                  {validationError && (
                    <div className="text-[color:var(--as-pink)] font-mono-pixel text-[10px] whitespace-pre-line leading-tight">
                      {validationError}
                    </div>
                  )}
                  <button
                    onClick={addWhitelist}
                    className="w-full pixel-btn text-[9px] py-1.5"
                  >
                    ADD TO TRUSTED
                  </button>
                </div>
                <div className="flex-1 overflow-y-auto space-y-1 custom-scrollbar text-xs">
                  {whitelist.filter((d: any) => d.trusted).length === 0 ? (
                    <div className="text-[color:var(--as-neon)]/40 text-center font-mono-pixel py-4">
                      No trusted devices.
                    </div>
                  ) : (
                    whitelist.filter((d: any) => d.trusted).map((dev: any) => (
                      <div key={dev.mac} className="flex flex-col border-b border-[color:var(--as-neon)]/10 py-2 font-mono-pixel text-[color:var(--as-neon)]/80">
                        <div className="flex justify-between items-center">
                          <span className="text-[color:var(--as-neon)] font-bold">{dev.name || "Unnamed"}</span>
                          <button
                            onClick={() => removeWhitelist(dev.mac)}
                            className="text-[color:var(--as-pink)] hover:underline text-[9px] ml-2"
                          >
                            [UNTRUST]
                          </button>
                        </div>
                        <div className="flex justify-between text-[10px] text-[color:var(--as-neon)]/50 mt-1">
                          <span>{dev.mac}</span>
                          <span>Seen: {dev.sightings}x</span>
                        </div>
                      </div>
                    ))
                  )}
                </div>
              </div>
            ) : (
              <div className="flex flex-col flex-1 overflow-hidden">
                <div className="flex justify-between items-center mb-3">
                  <p className="font-mono-pixel text-[color:var(--as-neon)]/60 text-[10px] leading-tight">
                    Devices active nearby. Name & trust them to whitelist.
                  </p>
                  <button
                    onClick={() => {
                      if (!status.scanning) {
                        serialAPI.sendCommand("SCAN_START");
                      }
                    }}
                    className={`font-pixel text-[8px] px-2 py-1 border border-[color:var(--as-orange)] text-[color:var(--as-orange)] ${status.scanning ? "opacity-50 cursor-not-allowed" : "hover:bg-[color:var(--as-orange)]/10"}`}
                    disabled={status.scanning}
                  >
                    {status.scanning ? "SCANNING..." : "SCAN DEVS"}
                  </button>
                </div>

                <div className="flex-1 overflow-y-auto space-y-2 custom-scrollbar text-xs">
                  {whitelist.filter((d: any) => !d.trusted).length === 0 ? (
                    <div className="text-[color:var(--as-neon)]/40 text-center font-mono-pixel py-8">
                      No nearby devices discovered yet. Enable Wi-Fi scan to listen.
                    </div>
                  ) : (
                    whitelist.filter((d: any) => !d.trusted).map((dev: any) => {
                      const classification = getClassification(dev);
                      const isRandom = isRandomizedMac(dev.mac);
                      return (
                        <div key={dev.mac} className="border border-[color:var(--as-neon)]/15 p-2 bg-black/40 flex flex-col space-y-2">
                          <div className="flex justify-between items-center">
                            <span className="font-mono-pixel font-bold text-[color:var(--as-orange)] text-[10px] truncate max-w-[120px]">
                              {dev.mac}
                            </span>
                            <div className="flex gap-1">
                              <span className={`text-[8px] px-1 py-0.5 border ${
                                classification === "SUSPICIOUS" ? "border-red-500 text-red-500 bg-red-950/20" :
                                classification === "FAMILIAR" ? "border-blue-400 text-blue-400 bg-blue-950/20" :
                                "border-yellow-400 text-yellow-400 bg-yellow-950/20"
                              } font-pixel`}>
                                {classification}
                              </span>
                              {isRandom && (
                                <span className="text-[8px] px-1 py-0.5 border border-purple-500 text-purple-400 bg-purple-950/20 font-pixel" title="Likely Phone / Randomized MAC">
                                  RANDOM
                                </span>
                              )}
                            </div>
                          </div>

                          {isRandom && (
                            <div className="text-[9px] text-purple-300/60 font-mono-pixel leading-none">
                              Android Private MAC / Likely Phone
                            </div>
                          )}

                          <div className="flex justify-between text-[10px] text-[color:var(--as-neon)]/50">
                            <span>Seen: {dev.sightings}x</span>
                            <span>Last: {formatSeenTime(dev.lastSeen)}</span>
                          </div>

                          <div className="flex gap-1">
                            <input
                              type="text"
                              placeholder="Assign Name"
                              defaultValue={dev.name}
                              onBlur={(e) => {
                                dev._typedName = e.target.value;
                              }}
                              className="flex-1 bg-black border border-[color:var(--as-neon)]/20 px-2 py-0.5 font-mono-pixel text-[10px] text-[color:var(--as-neon)]"
                            />
                            <button
                              onClick={() => {
                                const nameToSave = dev._typedName?.trim() || dev.name || (isRandom ? "Android Private MAC" : "Discovered");
                                
                                const nameRegex = /^[A-Za-z0-9 ]{3,20}$/;
                                if (!nameRegex.test(nameToSave)) {
                                  alert("Invalid name.\nExpected: 3-20 characters (letters, numbers, spaces).");
                                  return;
                                }
                                
                                serialAPI.sendCommand(`WL_ADD:${dev.mac},${nameToSave}`);
                                setTimeout(() => serialAPI.sendCommand("GET_HG_CFG"), 250);
                              }}
                              className="pixel-btn text-[8px] px-2 py-0.5"
                            >
                              TRUST
                            </button>
                            <button
                              onClick={() => toggleSuspicious(dev.mac)}
                              className={`border px-1.5 text-[8px] font-pixel ${classification === "SUSPICIOUS" ? "border-red-500 text-red-500 bg-red-950/30" : "border-red-500/30 text-red-500/60"}`}
                              title="Toggle Suspicious Flag"
                            >
                              ⚠️
                            </button>
                          </div>
                        </div>
                      );
                    })
                  )}
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}

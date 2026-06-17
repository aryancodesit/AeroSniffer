import { createFileRoute } from "@tanstack/react-router";
import { useEffect, useRef, useState } from "react";
import { BotFace, FACE_META, type FaceState } from "@/components/BotFace";
import { PixelBackdrop } from "@/components/PixelBackdrop";
import { LiveRadarPanel } from "@/components/LiveRadarPanel";
import { serialAPI } from "@/lib/serial";
import {
  ResponsiveContainer,
  AreaChart,
  Area,
  BarChart,
  Bar,
  XAxis,
  YAxis,
  Tooltip,
  Legend,
  LineChart,
  Line,
  CartesianGrid
} from "recharts";

export const Route = createFileRoute("/")({
  head: () => ({
    meta: [
      { title: "AeroSniffer — Pixel Companion" },
      {
        name: "description",
        content:
          "Meet AeroSniffer: a multi-boot ESP32-S3 desk gadget. Cyber-Pet, Security Sentinel, and Aviation Observer — in one tiny pixel-faced friend.",
      },
      { property: "og:title", content: "AeroSniffer — Pixel Companion" },
      {
        property: "og:description",
        content: "Three devices in one. A living desk companion with a pixel soul.",
      },
    ],
  }),
  component: Index,
});

function Index() {
  const [face, setFace] = useState<FaceState>("sys_boot");
  const [booted, setBooted] = useState(false);
  const [bootLines, setBootLines] = useState<string[]>([]);
  const [mode, setMode] = useState<0 | 1 | 2 | 3 | 4>(0);
  const [isConnected, setIsConnected] = useState(false);
  const heroRef = useRef<HTMLDivElement>(null);

  const [activeTab, setActiveTab] = useState<"companion" | "modes" | "faces">("companion");
  const [status, setStatus] = useState<any>({
    emotion: "idle",
    mood: "stable",
    activity: "idle",
    wifi: "disconnected",
    heap: 180000,
    fps: 30,
    mode: 0,
    flights: 0,
    networks: 0,
    coding: 0,
    hours: 0,
    fw: "1.0",
    hw: "detecting...",
  });
  const [heapHistory, setHeapHistory] = useState<{ time: string; heap: number }[]>([]);
  const [timeline, setTimeline] = useState<any[]>([]);
  const [lastUpdated, setLastUpdated] = useState<string>("");
  const [trustedCount, setTrustedCount] = useState<number>(0);
  const [aircraftCount, setAircraftCount] = useState<number>(0);
  const [eventCount, setEventCount] = useState<number>(0);
  const [aviationEnabled, setAviationEnabled] = useState<boolean>(true);
  const [devMode, setDevMode] = useState<boolean>(false);

  // Keep face synchronized with active mode and device status
  useEffect(() => {
    if (!booted || activeTab === "faces") return;

    if (isConnected) {
      if (status.face && status.face !== "") {
        setFace(status.face as FaceState);
      } else if (status.mode === 1) {
        // Security mode
        const isIntrusion = status.emotion === "sec_intrusion" || status.emotion === "intrusion" || status.activity === "intrusion";
        setFace(isIntrusion ? "sec_intrusion" : "sec_scanning");
      } else if (status.mode === 2) {
        // Aviation mode
        setFace(aviationEnabled ? "avi_radar" : "avi_disabled");
      } else {
        // Companion mode
        const rawEmo = String(status.emotion || "idle").toLowerCase();
        const emoMap: Record<string, FaceState> = {
          happy: "happy",
          sad: "sad",
          angry: "sad",
          curious: "thinking",
          sleepy: "sleepy",
          calm: "idle",
          alert: "sad",
          love: "love",
          surprised: "surprised",
          excited: "excited",
          
          sec_scanning: "sec_scanning",
          sec_intrusion: "sec_intrusion",
          avi_radar: "avi_radar",
          avi_lock: "avi_lock",
          avi_disabled: "avi_disabled",
          sys_boot: "sys_boot",
          sys_prefs: "sys_prefs",
          sys_error: "sys_error",
        };
        setFace(emoMap[rawEmo] || "idle");
      }
    } else {
      // Demo Mode
      if (mode === 1) {
        setFace("sec_scanning");
      } else if (mode === 2) {
        setFace(aviationEnabled ? "avi_radar" : "avi_disabled");
      } else {
        const rawEmo = String(status.emotion || "idle").toLowerCase();
        const emoMap: Record<string, FaceState> = {
          happy: "happy",
          sad: "sad",
          angry: "sad",
          curious: "thinking",
          sleepy: "sleepy",
          calm: "idle",
          alert: "sad",
          love: "love",
          surprised: "surprised",
          excited: "excited",
        };
        setFace(emoMap[rawEmo] || "idle");
      }
    }
  }, [isConnected, booted, activeTab, status.mode, status.emotion, status.activity, mode, aviationEnabled]);

  // Global status tracking when connected
  useEffect(() => {
    if (!isConnected) return;

    const removeListener = serialAPI.addListener((type: string, data: any) => {
      const nowStr = new Date().toLocaleTimeString();
      setLastUpdated(nowStr);
      if (type === "EVT") setEventCount((c) => c + 1);

      if (type === "RES") {
        if (data.fw) {
          setStatus((prev: any) => ({ ...prev, fw: data.fw, hw: data.hw || prev.hw }));
        }
        if (data.emotion !== undefined) {
          setStatus((prev: any) => ({ ...prev, ...data }));
          setHeapHistory((prev) => [
            ...prev.slice(-29),
            { time: nowStr.slice(-8), heap: data.heap / 1024 },
          ]);
        }
        if (data.flights !== undefined) {
          const count = Array.isArray(data.flights) ? data.flights.length : Number(data.flights);
          setAircraftCount(count);
        }
        if (data.whitelist !== undefined && Array.isArray(data.whitelist)) {
          setTrustedCount(data.whitelist.filter((d: any) => d.trusted).length);
        }
        if (data.timeline && Array.isArray(data.timeline)) {
          const events = data.timeline.map((evt: any) => {
            const sec = evt.time;
            const hrs = Math.floor(sec / 3600);
            const mins = Math.floor((sec % 3600) / 60);
            const secs = sec % 60;
            const timeStr = `${hrs.toString().padStart(2, '0')}:${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
            return {
              time: timeStr,
              text: evt.text,
              type: evt.type,
            };
          });
          setTimeline(events);
        }
      }
    });

    // Initial query
    serialAPI.sendCommand("GET_PET_STATUS");
    serialAPI.sendCommand("GET_TIMELINE");
    serialAPI.sendCommand("GET_HG_CFG");

    const petInterval = setInterval(() => {
      serialAPI.sendCommand("GET_PET_STATUS");
    }, 2000);

    const timelineInterval = setInterval(() => {
      serialAPI.sendCommand("GET_TIMELINE");
    }, 5000);

    const hgInterval = setInterval(() => {
      serialAPI.sendCommand("GET_HG_CFG");
    }, 5000);

    return () => {
      clearInterval(petInterval);
      clearInterval(timelineInterval);
      clearInterval(hgInterval);
      removeListener();
    };
  }, [isConnected, booted]);

  // Simulation loop when not connected (Demo Mode)
  useEffect(() => {
    if (isConnected || !booted) return;

    // Set initial values for demo mode
    setStatus({
      emotion: "idle",
      mood: "stable",
      activity: "idle",
      wifi: "AeroSniffer-SEC",
      heap: 184320,
      fps: 30,
      mode: mode,
      flights: mode === 2 ? 4 : 0,
      networks: mode === 1 ? 12 : 0,
      coding: 15,
      hours: 4,
      fw: "2.3-SENTINEL",
      hw: "DeskBuddy 2.0 (Simulated)",
    });

    const nowStr = new Date().toLocaleTimeString();
    setLastUpdated(nowStr);
    setTimeline([
      { time: nowStr, text: "AeroSniffer OS 2.3 simulated core boot OK", type: "system" },
      { time: nowStr, text: "Home Guard monitoring enabled", type: "activity" }
    ]);

    // Fill initial heap history
    const history = [];
    for (let i = 10; i >= 0; i--) {
      const d = new Date(Date.now() - i * 5000);
      history.push({
        time: d.toLocaleTimeString().slice(-8),
        heap: 180 + Math.round(Math.sin(i) * 3 + Math.random() * 2),
      });
    }
    setHeapHistory(history);

    const interval = setInterval(() => {
      const timeStr = new Date().toLocaleTimeString();
      setLastUpdated(timeStr);

      // Keep mode synchronized with current user tab selection
      setStatus((prev: any) => {
        const nextHeap = Math.round(180000 + Math.sin(Date.now() / 10000) * 3000 + Math.random() * 1000);
        const randomFlights = mode === 2 ? 4 + Math.round(Math.random() * 2) : 0;
        const randomNetworks = mode === 1 ? 12 + Math.round(Math.random() * 3) : 0;
        
        let act = prev.activity;
        let emo = prev.emotion;
        if (mode === 0) {
          const acts = ["coding", "typing", "thinking", "music"];
          act = acts[Math.floor(Math.random() * acts.length)];
          const emos = ["happy", "idle", "excited", "thinking", "love"];
          emo = emos[Math.floor(Math.random() * emos.length)];
        } else if (mode === 1) {
          act = "scanning";
          emo = "sec_scanning";
        } else if (mode === 2) {
          act = "tracking";
          emo = aviationEnabled ? "avi_radar" : "avi_disabled";
        }

        return {
          ...prev,
          mode: mode,
          heap: nextHeap,
          flights: randomFlights,
          networks: randomNetworks,
          emotion: emo,
          activity: act,
        };
      });

      setHeapHistory((prev) => [
        ...prev.slice(-29),
        { time: timeStr.slice(-8), heap: (180000 + Math.sin(Date.now() / 10000) * 3000 + Math.random() * 1000) / 1024 },
      ]);

      // Add simulated events to timeline periodically
      if (Math.random() > 0.4) {
        let text = "";
        let type = "";
        if (mode === 0) {
          const events = [
            "Companion bond state updated",
            "IDE scroll action detected (writing firmware)",
            "Pet smiled at user activity",
            "CPU load spike: pet became energized"
          ];
          text = events[Math.floor(Math.random() * events.length)];
          type = "emotion";
        } else if (mode === 1) {
          const events = [
            "Home Guard: Whitelisted device returned (iPad)",
            "⚠️ Alert: Unknown device detected on Channel 6",
            "Hopped channel: radio set to CH 11",
            "Hopped channel: radio set to CH 1"
          ];
          text = events[Math.floor(Math.random() * events.length)];
          type = "network";
        } else if (mode === 2) {
          const events = [
            "Aviation Target: AIC304 distance decreasing",
            "Aviation: Received ADS-B packet",
            "Aviation Target: IND912 bearing change",
            "Aviation airspace sweep: 4 targets tracked"
          ];
          text = events[Math.floor(Math.random() * events.length)];
          type = "flight";
        }

        setTimeline((prev) => [
          ...prev,
          { time: timeStr, text, type }
        ]);
        setEventCount((c) => c + 1);
      }
    }, 3000);

    return () => clearInterval(interval);
  }, [isConnected, booted, mode]);

  // Boot sequence → idle
  useEffect(() => {
    const lines = [
      "> XIAO_ESP32S3  boot   OK",
      "> FreeRTOS     init   OK",
      "> AeroSniffer OS 2.0  OK",
      "> loading pixel.soul …",
      "> hello, friend.",
    ];
    let i = 0;
    const t = setInterval(() => {
      setBootLines((p) => [...p, lines[i]]);
      i++;
      if (i >= lines.length) {
        clearInterval(t);
        setTimeout(() => {
          setFace("happy");
          setBooted(true);
          setTimeout(() => setFace("idle"), 1400);
        }, 600);
      }
    }, 380);
    return () => clearInterval(t);
  }, []);

  // React to mode changes
  useEffect(() => {
    if (!booted) return;
    setFace("excited");
    const t = setTimeout(() => setFace("idle"), 1200);
    return () => clearTimeout(t);
  }, [mode, booted]);

  return (
    <main className="pixelscape scanlines crt-flicker min-h-screen">
      <PixelBackdrop mode={mode} />
      <div className="relative z-10">
        <Nav
          onFace={(f) => booted && setFace(f)}
          isConnected={isConnected}
          setIsConnected={setIsConnected}
          activeTab={activeTab}
          setActiveTab={setActiveTab}
          aviationEnabled={aviationEnabled}
          setAviationEnabled={setAviationEnabled}
        />

        {/* HERO */}
        <section
          ref={heroRef}
          className="pixel-grid relative overflow-hidden border-b border-[color:var(--as-neon)]/20"
        >
          <div className="max-w-6xl mx-auto px-6 pt-10 pb-20 grid md:grid-cols-2 gap-10 items-center min-h-[78vh]">
            {/* Bot */}
            <div className="order-2 md:order-1 flex flex-col items-center md:items-start gap-6">
              <div
                className="pixel-border bg-[#06080e] p-6 float-y cursor-pointer select-none"
                onMouseEnter={() => booted && setFace("happy")}
                onMouseLeave={() => booted && setFace("idle")}
                onClick={() => {
                  if (!booted) return;
                  setFace("love");
                  setTimeout(() => setFace("idle"), 1600);
                }}
                title="say hi"
              >
                <BotFace state={face} size={420} followCursor={booted} />
                <div className="mt-3 flex items-center justify-between font-pixel text-[10px]">
                  <span className="neon-text">● {(FACE_META[face]?.label || "unknown").toUpperCase()}</span>
                  <span className="text-[color:var(--as-neon)]/60">240×240 · RGB565</span>
                </div>
              </div>

              <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70">
                hover · click · move your mouse — he's watching
              </div>
            </div>

            {/* Title + boot */}
            <div className="order-1 md:order-2">
              <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-3">
                ┌── AEROSNIFFER 2.0 ──┐
              </div>
              <h1 className="font-pixel neon-text text-3xl md:text-5xl leading-tight">
                AERO
                <br />
                SNIFFER
              </h1>
              <p className="font-mono-pixel mt-4 text-[color:var(--as-neon)]/80 text-xl md:text-2xl max-w-md">
                A multi-mode embedded intelligence platform.
              </p>
              <p className="font-mono-pixel mt-2 text-[color:var(--as-neon)]/60 text-sm max-w-md">
                Observe with the device. Act through the portal. Analyze through Mission Control.
              </p>

              <div className="mt-6 pixel-card p-4 font-mono-pixel text-base text-[color:var(--as-neon)] min-h-[180px]">
                {bootLines.map((l, i) => (
                  <div key={i}>{l}</div>
                ))}
                {!booted && <div className="caret" />}
                {booted && (
                  <div className="mt-2 text-[color:var(--as-neon)]/70">
                    &gt; ready. select a mode below ↓
                  </div>
                )}
              </div>

              <div className="mt-6 flex flex-wrap gap-3">
                <button onClick={() => setActiveTab("modes")} className="pixel-btn px-10">
                  CHOOSE A MODE
                </button>
              </div>
            </div>
          </div>

          {/* marquee */}
          <Marquee />
        </section>



        {/* TAB CONTENTS */}
        <div className="py-10">
          {activeTab === "companion" && (
            <div className="max-w-6xl mx-auto px-6 space-y-12">
              {devMode ? (
                <div className="space-y-10">
                  {/* Companion Status & Diagnostics Console */}
                  <div className="grid md:grid-cols-2 gap-6">
                    {/* Companion Status Panel */}
                    <div className="pixel-card p-6 border-[color:var(--as-pink)] flex flex-col justify-between">
                      <div>
                        <div className="font-pixel text-[10px] text-[color:var(--as-pink)] mb-4">
                          ▲ COMPANION STATUS
                        </div>
                        <div className="space-y-4 font-mono-pixel text-lg">
                          <div className="flex justify-between border-b border-[color:var(--as-neon)]/15 pb-2">
                            <span className="text-[color:var(--as-neon)]/60">Current Emotion</span>
                            <span className="font-pixel text-base text-[color:var(--as-pink)] uppercase glow-pink">
                              {isConnected ? status.emotion : "happy (demo)"}
                            </span>
                          </div>
                          <div className="flex justify-between border-b border-[color:var(--as-neon)]/15 pb-2">
                            <span className="text-[color:var(--as-neon)]/60">Current Mode</span>
                            <span className="text-[color:var(--as-yellow)] uppercase">
                              {mode === 0 ? "Cyber-Pet" : mode === 1 ? "Security Sentinel" : mode === 2 ? "Aviation Observer" : "Settings"}
                            </span>
                          </div>
                          <div className="flex justify-between border-b border-[color:var(--as-neon)]/15 pb-2">
                            <span className="text-[color:var(--as-neon)]/60">WiFi Status</span>
                            <span className="text-[color:var(--as-neon)] uppercase font-bold">
                              {isConnected ? (status.wifi || "CONNECTED").toUpperCase() : "DEMO_WIFI_ACTIVE"}
                            </span>
                          </div>
                          <div className="flex justify-between border-b border-[color:var(--as-neon)]/15 pb-2">
                            <span className="text-[color:var(--as-neon)]/60">Aircraft in Range</span>
                            <span className="text-[color:var(--as-yellow)] font-pixel text-base">
                              {isConnected ? aircraftCount : 4}
                            </span>
                          </div>
                          <div className="flex justify-between pb-2">
                            <span className="text-[color:var(--as-neon)]/60">Trusted Devices</span>
                            <span className="text-[color:var(--as-orange)] font-pixel text-base">
                              {isConnected ? trustedCount : 3}
                            </span>
                          </div>
                        </div>
                      </div>
                    </div>

                    {/* Console Panel */}
                    <div className="pixel-card p-6 border-[color:var(--as-violet)]">
                      <div className="font-pixel text-[10px] text-[color:var(--as-violet)] mb-4">
                        ▲ CONSOLE DIAGNOSTICS
                      </div>
                      <div className="space-y-4 font-mono-pixel text-lg">
                        <div className="flex justify-between border-b border-[color:var(--as-neon)]/15 pb-2">
                          <span className="text-[color:var(--as-neon)]/60">Serial Connected</span>
                          <span className="text-[color:var(--as-yellow)] uppercase font-bold">
                            {isConnected ? "YES" : "DEMO MODE ACTIVE"}
                          </span>
                        </div>
                        <div className="flex justify-between border-b border-[color:var(--as-neon)]/15 pb-2">
                          <span className="text-[color:var(--as-neon)]/60">Firmware Version</span>
                          <span className="text-[color:var(--as-yellow)] uppercase">
                            {isConnected ? (status.fw || "v2.3") : "v2.3-SENTINEL (DEMO)"}
                          </span>
                        </div>
                        <div className="flex justify-between border-b border-[color:var(--as-neon)]/15 pb-2">
                          <span className="text-[color:var(--as-neon)]/60">Current Mode</span>
                          <span className="text-[color:var(--as-orange)] uppercase font-bold">
                            {isConnected ? `MODE ${status.mode + 1}` : "DEMO RUNNING"}
                          </span>
                        </div>
                        <div className="flex justify-between border-b border-[color:var(--as-neon)]/15 pb-2">
                          <span className="text-[color:var(--as-neon)]/60">Events Received</span>
                          <span className="text-[color:var(--as-neon)]">
                            {isConnected ? eventCount : 12}
                          </span>
                        </div>
                        <div className="flex justify-between pb-2">
                          <span className="text-[color:var(--as-neon)]/60">Last Packet</span>
                          <span className="text-[color:var(--as-yellow)]">
                            {isConnected && lastUpdated ? lastUpdated : "real-time simulated"}
                          </span>
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Heap allocation & Timeline */}
                  {(isConnected || !isConnected) && (
                    <div className="grid md:grid-cols-2 gap-6">
                      {/* Heap Chart */}
                      <div className="pixel-card p-5">
                        <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-4">
                          ▲ REAL-TIME HEAP ALLOCATION MONITOR (KB)
                        </div>
                        <div className="h-[250px] w-full font-mono-pixel text-xs">
                          {heapHistory.length > 0 ? (
                            <ResponsiveContainer width="100%" height="100%">
                              <AreaChart data={heapHistory} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                                <defs>
                                  <linearGradient id="heapGrad" x1="0" y1="0" x2="0" y2="1">
                                    <stop offset="5%" stopColor="var(--as-neon)" stopOpacity={0.4} />
                                    <stop offset="95%" stopColor="var(--as-neon)" stopOpacity={0} />
                                  </linearGradient>
                                </defs>
                                <XAxis dataKey="time" stroke="var(--as-neon)" opacity={0.6} />
                                <YAxis
                                  stroke="var(--as-neon)"
                                  opacity={0.6}
                                  domain={["dataMin - 5", "dataMax + 5"]}
                                />
                                <Tooltip
                                  contentStyle={{
                                    backgroundColor: "#06080e",
                                    borderColor: "var(--as-neon)",
                                    color: "var(--as-neon)",
                                  }}
                                />
                                <CartesianGrid strokeDasharray="3 3" stroke="var(--as-neon)" opacity={0.1} />
                                <Area
                                  type="monotone"
                                  dataKey="heap"
                                  stroke="var(--as-neon)"
                                  fillOpacity={1}
                                  fill="url(#heapGrad)"
                                />
                              </AreaChart>
                            </ResponsiveContainer>
                          ) : (
                            <div className="h-full flex items-center justify-center text-[color:var(--as-neon)]/50">
                              Waiting for telemetry stream...
                            </div>
                          )}
                        </div>
                      </div>

                      {/* Timeline */}
                      <div className="pixel-card p-5 flex flex-col h-[350px]">
                        <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-4">
                          ▲ LOCAL COGNITIVE EVENT TIMELINE
                        </div>
                        <div className="overflow-y-auto flex-1 font-mono-pixel text-sm space-y-2 pr-2 custom-scrollbar">
                          {timeline.length === 0 ? (
                            <div className="text-[color:var(--as-neon)]/30 text-xs font-mono-pixel">
                              No events recorded today.
                            </div>
                          ) : (
                            timeline
                              .slice()
                              .reverse()
                              .map((evt, i) => (
                                <div key={i} className="border-b border-[color:var(--as-neon)]/10 pb-1">
                                  <span className="text-[color:var(--as-neon)]/40">[{evt.time}] </span>
                                  <span
                                    className={
                                      evt.type === "flight"
                                        ? "text-[color:var(--as-yellow)]"
                                        : evt.type === "network"
                                          ? "text-[color:var(--as-orange)]"
                                          : evt.type === "emotion"
                                            ? "text-[color:var(--as-pink)]"
                                            : evt.type === "activity"
                                              ? "text-[color:var(--as-violet)]"
                                              : "text-[color:var(--as-neon)]/80"
                                    }
                                  >
                                    {evt.text}
                                  </span>
                                </div>
                              ))
                          )}
                        </div>
                      </div>
                    </div>
                  )}
                </div>
              ) : (
                <div className="space-y-16">
                  {/* What Is AeroSniffer */}
                  <div className="pt-6">
                    <SectionHeader kicker="00 // INTRODUCTION" title="WHAT IS AEROSNIFFER?" />
                    <div className="mt-6 pixel-card p-8 border-[color:var(--as-neon)]/20 bg-gradient-to-br from-[#06080e] via-[#0a0d18] to-[#06080e]">
                      <div className="grid md:grid-cols-2 gap-8 items-center">
                        <div className="space-y-4 text-left">
                          <p className="font-mono-pixel text-sm text-[color:var(--as-neon)]/85 leading-relaxed">
                            AeroSniffer is a <span className="text-[color:var(--as-yellow)] font-bold">multi-mode embedded intelligence companion</span> built on the ESP32-S3.
                          </p>
                          <p className="font-mono-pixel text-sm text-[color:var(--as-neon)]/70 leading-relaxed">
                            Three distinct roles in a single portable device:
                          </p>
                          <div className="space-y-3 mt-4">
                            <div className="flex items-center gap-3">
                              <span className="font-pixel text-[9px] text-[color:var(--as-pink)] w-5">01</span>
                              <span className="font-mono-pixel text-sm text-[color:var(--as-neon)]">A desktop <span className="text-[color:var(--as-pink)] font-bold">Companion</span> that lives on your desk and reacts to you</span>
                            </div>
                            <div className="flex items-center gap-3">
                              <span className="font-pixel text-[9px] text-[color:var(--as-yellow)] w-5">02</span>
                              <span className="font-mono-pixel text-sm text-[color:var(--as-neon)]">A network <span className="text-[color:var(--as-yellow)] font-bold">Sentinel</span> that monitors local device presence</span>
                            </div>
                            <div className="flex items-center gap-3">
                              <span className="font-pixel text-[9px] text-[color:var(--as-violet)] w-5">03</span>
                              <span className="font-mono-pixel text-sm text-[color:var(--as-neon)]">An aviation <span className="text-[color:var(--as-violet)] font-bold">Observer</span> that tracks aircraft overhead</span>
                            </div>
                          </div>
                        </div>
                        <div className="flex justify-center items-center">
                          <BotFace state="happy" size={200} />
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Three Souls, One Shell */}
                  <div>
                    <SectionHeader kicker="01 // EMBEDDED SYSTEM" title="THREE SOULS, ONE SHELL" />
                    <div className="grid md:grid-cols-3 gap-6 mt-8">
                      <div className="pixel-card p-6 border-[color:var(--as-pink)] bg-[#06080e]/50 flex flex-col justify-between">
                        <div>
                          <div className="flex justify-between items-start mb-3">
                            <span className="font-pixel text-xs text-[color:var(--as-pink)]">MODE 01 // COMPANION</span>
                            <span className="font-mono-pixel text-[10px] text-[color:var(--as-neon)]/50">[ACTIVE]</span>
                          </div>
                          <h3 className="font-pixel text-base neon-text mb-2">Cyber-Pet</h3>
                          <p className="font-mono-pixel text-xs text-[color:var(--as-neon)]/75 leading-relaxed">
                            A responsive desk pet with a vector face mapping. Reacts dynamically to keyboard inputs, CPU usage metrics, and local time cycles.
                          </p>
                        </div>
                        <div className="mt-4 flex gap-1.5 flex-wrap">
                          <span className="text-[8px] font-pixel border border-[color:var(--as-pink)]/40 px-1.5 py-0.5 text-[color:var(--as-pink)]">OLED FACE</span>
                          <span className="text-[8px] font-pixel border border-[color:var(--as-pink)]/40 px-1.5 py-0.5 text-[color:var(--as-pink)]">VECTOR GRAPHICS</span>
                        </div>
                      </div>

                      <div className="pixel-card p-6 border-[color:var(--as-yellow)] bg-[#06080e]/50 flex flex-col justify-between">
                        <div>
                          <div className="flex justify-between items-start mb-3">
                            <span className="font-pixel text-xs text-[color:var(--as-yellow)]">MODE 02 // SENTINEL</span>
                            <span className="font-mono-pixel text-[10px] text-[color:var(--as-neon)]/50">[ACTIVE]</span>
                          </div>
                          <h3 className="font-pixel text-base text-[color:var(--as-yellow)] mb-2">Security Sentinel</h3>
                          <p className="font-mono-pixel text-xs text-[color:var(--as-neon)]/75 leading-relaxed">
                            Monitors local airspace for device presence and network changes. Integrates with the Home Guard rules engine to identify known and unrecognized devices.
                          </p>
                        </div>
                        <div className="mt-4 flex gap-1.5 flex-wrap">
                          <span className="text-[8px] font-pixel border border-[color:var(--as-yellow)]/40 px-1.5 py-0.5 text-[color:var(--as-yellow)]">NETWORK MONITOR</span>
                          <span className="text-[8px] font-pixel border border-[color:var(--as-yellow)]/40 px-1.5 py-0.5 text-[color:var(--as-yellow)]">HOME GUARD</span>
                        </div>
                      </div>

                      <div className="pixel-card p-6 border-[color:var(--as-violet)] bg-[#06080e]/50 flex flex-col justify-between">
                        <div>
                          <div className="flex justify-between items-start mb-3">
                            <span className="font-pixel text-xs text-[color:var(--as-violet)]">MODE 03 // AVIATION OBSERVER</span>
                            <span className="font-mono-pixel text-[10px] text-[color:var(--as-neon)]/50">[ACTIVE]</span>
                          </div>
                          <h3 className="font-pixel text-base text-[color:var(--as-violet)] mb-2">Aviation Observer</h3>
                          <p className="font-mono-pixel text-xs text-[color:var(--as-neon)]/75 leading-relaxed">
                            Polls local ADS-B coordinates and tracks real-time aircraft altitude, heading drift, and speed curves on a dedicated custom retro radar HUD.
                          </p>
                        </div>
                        <div className="mt-4 flex gap-1.5 flex-wrap">
                          <span className="text-[8px] font-pixel border border-[color:var(--as-violet)]/40 px-1.5 py-0.5 text-[color:var(--as-violet)]">ADS-B INGEST</span>
                          <span className="text-[8px] font-pixel border border-[color:var(--as-violet)]/40 px-1.5 py-0.5 text-[color:var(--as-violet)]">OPENSKY API</span>
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Portal Showcase */}
                  <div>
                    <SectionHeader kicker="02 // WEB APIS" title="PORTAL SHOWCASE" />
                    <p className="font-mono-pixel text-sm text-[color:var(--as-neon)]/60 mt-3 mb-8 text-left max-w-3xl">
                      The ESP32 serves a local HTTP/REST API and web console — the AeroPortal — giving you real-time access to everything the device sees.
                    </p>
                    <div className="grid md:grid-cols-3 gap-4 mt-4">
                      <div className="pixel-card p-5 border-[color:var(--as-neon)]/30 bg-[#06080e]/40 text-left">
                        <div className="font-pixel text-[8px] text-[color:var(--as-neon)] mb-2 flex items-center gap-2">
                          <span className="inline-block w-2 h-2 rounded-full bg-[color:var(--as-neon)]" />
                          OVERVIEW
                        </div>
                        <h4 className="font-pixel text-xs text-[color:var(--as-neon)] mb-2">Live Dashboard</h4>
                        <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60 leading-relaxed">
                          Real-time packet stats, channel hopping status, signal heat map, and device connection state — all on one screen.
                        </p>
                        <div className="mt-3 bg-black/60 border border-[color:var(--as-neon)]/10 p-2 font-mono text-[9px] space-y-1">
                          <div className="flex justify-between text-[color:var(--as-yellow)]">
                            <span>PPS</span><span className="text-[color:var(--as-neon)]">1,482</span>
                          </div>
                          <div className="flex justify-between text-[color:var(--as-yellow)]">
                            <span>CH</span><span className="text-[color:var(--as-neon)]">6 (HOP)</span>
                          </div>
                          <div className="flex justify-between text-[color:var(--as-yellow)]">
                            <span>STATE</span><span className="text-green-400">ACTIVE</span>
                          </div>
                        </div>
                      </div>
                      <div className="pixel-card p-5 border-[color:var(--as-orange)]/30 bg-[#06080e]/40 text-left">
                        <div className="font-pixel text-[8px] text-[color:var(--as-orange)] mb-2 flex items-center gap-2">
                          <span className="inline-block w-2 h-2 rounded-full bg-[color:var(--as-orange)]" />
                          THREAT TIMELINE
                        </div>
                        <h4 className="font-pixel text-xs text-[color:var(--as-orange)] mb-2">Event Log & Severity</h4>
                        <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60 leading-relaxed">
                          Chronological event feed with severity badges (critical, warning, info). Filter by type and drill into MAC-level details.
                        </p>
                        <div className="mt-3 bg-black/60 border border-[color:var(--as-orange)]/10 p-2 font-mono text-[9px] space-y-1">
                          <div className="flex items-center gap-1">
                            <span className="text-[color:var(--as-pink)]">◆</span>
                            <span className="text-[color:var(--as-neon)]/70">Unknown device F2:C3:04:88:99:AA</span>
                          </div>
                          <div className="flex items-center gap-1">
                            <span className="text-[color:var(--as-yellow)]">◇</span>
                            <span className="text-[color:var(--as-neon)]/50">Probe request on CH 6</span>
                          </div>
                          <div className="flex items-center gap-1">
                            <span className="text-[color:var(--as-neon)]">○</span>
                            <span className="text-[color:var(--as-neon)]/50">Known device joined</span>
                          </div>
                        </div>
                      </div>
                      <div className="pixel-card p-5 border-[color:var(--as-violet)]/30 bg-[#06080e]/40 text-left">
                        <div className="font-pixel text-[8px] text-[color:var(--as-violet)] mb-2 flex items-center gap-2">
                          <span className="inline-block w-2 h-2 rounded-full bg-[color:var(--as-violet)]" />
                          DEVICES
                        </div>
                        <h4 className="font-pixel text-xs text-[color:var(--as-violet)] mb-2">Discovered & Trusted</h4>
                        <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60 leading-relaxed">
                          Auto-populated device registry showing MAC, vendor, first/last seen, RSSI curve, and trust classification.
                        </p>
                        <div className="mt-3 bg-black/60 border border-[color:var(--as-violet)]/10 p-2 font-mono text-[9px] space-y-1">
                          <div className="flex justify-between text-green-400"><span>iPhone (4A:F2)</span><span>TRUSTED</span></div>
                          <div className="flex justify-between text-[color:var(--as-orange)]"><span>Galaxy (8C:E2)</span><span>KNOWN</span></div>
                          <div className="flex justify-between text-[color:var(--as-pink)]"><span>Unknown (F2:C3)</span><span>NEW</span></div>
                        </div>
                      </div>
                    </div>
                    <div className="grid md:grid-cols-2 gap-4 mt-4">
                      <div className="pixel-card p-5 border-[color:var(--as-yellow)]/30 bg-[#06080e]/40 text-left">
                        <div className="font-pixel text-[8px] text-[color:var(--as-yellow)] mb-2 flex items-center gap-2">
                          <span className="inline-block w-2 h-2 rounded-full bg-[color:var(--as-yellow)]" />
                          HOME GUARD
                        </div>
                        <h4 className="font-pixel text-xs text-[color:var(--as-yellow)] mb-2">Presence Engine</h4>
                        <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60 leading-relaxed">
                          Whitelist familiar devices by MAC. Get notified when recognized devices arrive or leave. Flag unknown ranges as suspicious.
                        </p>
                        <div className="mt-3 bg-black/60 border border-[color:var(--as-yellow)]/10 p-2 font-mono text-[9px] space-y-1">
                          <div className="flex justify-between text-green-400"><span>Aryan's iPhone</span><span>PRESENT</span></div>
                          <div className="flex justify-between text-green-400"><span>Dev Laptop</span><span>PRESENT</span></div>
                          <div className="flex justify-between text-gray-500"><span>Smart TV</span><span>AWAY</span></div>
                        </div>
                      </div>
                      <div className="pixel-card p-5 border-[color:var(--as-pink)]/30 bg-[#06080e]/40 text-left">
                        <div className="font-pixel text-[8px] text-[color:var(--as-pink)] mb-2 flex items-center gap-2">
                          <span className="inline-block w-2 h-2 rounded-full bg-[color:var(--as-pink)]" />
                          SETTINGS
                        </div>
                        <h4 className="font-pixel text-xs text-[color:var(--as-pink)] mb-2">Device Configuration</h4>
                        <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60 leading-relaxed">
                          WiFi credentials, bounding box coordinates, display brightness, Home Guard sensitivity, and NVS-backed preferences.
                        </p>
                        <div className="mt-3 bg-black/60 border border-[color:var(--as-pink)]/10 p-2 font-mono text-[9px] space-y-1">
                          <div className="flex justify-between"><span className="text-[color:var(--as-neon)]/70">WiFi</span><span className="text-[color:var(--as-neon)]">Home_SSID</span></div>
                          <div className="flex justify-between"><span className="text-[color:var(--as-neon)]/70">Sensitivity</span><span className="text-[color:var(--as-yellow)]">MEDIUM</span></div>
                          <div className="flex justify-between"><span className="text-[color:var(--as-neon)]/70">BBox</span><span className="text-[color:var(--as-neon)]">37.7, -122.4</span></div>
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Architecture */}
                  <div>
                    <SectionHeader kicker="03 // SYSTEM PIPELINE" title="OBSERVE → ACT → ANALYZE" />
                    <p className="font-mono-pixel text-sm text-[color:var(--as-neon)]/60 mt-3 mb-8 text-left max-w-3xl">
                      AeroSniffer's architecture is split across three layers — from raw signal capture on the badge to remote analysis.
                    </p>
                    <div className="space-y-0 relative">
                      {/* Layer 1: Device */}
                      <div className="pixel-card p-6 border-[color:var(--as-neon)] bg-[#06080e]/60 relative z-10">
                        <div className="flex items-center gap-3 mb-3">
                          <span className="font-pixel text-[9px] text-[color:var(--as-neon)] border border-[color:var(--as-neon)]/40 px-2 py-0.5">LAYER 01</span>
                          <span className="font-pixel text-sm text-[color:var(--as-neon)]">DEVICE — OBSERVE + REACT</span>
                        </div>
                        <div className="grid md:grid-cols-2 gap-4 text-left font-mono-pixel text-xs">
                          <div className="bg-black/40 border border-[color:var(--as-neon)]/15 p-3">
                            <div className="text-[color:var(--as-neon)]/50 text-[10px] mb-1">OBSERVE</div>
                            <ul className="text-[color:var(--as-neon)]/80 space-y-1">
                              <li>• 802.11 promiscuous monitor</li>
                              <li>• ADS-B airspace polling</li>
                              <li>• Capacitive touch sense</li>
                              <li>• PC telemetry via USB Serial</li>
                            </ul>
                          </div>
                          <div className="bg-black/40 border border-[color:var(--as-neon)]/15 p-3">
                            <div className="text-[color:var(--as-neon)]/50 text-[10px] mb-1">REACT</div>
                            <ul className="text-[color:var(--as-neon)]/80 space-y-1">
                              <li>• FreeRTOS dual-core scheduler</li>
                              <li>• TFT OLED face rendering</li>
                              <li>• Home Guard classification</li>
                              <li>• NVS preference vault</li>
                            </ul>
                          </div>
                        </div>
                      </div>
                      {/* Arrow connector */}
                      <div className="flex justify-center py-2 relative z-10">
                        <div className="w-0.5 h-6 bg-gradient-to-b from-[color:var(--as-neon)] to-[color:var(--as-violet)]" />
                      </div>
                      <div className="flex justify-center relative z-10">
                        <span className="font-pixel text-[8px] text-[color:var(--as-violet)] bg-[#06080e] px-2 border border-[color:var(--as-violet)]/30">↓ WEB SERIAL / TCP / HTTP ↓</span>
                      </div>
                      <div className="flex justify-center py-2 relative z-10">
                        <div className="w-0.5 h-6 bg-gradient-to-b from-[color:var(--as-violet)] to-[color:var(--as-pink)]" />
                      </div>
                      {/* Layer 2: Portal */}
                      <div className="pixel-card p-6 border-[color:var(--as-violet)] bg-[#06080e]/60 relative z-10">
                        <div className="flex items-center gap-3 mb-3">
                          <span className="font-pixel text-[9px] text-[color:var(--as-violet)] border border-[color:var(--as-violet)]/40 px-2 py-0.5">LAYER 02</span>
                          <span className="font-pixel text-sm text-[color:var(--as-violet)]">PORTAL — ACT</span>
                        </div>
                        <div className="bg-black/40 border border-[color:var(--as-violet)]/15 p-3 text-left font-mono-pixel text-xs">
                          <div className="text-[color:var(--as-violet)]/50 text-[10px] mb-1">ACT</div>
                          <ul className="text-[color:var(--as-neon)]/80 space-y-1 grid md:grid-cols-2">
                            <li>• Configure whitelist rules</li>
                            <li>• Set sensitivity thresholds</li>
                            <li>• View live event timeline</li>
                            <li>• Toggle scan channels</li>
                            <li>• Manage device registry</li>
                            <li>• Update bounding box</li>
                          </ul>
                        </div>
                      </div>
                      {/* Arrow connector */}
                      <div className="flex justify-center py-2 relative z-10">
                        <div className="w-0.5 h-6 bg-gradient-to-b from-[color:var(--as-pink)] to-[color:var(--as-yellow)]" />
                      </div>
                      <div className="flex justify-center relative z-10">
                        <span className="font-pixel text-[8px] text-[color:var(--as-yellow)] bg-[#06080e] px-2 border border-[color:var(--as-yellow)]/30">↓ COMPANION APP API / EXPORT ↓</span>
                      </div>
                      <div className="flex justify-center py-2 relative z-10">
                        <div className="w-0.5 h-6 bg-gradient-to-b from-[color:var(--as-yellow)] to-[color:var(--as-orange)]" />
                      </div>
                      {/* Layer 3: Mission Control */}
                      <div className="pixel-card p-6 border-[color:var(--as-yellow)] bg-[#06080e]/60 relative z-10">
                        <div className="flex items-center gap-3 mb-3">
                          <span className="font-pixel text-[9px] text-[color:var(--as-yellow)] border border-[color:var(--as-yellow)]/40 px-2 py-0.5">LAYER 03</span>
                          <span className="font-pixel text-sm text-[color:var(--as-yellow)]">MISSION CONTROL — ANALYZE</span>
                        </div>
                        <div className="bg-black/40 border border-[color:var(--as-yellow)]/15 p-3 text-left font-mono-pixel text-xs">
                          <div className="text-[color:var(--as-yellow)]/50 text-[10px] mb-1">ANALYZE</div>
                          <ul className="text-[color:var(--as-neon)]/80 space-y-1 grid md:grid-cols-2">
                            <li>• Cross-session signal trends</li>
                            <li>• Device presence history</li>
                            <li>• Export PCAP / CSV logs</li>
                            <li>• Multi-node correlation</li>
                            <li>• Encrypted peer mesh sync</li>
                            <li>• Forensic observation queries</li>
                          </ul>
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Development Roadmap */}
                  <div className="pb-10">
                    <SectionHeader kicker="04 // DEVELOPMENT" title="BUILD STATUS" />
                    <p className="font-mono-pixel text-sm text-[color:var(--as-neon)]/60 mt-3 mb-8 text-left max-w-3xl">
                      Current snapshot of where the project stands and what is coming next.
                    </p>
                    <div className="grid md:grid-cols-3 gap-4 mt-8">
                      <div className="pixel-card p-5 border-[color:var(--as-neon)]/30 bg-[#06080e]/40 text-left">
                        <div className="flex items-center gap-2 mb-3">
                          <span className="inline-block w-2 h-2 rounded-full bg-green-400" />
                          <span className="font-pixel text-[8px] text-green-400">STABLE</span>
                        </div>
                        <div className="space-y-3">
                          <div>
                            <h4 className="font-pixel text-xs text-[color:var(--as-neon)] mb-1">Firmware V2.3</h4>
                            <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60">
                              Threat intelligence pipeline, Home Guard presence engine, aviation radar, NVS settings layer.
                            </p>
                          </div>
                          <div>
                            <h4 className="font-pixel text-xs text-[color:var(--as-neon)] mb-1">AeroPortal</h4>
                            <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60">
                              Overview dashboard, device registry, threat timeline, Home Guard config, settings panel.
                            </p>
                          </div>
                          <div>
                            <h4 className="font-pixel text-xs text-[color:var(--as-neon)] mb-1">Companion App</h4>
                            <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60">
                              Web serial console, BotFace emotion control, live radar panel, flight tracker.
                            </p>
                          </div>
                        </div>
                      </div>
                      <div className="pixel-card p-5 border-[color:var(--as-yellow)]/30 bg-[#06080e]/40 text-left">
                        <div className="flex items-center gap-2 mb-3">
                          <span className="inline-block w-2 h-2 rounded-full bg-[color:var(--as-yellow)]" />
                          <span className="font-pixel text-[8px] text-[color:var(--as-yellow)]">IN PROGRESS</span>
                        </div>
                        <div className="space-y-3">
                          <div>
                            <h4 className="font-pixel text-xs text-[color:var(--as-yellow)] mb-1">Website V2</h4>
                            <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60">
                              Identity migration, Portal showcase, architecture diagram, product landing page.
                            </p>
                          </div>
                          <div>
                            <h4 className="font-pixel text-xs text-[color:var(--as-yellow)] mb-1">RC1 Validation</h4>
                            <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60">
                              19 required tests across 7 categories. Bug sprint, V2.3 release candidate.
                            </p>
                          </div>
                        </div>
                      </div>
                      <div className="pixel-card p-5 border-[color:var(--as-violet)]/30 bg-[#06080e]/40 text-left">
                        <div className="flex items-center gap-2 mb-3">
                          <span className="inline-block w-2 h-2 rounded-full bg-[color:var(--as-violet)]" />
                          <span className="font-pixel text-[8px] text-[color:var(--as-violet)]">NEXT</span>
                        </div>
                        <div className="space-y-3">
                          <div>
                            <h4 className="font-pixel text-xs text-[color:var(--as-violet)] mb-1">V2.3 Release</h4>
                            <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60">
                              Final bug fixes, documentation, packaging, and distribution.
                            </p>
                          </div>
                          <div>
                            <h4 className="font-pixel text-xs text-[color:var(--as-violet)] mb-1">V3.0 Mission Control</h4>
                            <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60">
                              Multi-node correlation, encrypted peer mesh, forensic observation interfaces.
                            </p>
                          </div>
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Open Source */}
                  <div className="pb-10">
                    <SectionHeader kicker="05 // COMMUNITY" title="OPEN SOURCE" />
                    <div className="grid md:grid-cols-2 gap-6 mt-8">
                      <div className="pixel-card p-6 border-[color:var(--as-neon)]/30 bg-[#06080e]/40 text-left">
                        <h4 className="font-pixel text-xs text-[color:var(--as-neon)] mb-2">Fully Open Hardware & Software</h4>
                        <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60 leading-relaxed">
                          AeroSniffer is built in the open. The entire firmware (FreeRTOS/C++), companion web app (React/TypeScript), and PC agent tools are published on GitHub. Circuit schematics and PCB design files are included for the ESP32-S3 badge.
                        </p>
                      </div>
                      <div className="pixel-card p-6 border-[color:var(--as-violet)]/30 bg-[#06080e]/40 text-left">
                        <h4 className="font-pixel text-xs text-[color:var(--as-violet)] mb-2">MIT Licensed</h4>
                        <p className="font-mono-pixel text-[11px] text-[color:var(--as-neon)]/60 leading-relaxed">
                          All source code is released under the MIT license. Fork it, hack it, build your own variant — attribution is all we ask. Contributions, issues, and PRs are welcome.
                        </p>
                      </div>
                    </div>
                  </div>
                </div>
              )}
            </div>
          )}

          {activeTab === "modes" && (
            <div className="max-w-6xl mx-auto px-6 space-y-10">
              <SectionHeader kicker="01 // OPERATING MODES" title="THREE SOULS, ONE SHELL" />
              <div className="grid md:grid-cols-3 gap-5 mt-10">
                <ModeCard
                  n={1}
                  name="Cyber-Pet"
                  face="love"
                  desc="A living desktop companion. High-FPS vector face that reacts to your typing, CPU load, and the apps you open. He gets sleepy when you do."
                  tags={["240×240 OLED", "PC-driven", "reacts to you"]}
                  active={mode === 0}
                  onClick={() => {
                    setMode(0);
                    if (isConnected) {
                      serialAPI.sendCommand("SET_MODE:0");
                    }
                  }}
                />
                <ModeCard
                  n={2}
                  name="Security Sentinel"
                  face="sec_scanning"
                  desc="Network monitor with an animated radar sweep + companion web app for presence awareness. Plug in, observe, learn."
                  tags={["Network Monitor", "Security Sentinel", "Web Serial"]}
                  active={mode === 1}
                  onClick={() => {
                    setMode(1);
                    if (isConnected) {
                      serialAPI.sendCommand("SET_MODE:1");
                    }
                  }}
                />
                <ModeCard
                  n={3}
                  name="Aviation Observer"
                  face="excited"
                  desc="Live ADS-B tracker pulling from the OpenSky Network. Watch callsigns, altitude, speed, and heading drift across his tiny screen."
                  tags={["ADS-B live", "OpenSky", "compass"]}
                  active={mode === 2}
                  onClick={() => {
                    setMode(2);
                    if (isConnected) {
                      serialAPI.sendCommand("SET_MODE:2");
                    }
                  }}
                />
              </div>

              {/* Mode Specific panels */}
              <div className="mt-10">
                {mode === 1 && <LiveRadarPanel />}
                {mode === 2 && <FlightPanel isConnected={isConnected} devMode={devMode} />}
                {mode === 0 && <PetPanel isConnected={isConnected} />}
              </div>
            </div>
          )}

          {activeTab === "faces" && (
            <div className="max-w-6xl mx-auto px-6">
              <SectionHeader kicker="02 // EMOTION REGISTER" title="HIS LITTLE FEELINGS" />
              <p className="max-w-2xl mx-auto text-center font-mono-pixel text-[color:var(--as-neon)]/70 text-lg mt-3">
                Select an emotion below to command your AeroSniffer.
                Each face is animated using custom vector SVGs inside the browser and mapped to core emotions on the ESP32.
              </p>

              {(["CORE EMOTIONS", "SECURITY", "AVIATION", "SYSTEM", "SECRET"] as const).map((g) => (
                <div key={g} className="max-w-6xl mx-auto mt-12">
                  <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/60 mb-4">
                    ▲ {g}
                  </div>
                  <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 gap-4">
                    {Object.keys(FACE_META)
                      .filter((f) => FACE_META[f as FaceState].group === g)
                      .map((f) => (
                        <button
                          key={f}
                          onClick={() => {
                            setFace(f as FaceState);
                            if (isConnected) {
                              const WEB_TO_ESP_FACE: Record<string, string> = {
                                idle: "IDLE",
                                happy: "HAPPY",
                                excited: "EXCITED",
                                sleepy: "SLEEPY",
                                thinking: "THINKING",
                                sad: "SAD_ERROR",
                                surprised: "SURPRISED",
                                love: "LOVE_BONDING",
                                sec_scanning: "SEC_SCANNING",
                                sec_intrusion: "SEC_INTRUSION",
                                avi_radar: "AVI_RADAR",
                                avi_lock: "AVI_LOCK",
                                avi_disabled: "AVI_DISABLED",
                                sys_boot: "SYS_BOOT",
                                sys_prefs: "SYS_PREFS",
                                sys_error: "SYS_ERROR",
                                sec_matrix: "SEC_MATRIX",
                                sec_retro: "SEC_RETRO",
                                sec_rainbow: "SEC_RAINBOW",
                              };
                              const espFace = WEB_TO_ESP_FACE[f];
                              if (espFace) {
                                serialAPI.sendCommand(`FACE:${espFace}`, true);
                              }
                            }
                          }}
                          className={`pixel-card p-4 text-left transition-all hover:-translate-y-1 ${
                            face === f ? "ring-2 ring-[color:var(--as-neon)]" : ""
                          }`}
                          style={
                            face === f
                              ? {
                                  boxShadow: `0 0 0 2px ${FACE_META[f as FaceState].color}, 0 0 28px ${FACE_META[f as FaceState].color}66`,
                                }
                              : undefined
                          }
                        >
                          <BotFace state={f as FaceState} size={220} />
                          <div className="mt-3 font-pixel text-xs" style={{ color: FACE_META[f as FaceState].color }}>
                            {FACE_META[f as FaceState].label}
                          </div>
                          <div className="font-mono-pixel text-[color:var(--as-neon)]/60 text-base mt-1">
                            {FACE_META[f as FaceState].sub}
                          </div>
                        </button>
                      ))}
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* FOOTER */}
        <footer className="border-t border-[color:var(--as-neon)]/20 px-6 py-10">
          <div className="max-w-6xl mx-auto flex flex-wrap items-center justify-between gap-4">
            <div className="flex items-center gap-3">
              <BotFace state="idle" size={56} />
              <div>
                <div className="font-pixel text-xs neon-text">AeroSniffer</div>
                <div className="font-mono-pixel text-[color:var(--as-neon)]/60 text-sm">
                  Platform v2.0 · open source · embedded intelligence
                </div>
              </div>
            </div>
            <div className="flex flex-col md:flex-row items-start md:items-center gap-4 font-mono-pixel text-[color:var(--as-neon)]/60 text-base">
              <span>Ensure your device is plugged in via USB-C · set Mode 2 for Security.</span>
              <button
                onClick={() => setDevMode(!devMode)}
                className={`font-pixel text-[9px] px-3 py-1 border transition-all ${
                  devMode
                    ? "border-[color:var(--as-orange)] text-[color:var(--as-orange)] bg-[color:var(--as-orange)]/15"
                    : "border-[color:var(--as-neon)]/30 text-[color:var(--as-neon)]/50 hover:border-[color:var(--as-neon)] hover:text-[color:var(--as-neon)]"
                }`}
              >
                {devMode ? "DIAGNOSTICS: ACTIVE" : "DIAGNOSTICS: HIDDEN"}
              </button>
            </div>
          </div>
        </footer>
      </div>
    </main>
  );
}

/* ───── helpers ───── */

interface NavProps {
  onFace: (f: FaceState) => void;
  isConnected: boolean;
  setIsConnected: (connected: boolean) => void;
  activeTab: "companion" | "modes" | "faces";
  setActiveTab: (tab: "companion" | "modes" | "faces") => void;
  aviationEnabled: boolean;
  setAviationEnabled: (enabled: boolean) => void;
}

function Nav({ onFace, isConnected, setIsConnected, activeTab, setActiveTab, aviationEnabled, setAviationEnabled }: NavProps) {
  const [showSetup, setShowSetup] = useState(false);
  const [deviceData, setDeviceData] = useState<any>(null);
  const [wifi, setWifi] = useState({ ssid: "", pass: "" });
  const [bbox, setBbox] = useState({ lamin: "", lomin: "", lamax: "", lomax: "" });
  const [refreshInterval, setRefreshInterval] = useState<number>(30000);
  const [setupMsg, setSetupMsg] = useState("");
  const [searchQuery, setSearchQuery] = useState("");
  const [isSearching, setIsSearching] = useState(false);

  useEffect(() => {
    // Sync initial state
    setIsConnected(serialAPI.port !== null);

    const removeListener = serialAPI.addListener((type: string, data: any) => {
      if (type === "RES") {
        if (data.fw || data.lamin !== undefined) {
          setDeviceData(data);
          if (data.lamin !== undefined) {
            setBbox({
              lamin: data.lamin.toString(),
              lomin: data.lomin.toString(),
              lamax: data.lamax.toString(),
              lomax: data.lomax.toString(),
            });
          }
          if (data.ssid && data.ssid !== "YOUR_WIFI_SSID") {
            setWifi((w) => ({ ...w, ssid: data.ssid }));
          }
          if (data.refresh !== undefined) {
            setRefreshInterval(Number(data.refresh));
          }
          if (data.aviation_enabled !== undefined) {
            setAviationEnabled(data.aviation_enabled === true || data.aviation_enabled === "true");
          }
        }
      }
    });

    const removeConnect = serialAPI.addConnectListener(() => setIsConnected(true));
    const removeDisconnect = serialAPI.addDisconnectListener(() => setIsConnected(false));

    return () => {
      removeListener();
      removeConnect();
      removeDisconnect();
    };
  }, []); // Only run once on mount

  // Auto-detect first boot setup requirement
  useEffect(() => {
    if (isConnected && deviceData?.ssid === "YOUR_WIFI_SSID" && !showSetup) {
      setTimeout(() => setShowSetup(true), 1500);
    }
  }, [isConnected, deviceData?.ssid]);

  const handleConnect = async () => {
    try {
      setSetupMsg("");
      await serialAPI.connect();
      setIsConnected(true);
      await serialAPI.sendCommand("PING");
      setTimeout(() => serialAPI.sendCommand("GET_CFG"), 500);
    } catch (e: any) {
      setSetupMsg(e.message || "Connection failed.");
    }
  };

  const handleAutoLocate = () => {
    if (navigator.geolocation) {
      navigator.geolocation.getCurrentPosition((pos) => {
        const lat = pos.coords.latitude;
        const lon = pos.coords.longitude;
        setBbox({
          lamin: (lat - 0.6).toFixed(2),
          lamax: (lat + 0.6).toFixed(2),
          lomin: (lon - 0.9).toFixed(2),
          lomax: (lon + 0.9).toFixed(2),
        });
      }, () => setSetupMsg("Location access denied."));
    } else {
      setSetupMsg("Geolocation not supported.");
    }
  };

  const handleCitySearch = async () => {
    if (!searchQuery.trim()) return;
    setIsSearching(true);
    setSetupMsg("Searching for city...");
    try {
      const res = await fetch(`https://nominatim.openstreetmap.org/search?format=json&q=${encodeURIComponent(searchQuery)}`);
      const data = await res.json();
      if (data && data.length > 0) {
        const lat = parseFloat(data[0].lat);
        const lon = parseFloat(data[0].lon);
        setBbox({
          lamin: (lat - 0.6).toFixed(2),
          lamax: (lat + 0.6).toFixed(2),
          lomin: (lon - 0.9).toFixed(2),
          lomax: (lon + 0.9).toFixed(2),
        });
        setSetupMsg(`Found: ${data[0].display_name.split(',')[0]}`);
      } else {
        setSetupMsg("City not found.");
      }
    } catch (err) {
      setSetupMsg("Search failed.");
    } finally {
      setIsSearching(false);
    }
  };

  const handleSave = () => {
    if (wifi.ssid) serialAPI.sendCommand(`SET_WIFI:${wifi.ssid}:${wifi.pass}`);
    if (bbox.lamin) serialAPI.sendCommand(`SET_BBOX:${bbox.lamin}:${bbox.lomin}:${bbox.lamax}:${bbox.lomax}`);
    if (refreshInterval) serialAPI.sendCommand(`SET_REFRESH:${refreshInterval}`);
    serialAPI.sendCommand(`SET_AVIATION:${aviationEnabled}`);
    setSetupMsg("Saved! Rebooting robot...");
    setTimeout(() => {
      serialAPI.sendCommand("REBOOT");
      serialAPI.disconnect();
      setIsConnected(false);
      setShowSetup(false);
    }, 1000);
  };

  const items: { label: string; tab: "companion" | "modes" | "faces"; face: FaceState }[] = [
    { label: "companion", tab: "companion", face: "thinking" },
    { label: "modes", tab: "modes", face: "excited" },
    { label: "faces", tab: "faces", face: "happy" },
  ];
  return (
    <>
      <nav className="sticky top-0 z-40 backdrop-blur-md bg-[#06080e]/70 border-b border-[color:var(--as-neon)]/20">
        <div className="max-w-6xl mx-auto flex items-center justify-between px-6 py-3">
          <div className="flex items-center gap-3">
            <button onClick={() => setActiveTab("companion")} className="flex items-center gap-3">
              <BotFace state="idle" size={44} />
              <span className="font-pixel text-xs neon-text hidden sm:inline">AeroSniffer</span>
            </button>
            <div className="flex items-center">
              {isConnected ? (
                <span className="font-pixel text-[8px] border border-green-500 text-green-400 bg-green-950/20 px-2 py-0.5 animate-pulse">
                  ● LIVE
                </span>
              ) : (
                <button
                  onClick={handleConnect}
                  className="font-pixel text-[8px] border border-[color:var(--as-orange)] text-[color:var(--as-orange)] bg-[color:var(--as-orange)]/10 px-2 py-0.5 hover:bg-[color:var(--as-orange)] hover:text-black transition-colors"
                  title="Click to link physical USB-C device"
                >
                  ● DEMO (CONNECT)
                </button>
              )}
            </div>
          </div>
          <ul className="flex items-center gap-1 sm:gap-4">
            {items.map((i) => (
              <li key={i.label}>
                <button
                  onClick={() => setActiveTab(i.tab)}
                  onMouseEnter={() => onFace(i.face)}
                  className={`font-pixel text-[10px] px-3 py-2 ${
                    activeTab === i.tab
                      ? "text-[color:var(--as-neon)] underline"
                      : "text-[color:var(--as-neon)]/70 hover:text-[color:var(--as-neon)]"
                  }`}
                >
                  {i.label.toUpperCase()}
                </button>
              </li>
            ))}
            <li>
              <button
                onClick={() => setShowSetup(true)}
                className="font-pixel text-[10px] text-[color:var(--as-yellow)] hover:underline px-3 py-2 border border-[color:var(--as-yellow)]"
                onMouseEnter={() => onFace("thinking")}
              >
                ⚙️ SETUP
              </button>
            </li>
          </ul>
        </div>
      </nav>

      {/* GLOBAL SETUP MODAL */}
      {showSetup && (
        <div className="fixed inset-0 z-[100] bg-black/80 flex items-center justify-center p-4">
          <div className="bg-[#06080e] border-2 border-[color:var(--as-neon)] p-6 max-w-md w-full pixel-card">
            <div className="flex justify-between items-center mb-6 border-b border-[color:var(--as-neon)]/20 pb-4">
              <div className="font-pixel text-lg text-[color:var(--as-neon)]">
                ⚙️ AEROSNIFFER SETUP
              </div>
              <button onClick={() => setShowSetup(false)} className="text-[color:var(--as-pink)] font-pixel text-xs hover:underline">
                [X] CLOSE
              </button>
            </div>

            {!isConnected ? (
              <div className="text-center py-6">
                <p className="font-mono-pixel text-[color:var(--as-neon)]/70 mb-6 text-sm">
                  To configure WiFi and Location, connect your AeroSniffer via USB (works in any mode!).
                </p>
                <button onClick={handleConnect} className="pixel-btn w-full">
                  CONNECT VIA WEB SERIAL
                </button>
                {setupMsg && <p className="mt-4 font-mono-pixel text-[color:var(--as-pink)] text-xs">{setupMsg}</p>}
              </div>
            ) : (
              <div className="space-y-4">
                <div className="font-pixel text-[10px] text-[color:var(--as-orange)] mb-4">
                  ▲ CONNECTED TO ESP32
                </div>
                
                <div>
                  <label className="block font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-2">Wi-Fi SSID</label>
                  <input type="text" value={wifi.ssid} onChange={(e) => setWifi(w => ({ ...w, ssid: e.target.value }))} className="w-full bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-sm" placeholder="Network Name" />
                </div>
                <div>
                  <label className="block font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-2">Wi-Fi Password</label>
                  <input type="password" value={wifi.pass} onChange={(e) => setWifi(w => ({ ...w, pass: e.target.value }))} className="w-full bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-sm" placeholder="Password" />
                </div>
                <div>
                  <label className="block font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-2">Flight Refresh Rate</label>
                  <select 
                    value={refreshInterval} 
                    onChange={(e) => setRefreshInterval(Number(e.target.value))} 
                    className="w-full bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-sm focus:outline-none focus:border-[color:var(--as-neon)]"
                  >
                    <option value={15000} className="bg-[#06080e] text-[color:var(--as-neon)]">15 Seconds</option>
                    <option value={30000} className="bg-[#06080e] text-[color:var(--as-neon)]">30 Seconds</option>
                    <option value={60000} className="bg-[#06080e] text-[color:var(--as-neon)]">60 Seconds</option>
                    <option value={120000} className="bg-[#06080e] text-[color:var(--as-neon)]">120 Seconds</option>
                  </select>
                </div>

                <div>
                  <label className="block font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-2">Aviation Mode (ADS-B)</label>
                  <button
                    onClick={() => setAviationEnabled(!aviationEnabled)}
                    className={`w-full font-pixel text-xs p-2.5 border transition-all flex items-center justify-between ${
                      aviationEnabled
                        ? "border-[color:var(--as-neon)] text-[color:var(--as-neon)] bg-[color:var(--as-neon)]/15 font-bold"
                        : "border-[color:var(--as-pink)]/50 text-[color:var(--as-pink)]/70 bg-[color:var(--as-pink)]/5"
                    }`}
                  >
                    <span>{aviationEnabled ? "ACTIVE" : "DISABLED"}</span>
                    <span className="font-mono-pixel text-[10px] opacity-75">{aviationEnabled ? "[ ON ]" : "[ OFF ]"}</span>
                  </button>
                </div>

                <div className="border-t border-[color:var(--as-neon)]/20 pt-4 mt-4">
                  <div className="flex flex-col gap-2 mb-3">
                    <label className="block font-pixel text-[10px] text-[color:var(--as-yellow)]/70">Mode 3 Radar Bounds</label>
                    
                    <div className="flex gap-2">
                      <input 
                        type="text" 
                        value={searchQuery}
                        onChange={(e) => setSearchQuery(e.target.value)}
                        onKeyDown={(e) => e.key === 'Enter' && handleCitySearch()}
                        placeholder="Search City..." 
                        className="flex-1 bg-black border border-[color:var(--as-yellow)]/30 p-2 font-mono-pixel text-[color:var(--as-yellow)] text-xs"
                      />
                      <button 
                        onClick={handleCitySearch}
                        disabled={isSearching}
                        className="text-[10px] font-pixel text-[color:var(--as-yellow)] hover:underline border border-[color:var(--as-yellow)] px-2 py-1 whitespace-nowrap"
                      >
                        {isSearching ? "[ ... ]" : "[ SEARCH ]"}
                      </button>
                      <button onClick={handleAutoLocate} className="text-[10px] font-pixel text-[color:var(--as-yellow)] hover:underline border border-[color:var(--as-yellow)] px-2 py-1 whitespace-nowrap" title="Use GPS">
                        [ GPS ]
                      </button>
                    </div>
                  </div>
                  <div className="grid grid-cols-2 gap-2 mt-2">
                    <input type="number" step="0.1" value={bbox.lamin} onChange={e => setBbox(b => ({...b, lamin: e.target.value}))} placeholder="Min Lat" className="bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-sm" />
                    <input type="number" step="0.1" value={bbox.lamax} onChange={e => setBbox(b => ({...b, lamax: e.target.value}))} placeholder="Max Lat" className="bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-sm" />
                    <input type="number" step="0.1" value={bbox.lomin} onChange={e => setBbox(b => ({...b, lomin: e.target.value}))} placeholder="Min Lon" className="bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-sm" />
                    <input type="number" step="0.1" value={bbox.lomax} onChange={e => setBbox(b => ({...b, lomax: e.target.value}))} placeholder="Max Lon" className="bg-black border border-[color:var(--as-neon)]/30 p-2 font-mono-pixel text-[color:var(--as-neon)] text-sm" />
                  </div>
                </div>

                {setupMsg && <p className="font-mono-pixel text-[color:var(--as-neon)] text-xs text-center my-2">{setupMsg}</p>}

                <div className="pt-4 mt-2">
                  <button onClick={handleSave} className="pixel-btn w-full py-3 text-xs mb-3">
                    SAVE & REBOOT ROBOT
                  </button>
                  <button onClick={() => { serialAPI.disconnect(); setIsConnected(false); }} className="pixel-btn pixel-btn-ghost w-full py-2 text-xs border-[color:var(--as-pink)] text-[color:var(--as-pink)]">
                    DISCONNECT
                  </button>
                </div>
              </div>
            )}
          </div>
        </div>
      )}
    </>
  );
}

function SectionHeader({ kicker, title }: { kicker: string; title: string }) {
  return (
    <div className="max-w-6xl mx-auto text-center">
      <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70">{kicker}</div>
      <h2 className="font-pixel neon-text text-2xl md:text-4xl mt-3">{title}</h2>
    </div>
  );
}

function ModeCard({
  n,
  name,
  face,
  desc,
  tags,
  active,
  onClick,
}: {
  n: number;
  name: string;
  face: FaceState;
  desc: string;
  tags: string[];
  active: boolean;
  onClick: () => void;
}) {
  return (
    <button
      onClick={onClick}
      className={`pixel-card p-5 text-left transition-all hover:-translate-y-1 ${
        active ? "ring-2 ring-[color:var(--as-neon)]" : ""
      }`}
      style={
        active
          ? { boxShadow: `0 0 0 2px ${FACE_META[face].color}, 0 0 30px ${FACE_META[face].color}55` }
          : undefined
      }
    >
      <div className="flex items-start justify-between">
        <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70">MODE {n}</div>
        <BotFace state={face} size={90} />
      </div>
      <div className="font-pixel text-lg mt-3" style={{ color: FACE_META[face].color }}>
        {name}
      </div>
      <p className="font-mono-pixel text-[color:var(--as-neon)]/80 text-lg mt-2">{desc}</p>
      <div className="flex flex-wrap gap-2 mt-4">
        {tags.map((t) => (
          <span
            key={t}
            className="font-pixel text-[9px] px-2 py-1 border border-[color:var(--as-neon)]/40 text-[color:var(--as-neon)]/80"
          >
            {t}
          </span>
        ))}
      </div>
    </button>
  );
}

function Marquee() {
  const items = [
    "★ XIAO ESP32-S3",
    "● FreeRTOS",
    "▲ 240×240 OLED",
    "◆ WiFi 802.11",
    "♥ ADS-B Live",
    "■ Web Serial",
    "✦ AeroSniffer OS 2.0",
    "◉ Pixel Soul",
  ];
  const row = [...items, ...items];
  return (
    <div className="border-y border-[color:var(--as-neon)]/20 bg-[#06080e]/60 overflow-hidden">
      <div className="marquee-track flex gap-10 whitespace-nowrap py-3">
        {row.map((it, i) => (
          <span key={i} className="font-pixel text-[10px] text-[color:var(--as-neon)]/70">
            {it}
          </span>
        ))}
      </div>
    </div>
  );
}

interface FlightPanelProps {
  isConnected: boolean;
  devMode?: boolean;
}

function FlightPanel({ isConnected, devMode }: FlightPanelProps) {
  const [flights, setFlights] = useState<any[]>([]);
  const [bbox, setBbox] = useState<any>(null);
  const [lastUpdated, setLastUpdated] = useState<string>("");
  const [debugLog, setDebugLog] = useState<string[]>([]);
  const [rawJson, setRawJson] = useState<string>("");
  const [showDebug, setShowDebug] = useState(false);

  const addDebug = (msg: string) => {
    const ts = new Date().toLocaleTimeString();
    setDebugLog((prev) => [`[${ts}] ${msg}`, ...prev.slice(0, 29)]);
  };

  useEffect(() => {
    if (!isConnected) return;

    serialAPI.sendCommand("GET_CFG");
    addDebug("Sent GET_CFG");

    const removeListener = serialAPI.addListener((type: string, data: any) => {
      if (type === "RES") {
        if (data.flights && Array.isArray(data.flights)) {
          console.log(`[WEB] FlightPanel received ${data.flights.length} aircraft`, data.flights);
          addDebug(`RES received: ${data.flights.length} aircraft`);
          setFlights(data.flights);
          setLastUpdated(new Date().toLocaleTimeString());
          setRawJson(JSON.stringify(data, null, 2).substring(0, 2000));
        } else if (data.flights !== undefined && typeof data.flights !== "number") {
          // flights key exists but is not an array and is not a count number
          console.warn("[WEB] data.flights is not an array:", data.flights);
          addDebug(`WARN: data.flights is not array: ${typeof data.flights}`);
        }
        if (data.lamin !== undefined) {
          setBbox(data);
          addDebug(`Config received: bbox ${data.lamin}/${data.lomin}/${data.lamax}/${data.lomax}`);
        }
      }
    });

    serialAPI.sendCommand("GET_FLIGHTS");
    addDebug("Sent initial GET_FLIGHTS");

    const interval = setInterval(() => {
      serialAPI.sendCommand("GET_FLIGHTS");
    }, 3000);

    return () => {
      clearInterval(interval);
      removeListener();
    };
  }, [isConnected]);

  // Simulation effect for Demo Mode
  useEffect(() => {
    if (isConnected) return;

    setBbox({
      lamin: "28.30",
      lomin: "76.80",
      lamax: "28.90",
      lomax: "77.60"
    });

    const initialFlights = [
      { callsign: "AIC304", alt: 9800, spd: 240, hdg: 120, lat: 28.56, lon: 77.10, gnd: false },
      { callsign: "IND912", alt: 4500, spd: 180, hdg: 45, lat: 28.61, lon: 77.25, gnd: false },
      { callsign: "UAE77", alt: 10500, spd: 250, hdg: 270, lat: 28.45, lon: 77.02, gnd: false },
      { callsign: "AIC420", alt: 220, spd: 15, hdg: 90, lat: 28.56, lon: 77.09, gnd: true }
    ];
    setFlights(initialFlights);
    setLastUpdated(new Date().toLocaleTimeString());
    setRawJson(JSON.stringify({ flights: initialFlights }, null, 2));

    const simInterval = setInterval(() => {
      setFlights((prevFlights) =>
        prevFlights.map((f) => {
          const altDelta = Math.round((Math.random() - 0.5) * 60);
          const spdDelta = Math.round((Math.random() - 0.5) * 4);
          const latDelta = (Math.random() - 0.5) * 0.003;
          const lonDelta = (Math.random() - 0.5) * 0.003;
          return {
            ...f,
            alt: Math.max(100, f.alt + altDelta),
            spd: Math.max(10, f.spd + spdDelta),
            lat: f.lat + latDelta,
            lon: f.lon + lonDelta,
          };
        })
      );
      const ts = new Date().toLocaleTimeString();
      setLastUpdated(ts);
      setDebugLog((prev) => [`[${ts}] Simulated ADS-B update processed: 4 targets active`, ...prev.slice(0, 19)]);
    }, 3000);

    return () => clearInterval(simInterval);
  }, [isConnected]);

  const centerLat = bbox ? (parseFloat(bbox.lamin) + parseFloat(bbox.lamax)) / 2 : 0;
  const centerLon = bbox ? (parseFloat(bbox.lomin) + parseFloat(bbox.lomax)) / 2 : 0;

  const getDistanceKm = (lat: number, lon: number) => {
    if (!centerLat || !centerLon) return 0;
    const R = 6371;
    const dLat = ((lat - centerLat) * Math.PI) / 180;
    const dLon = ((lon - centerLon) * Math.PI) / 180;
    const a =
      Math.sin(dLat / 2) * Math.sin(dLat / 2) +
      Math.cos((centerLat * Math.PI) / 180) *
        Math.cos((lat * Math.PI) / 180) *
        Math.sin(dLon / 2) *
        Math.sin(dLon / 2);
    const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
    return R * c;
  };

  const getCompass = (deg: number) => {
    const sectors = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"];
    const index = Math.round(deg / 45) % 8;
    return sectors[index];
  };

  const chartData = flights.map((f) => ({
    callsign: f.callsign || "UNK",
    altitude: Math.round(f.alt * 3.28084),
    speed: Math.round(f.spd * 1.94384),
    distance: Math.round(getDistanceKm(f.lat, f.lon)),
  }));

  return (
    <div className="max-w-6xl mx-auto mt-10 space-y-6">
      {!isConnected && (
        <div className="pixel-card p-3 border-[color:var(--as-orange)]/50 bg-[color:var(--as-orange)]/5 font-pixel text-[9px] text-[color:var(--as-orange)] flex items-center justify-between">
          <span>▲ DEMO MODE: RUNNING SIMULATED ADS-B FLIGHT RADAR</span>
          <span className="animate-pulse">● SIMULATING</span>
        </div>
      )}
      <div className="pixel-card p-6 border-[color:var(--as-yellow)]">
        <div className="flex flex-wrap items-center justify-between gap-4 border-b border-[color:var(--as-yellow)]/20 pb-4 mb-4">
          <div className="font-pixel text-[10px] text-[color:var(--as-yellow)]">
            ▲ MODE 3 · FLIGHT RADAR · live ADS-B tracker
          </div>
          {lastUpdated && (
            <div className="font-mono-pixel text-xs text-[color:var(--as-yellow)]/60">
              last update: {lastUpdated}
            </div>
          )}
        </div>

        <div className="grid grid-cols-2 md:grid-cols-4 gap-4 font-mono-pixel text-[color:var(--as-neon)]/80 text-sm">
          <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/20">
            <div className="text-[color:var(--as-neon)]/50 text-xs mb-1">AIRCRAFT TRACKED</div>
            <div className="text-xl text-[color:var(--as-yellow)]">{flights.length}</div>
          </div>
          <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/20">
            <div className="text-[color:var(--as-neon)]/50 text-xs mb-1">BOUNDING BOX</div>
            <div className="text-xs truncate">
              {bbox ? `${bbox.lamin}°N to ${bbox.lamax}°N` : "Loading..."}
            </div>
          </div>
          <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/20">
            <div className="text-[color:var(--as-neon)]/50 text-xs mb-1">AIRSPACE CENTER</div>
            <div className="text-xs truncate">
              {bbox ? `${centerLat.toFixed(2)}°, ${centerLon.toFixed(2)}°` : "Loading..."}
            </div>
          </div>
          <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/20">
            <div className="text-[color:var(--as-neon)]/50 text-xs mb-1">WIFI STATUS</div>
            <div className="text-xs text-[color:var(--as-neon)]">CONNECTED</div>
          </div>
        </div>
      </div>

      {flights.length > 0 ? (
        <div className="grid md:grid-cols-2 gap-6">
          <div className="pixel-card p-4">
            <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-4">
              ▲ AIRCRAFT ALTITUDES (FEET)
            </div>
            <div className="h-[250px] w-full font-mono-pixel text-xs">
              <ResponsiveContainer width="100%" height="100%">
                <BarChart data={chartData} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                  <XAxis dataKey="callsign" stroke="var(--as-neon)" opacity={0.6} />
                  <YAxis stroke="var(--as-neon)" opacity={0.6} />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: "#06080e",
                      borderColor: "var(--as-neon)",
                      color: "var(--as-neon)",
                    }}
                    cursor={{ fill: "rgba(74, 240, 188, 0.1)" }}
                  />
                  <Bar dataKey="altitude" fill="var(--as-yellow)" radius={[4, 4, 0, 0]} />
                </BarChart>
              </ResponsiveContainer>
            </div>
          </div>

          <div className="pixel-card p-4">
            <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-4">
              ▲ AIRCRAFT SPEEDS (KNOTS)
            </div>
            <div className="h-[250px] w-full font-mono-pixel text-xs">
              <ResponsiveContainer width="100%" height="100%">
                <AreaChart data={chartData} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                  <defs>
                    <linearGradient id="speedGrad" x1="0" y1="0" x2="0" y2="1">
                      <stop offset="5%" stopColor="var(--as-violet)" stopOpacity={0.6} />
                      <stop offset="95%" stopColor="var(--as-violet)" stopOpacity={0} />
                    </linearGradient>
                  </defs>
                  <XAxis dataKey="callsign" stroke="var(--as-neon)" opacity={0.6} />
                  <YAxis stroke="var(--as-neon)" opacity={0.6} />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: "#06080e",
                      borderColor: "var(--as-neon)",
                      color: "var(--as-neon)",
                    }}
                  />
                  <Area
                    type="monotone"
                    dataKey="speed"
                    stroke="var(--as-violet)"
                    fillOpacity={1}
                    fill="url(#speedGrad)"
                  />
                </AreaChart>
              </ResponsiveContainer>
            </div>
          </div>
        </div>
      ) : (
        <div className="pixel-card p-10 text-center font-mono-pixel text-[color:var(--as-neon)]/60 text-lg">
          No live flights in range or fetching data from OpenSky...
        </div>
      )}

      <div className="pixel-card p-6">
        <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-4">
          ▲ ADS-B AIRSPACE TARGET LOG
        </div>
        <div className="overflow-x-auto">
          <table className="w-full font-mono-pixel text-sm md:text-base">
            <thead className="text-[color:var(--as-neon)]/60 font-pixel text-[10px] border-b border-[color:var(--as-neon)]/20">
              <tr>
                <th className="text-left py-3">CALLSIGN</th>
                <th className="text-left">ALTITUDE</th>
                <th className="text-left">SPEED</th>
                <th className="text-left">HEADING</th>
                <th className="text-left">DISTANCE</th>
                <th className="text-left">POSITION</th>
                <th className="text-left">STATUS</th>
              </tr>
            </thead>
            <tbody>
              {flights.map((f, i) => {
                const altFt = Math.round(f.alt * 3.28084);
                const spdKt = Math.round(f.spd * 1.94384);
                const distKm = getDistanceKm(f.lat, f.lon);
                return (
                  <tr
                    key={i}
                    className="border-b border-[color:var(--as-neon)]/10 hover:bg-[color:var(--as-neon)]/5 transition-colors"
                  >
                    <td className="py-3 font-pixel text-xs text-[color:var(--as-yellow)]">
                      {f.callsign || "UNK"}
                    </td>
                    <td>
                      {altFt.toLocaleString()} ft (FL{Math.round(altFt / 100)})
                    </td>
                    <td>{spdKt} kt</td>
                    <td>
                      {Math.round(f.hdg)}° ({getCompass(f.hdg)})
                    </td>
                    <td>{distKm > 0 ? `${distKm.toFixed(1)} km` : "Center"}</td>
                    <td className="text-xs">
                      {f.lat.toFixed(4)}°N, {f.lon.toFixed(4)}°E
                    </td>
                    <td>
                      <span
                        className={`px-2 py-0.5 text-[10px] border ${
                          f.gnd
                            ? "border-[color:var(--as-orange)]/60 text-[color:var(--as-orange)]"
                            : "border-[color:var(--as-neon)]/60 text-[color:var(--as-neon)]"
                        }`}
                      >
                        {f.gnd ? "GROUND" : "AIRBORNE"}
                      </span>
                    </td>
                  </tr>
                );
              })}
              {flights.length === 0 && (
                <tr>
                  <td colSpan={7} className="py-6 text-center text-[color:var(--as-neon)]/40">
                    No active targets in zone.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>

      {/* Flight Debug Panel */}
      {devMode && (
        <div className="pixel-card p-4 border-red-500/30">
          <div className="flex items-center justify-between mb-3">
            <div className="font-pixel text-[10px] text-red-400">
              ▲ FLIGHT DEBUG PANEL
            </div>
            <button
              onClick={() => setShowDebug(!showDebug)}
              className="font-pixel text-[9px] px-3 py-1 border border-red-500/40 text-red-400 hover:bg-red-950/20"
            >
              {showDebug ? "HIDE" : "SHOW"} DEBUG
            </button>
          </div>

          <div className="grid grid-cols-2 md:grid-cols-4 gap-3 font-mono-pixel text-xs mb-3">
            <div className="bg-black/60 p-2 border border-red-500/20">
              <div className="text-red-400/60 text-[10px]">ESP32 STATUS</div>
              <div className={isConnected ? "text-green-400" : "text-[color:var(--as-yellow)] font-bold"}>
                {isConnected ? "CONNECTED" : "DEMO MODE ACTIVE"}
              </div>
            </div>
            <div className="bg-black/60 p-2 border border-red-500/20">
              <div className="text-red-400/60 text-[10px]">LAST PACKET</div>
              <div className="text-[color:var(--as-neon)]">{lastUpdated || "never"}</div>
            </div>
            <div className="bg-black/60 p-2 border border-red-500/20">
              <div className="text-red-400/60 text-[10px]">AIRCRAFT COUNT</div>
              <div className="text-[color:var(--as-yellow)] text-lg">{flights.length}</div>
            </div>
            <div className="bg-black/60 p-2 border border-red-500/20">
              <div className="text-red-400/60 text-[10px]">BBOX LOADED</div>
              <div className={bbox ? "text-green-400" : "text-[color:var(--as-neon)]"}>
                {bbox ? (isConnected ? "YES" : "DEMO BBOX") : "NO"}
              </div>
            </div>
          </div>

          {showDebug && (
            <div className="space-y-3">
              <div className="bg-black/60 p-3 border border-red-500/20 max-h-[200px] overflow-y-auto custom-scrollbar">
                <div className="text-red-400/60 text-[10px] font-pixel mb-2">EVENT LOG</div>
                {debugLog.length === 0 ? (
                  <div className="text-[color:var(--as-neon)]/30 text-xs font-mono-pixel">
                    Waiting for data...
                  </div>
                ) : (
                  debugLog.map((line, i) => (
                    <div key={i} className="text-[11px] font-mono-pixel text-[color:var(--as-neon)]/70 border-b border-red-500/10 py-0.5">
                      {line}
                    </div>
                  ))
                )}
              </div>
              <div className="bg-black/60 p-3 border border-red-500/20 max-h-[300px] overflow-y-auto custom-scrollbar">
                <div className="text-red-400/60 text-[10px] font-pixel mb-2">RAW AIRCRAFT JSON</div>
                <pre className="text-[10px] font-mono-pixel text-[color:var(--as-neon)]/60 whitespace-pre-wrap break-all">
                  {rawJson || "No flight data received yet."}
                </pre>
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
}

function ToolCard({
  icon,
  color,
  title,
  desc,
  btn,
  href,
  onClick,
}: {
  icon: string;
  color: string;
  title: string;
  desc: string;
  btn: string;
  href: string;
  onClick?: () => void;
}) {
  return (
    <div
      className="pixel-card p-5 flex flex-col h-full transition-all hover:-translate-y-1"
      style={{ boxShadow: `0 0 0 1px ${color}55, 0 10px 40px -10px ${color}33` }}
    >
      <div className="flex items-center gap-3">
        <span
          className="font-pixel text-lg w-10 h-10 grid place-items-center"
          style={{ background: `${color}22`, color, border: `1px solid ${color}66` }}
        >
          {icon}
        </span>
        <div className="font-pixel text-sm" style={{ color }}>
          {title}
        </div>
      </div>
      <p className="font-mono-pixel text-[color:var(--as-neon)]/80 text-lg mt-4 flex-1">{desc}</p>
      <a
        href={href}
        onClick={onClick}
        className="pixel-btn pixel-btn-ghost mt-5 text-center"
        style={{ color, borderColor: color, boxShadow: `4px 4px 0 ${color}33` }}
      >
        {btn}
      </a>
    </div>
  );
}

interface PetPanelProps {
  isConnected: boolean;
}

interface PetStatus {
  emotion: string;
  mood: string;
  activity: string;
  wifi: string;
  heap: number;
  fps: number;
  mode: number;
  flights: number;
  networks: number;
  coding: number;
  hours: number;
}

interface TimelineEvent {
  time: string;
  text: string;
  type: string;
}

function PetPanel({ isConnected }: PetPanelProps) {
  const [status, setStatus] = useState<PetStatus>({
    emotion: "calm",
    mood: "stable",
    activity: "idle",
    wifi: "disconnected",
    heap: 180000,
    fps: 30,
    mode: 0,
    flights: 0,
    networks: 0,
    coding: 0,
    hours: 0,
  });

  const [heapHistory, setHeapHistory] = useState<{ time: string; heap: number }[]>([]);
  const [timeline, setTimeline] = useState<TimelineEvent[]>([
    { time: new Date().toLocaleTimeString(), text: "System monitoring initialised.", type: "system" },
  ]);

  const lastStatusRef = useRef<PetStatus | null>(null);

  useEffect(() => {
    if (!isConnected) return;

    const removeListener = serialAPI.addListener((type: string, data: any) => {
      if (type === "RES") {
        if (data.emotion !== undefined) {
          const newStatus: PetStatus = {
            emotion: data.emotion,
            mood: data.mood,
            activity: data.activity,
            wifi: data.wifi,
            heap: data.heap,
            fps: data.fps,
            mode: data.mode,
            flights: data.flights || 0,
            networks: data.networks || 0,
            coding: data.coding || 0,
            hours: data.hours || 0,
          };

          setStatus(newStatus);

          setHeapHistory((prev) => [
            ...prev.slice(-29),
            { time: new Date().toLocaleTimeString().slice(-8), heap: data.heap / 1024 },
          ]);
        }

        if (data.timeline && Array.isArray(data.timeline)) {
          const events = data.timeline.map((evt: any) => {
            const sec = evt.time;
            const hrs = Math.floor(sec / 3600);
            const mins = Math.floor((sec % 3600) / 60);
            const secs = sec % 60;
            const timeStr = `${hrs.toString().padStart(2, '0')}:${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
            return {
              time: timeStr,
              text: evt.text,
              type: evt.type,
            };
          });
          setTimeline(events);
        }
      }
    });

    serialAPI.sendCommand("GET_PET_STATUS");
    serialAPI.sendCommand("GET_TIMELINE");

    const statusInterval = setInterval(() => {
      serialAPI.sendCommand("GET_PET_STATUS");
    }, 1500);

    const timelineInterval = setInterval(() => {
      serialAPI.sendCommand("GET_TIMELINE");
    }, 4500);

    return () => {
      clearInterval(statusInterval);
      clearInterval(timelineInterval);
      removeListener();
    };
  }, [isConnected]);

  // Simulation effect for Demo Mode
  useEffect(() => {
    if (isConnected) return;

    setStatus({
      emotion: "happy",
      mood: "stable",
      activity: "coding",
      wifi: "AeroSniffer-SEC",
      heap: 184320,
      fps: 30,
      mode: 0,
      flights: 2,
      networks: 14,
      coding: 45,
      hours: 12,
    });

    const nowStr = new Date().toLocaleTimeString();
    setTimeline([
      { time: nowStr, text: "Cyber-Pet consciousness linked in Demo Mode", type: "system" },
      { time: nowStr, text: "Began observing simulated desktop focus", type: "activity" }
    ]);

    const history = [];
    for (let i = 10; i >= 0; i--) {
      const d = new Date(Date.now() - i * 5000);
      history.push({
        time: d.toLocaleTimeString().slice(-8),
        heap: 180 + Math.round(Math.sin(i) * 3 + Math.random() * 2),
      });
    }
    setHeapHistory(history);

    const simInterval = setInterval(() => {
      const ts = new Date().toLocaleTimeString();
      
      setStatus((prev) => {
        const nextHeap = Math.round(180000 + Math.sin(Date.now() / 15000) * 4000 + Math.random() * 1500);
        const acts = ["coding", "typing", "thinking", "music"];
        const emos = ["happy", "idle", "excited", "thinking", "love"];
        const nextAct = acts[Math.floor(Math.random() * acts.length)];
        const nextEmo = emos[Math.floor(Math.random() * emos.length)];

        if (Math.random() > 0.5) {
          const events = [
            `Activity transition to: ${nextAct}`,
            `Emotion state set to: ${nextEmo}`,
            "Mock PC Agent reported active keystrokes",
            "Pet status heartbeat logged successfully"
          ];
          setTimeline((t) => [
            ...t.slice(-29),
            { time: ts, text: events[Math.floor(Math.random() * events.length)], type: "activity" }
          ]);
        }

        return {
          ...prev,
          emotion: nextEmo,
          activity: nextAct,
          heap: nextHeap,
        };
      });

      setHeapHistory((prev) => [
        ...prev.slice(-29),
        { time: ts.slice(-8), heap: (180000 + Math.sin(Date.now() / 15000) * 4000 + Math.random() * 1500) / 1024 },
      ]);
    }, 3000);

    return () => clearInterval(simInterval);
  }, [isConnected]);

  const getActivityDesc = (act: string) => {
    switch (act) {
      case "coding":
        return "Writing code in the IDE. High focus.";
      case "music":
        return "Listening to music. Happiness boosted.";
      case "typing":
        return "Actively typing. Busy/gaming.";
      case "thinking":
        return "Analyzing logs or editing media.";
      case "sleeping":
        return "Idle / inactive. Power saving.";
      default:
        return "Observing the desktop surroundings.";
    }
  };

  return (
    <div className="max-w-6xl mx-auto mt-10 space-y-6">
      {!isConnected && (
        <div className="pixel-card p-3 border-[color:var(--as-orange)]/50 bg-[color:var(--as-orange)]/5 font-pixel text-[9px] text-[color:var(--as-orange)] flex items-center justify-between">
          <span>▲ DEMO MODE: RUNNING SIMULATED COGNITIVE CORE TELEMETRY</span>
          <span className="animate-pulse">● SIMULATING</span>
        </div>
      )}
      <div className="grid md:grid-cols-3 gap-6">
        <div className="pixel-card p-6 border-[color:var(--as-pink)] flex flex-col justify-between">
          <div>
            <div className="font-pixel text-[10px] text-[color:var(--as-pink)] mb-4">
              ▲ EMOTION ENGINE STATE
            </div>
            <div className="space-y-4">
              <div>
                <span className="font-pixel text-[10px] text-[color:var(--as-neon)]/60 block mb-1">
                  EMOTION
                </span>
                <span className="font-pixel text-2xl text-[color:var(--as-pink)] uppercase glow-pink">
                  {status.emotion}
                </span>
              </div>
              <div>
                <span className="font-pixel text-[10px] text-[color:var(--as-neon)]/60 block mb-1">
                  MOOD ENGINE
                </span>
                <span className="font-mono-pixel text-lg text-[color:var(--as-neon)] capitalize">
                  {status.mood}
                </span>
              </div>
              <div>
                <span className="font-pixel text-[10px] text-[color:var(--as-neon)]/60 block mb-1">
                  ACTIVITY CONTEXT
                </span>
                <span className="font-pixel text-xs text-[color:var(--as-violet)] block mb-1 uppercase">
                  ● {status.activity}
                </span>
                <span className="font-mono-pixel text-sm text-[color:var(--as-neon)]/80">
                  {getActivityDesc(status.activity)}
                </span>
              </div>
            </div>
          </div>
          <div className="border-t border-[color:var(--as-neon)]/15 pt-4 mt-6 font-mono-pixel text-xs text-[color:var(--as-neon)]/50">
            double-buffered renderer running
          </div>
        </div>

        <div className="pixel-card p-6 border-[color:var(--as-violet)]">
          <div className="font-pixel text-[10px] text-[color:var(--as-violet)] mb-4">
            ▲ MEMORY ENGINE COGNITION
          </div>
          <div className="grid grid-cols-2 gap-4 font-mono-pixel text-[color:var(--as-neon)]/80">
            <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/10">
              <span className="text-[color:var(--as-neon)]/50 text-[10px] block mb-1">
                FLIGHTS SEEN
              </span>
              <span className="text-xl text-[color:var(--as-yellow)] font-pixel">
                {status.flights}
              </span>
            </div>
            <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/10">
              <span className="text-[color:var(--as-neon)]/50 text-[10px] block mb-1">
                NETWORKS SEEN
              </span>
              <span className="text-xl text-[color:var(--as-orange)] font-pixel">
                {status.networks}
              </span>
            </div>
            <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/10">
              <span className="text-[color:var(--as-neon)]/50 text-[10px] block mb-1">
                CODING TIME
              </span>
              <span className="text-sm text-[color:var(--as-neon)] font-mono-pixel">
                {status.coding} mins
              </span>
            </div>
            <div className="bg-[#06080e] p-3 border border-[color:var(--as-neon)]/10">
              <span className="text-[color:var(--as-neon)]/50 text-[10px] block mb-1">
                ACTIVE TIME
              </span>
              <span className="text-sm text-[color:var(--as-neon)] font-mono-pixel">
                {status.hours} hours
              </span>
            </div>
          </div>
          <div className="mt-4 p-3 bg-black/40 border border-[color:var(--as-violet)]/20 font-mono-pixel text-xs text-[color:var(--as-neon)]/70">
            Cognitive stats are loaded dynamically from ESP32 flash memory NVS partition.
          </div>
        </div>

        <div className="pixel-card p-6 flex flex-col justify-between">
          <div>
            <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-4">
              ▲ HARDWARE TELEMETRY
            </div>
            <div className="space-y-3 font-mono-pixel text-sm">
              <div className="flex justify-between border-b border-[color:var(--as-neon)]/10 pb-1">
                <span className="text-[color:var(--as-neon)]/50">WiFi Network</span>
                <span>{status.wifi}</span>
              </div>
              <div className="flex justify-between border-b border-[color:var(--as-neon)]/10 pb-1">
                <span className="text-[color:var(--as-neon)]/50">Free Heap</span>
                <span>{Math.round(status.heap / 1024)} KB</span>
              </div>
              <div className="flex justify-between border-b border-[color:var(--as-neon)]/10 pb-1">
                <span className="text-[color:var(--as-neon)]/50">Render Rate</span>
                <span>{status.fps} FPS</span>
              </div>
              <div className="flex justify-between pb-1">
                <span className="text-[color:var(--as-neon)]/50">Active Partition</span>
                <span>Mode {status.mode}</span>
              </div>
            </div>
          </div>
          <div className="h-[100px] w-full font-mono-pixel text-[8px] mt-4">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={heapHistory}>
                <YAxis hide={true} domain={["dataMin - 5", "dataMax + 5"]} />
                <Tooltip contentStyle={{ backgroundColor: "#06080e", borderColor: "var(--as-neon)" }} />
                <Line type="monotone" dataKey="heap" stroke="var(--as-neon)" strokeWidth={1} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </div>
      </div>

      <div className="grid md:grid-cols-2 gap-6">
        <div className="pixel-card p-5">
          <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-4">
            ▲ REAL-TIME HEAP ALLOCATION MONITOR (KB)
          </div>
          <div className="h-[250px] w-full font-mono-pixel text-xs">
            {heapHistory.length > 0 ? (
              <ResponsiveContainer width="100%" height="100%">
                <AreaChart data={heapHistory} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                  <defs>
                    <linearGradient id="heapGrad" x1="0" y1="0" x2="0" y2="1">
                      <stop offset="5%" stopColor="var(--as-neon)" stopOpacity={0.4} />
                      <stop offset="95%" stopColor="var(--as-neon)" stopOpacity={0} />
                    </linearGradient>
                  </defs>
                  <XAxis dataKey="time" stroke="var(--as-neon)" opacity={0.6} />
                  <YAxis
                    stroke="var(--as-neon)"
                    opacity={0.6}
                    domain={["dataMin - 5", "dataMax + 5"]}
                  />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: "#06080e",
                      borderColor: "var(--as-neon)",
                      color: "var(--as-neon)",
                    }}
                  />
                  <CartesianGrid strokeDasharray="3 3" stroke="var(--as-neon)" opacity={0.1} />
                  <Area
                    type="monotone"
                    dataKey="heap"
                    stroke="var(--as-neon)"
                    fillOpacity={1}
                    fill="url(#heapGrad)"
                  />
                </AreaChart>
              </ResponsiveContainer>
            ) : (
              <div className="h-full flex items-center justify-center text-[color:var(--as-neon)]/50">
                Waiting for telemetry stream...
              </div>
            )}
          </div>
        </div>

        <div className="pixel-card p-5 flex flex-col h-[350px]">
          <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-4">
            ▲ LOCAL COGNITIVE EVENT TIMELINE
          </div>
          <div className="overflow-y-auto flex-1 font-mono-pixel text-sm space-y-2 pr-2 custom-scrollbar">
            {timeline
              .slice()
              .reverse()
              .map((evt, i) => (
                <div key={i} className="border-b border-[color:var(--as-neon)]/10 pb-1">
                  <span className="text-[color:var(--as-neon)]/40">[{evt.time}] </span>
                  <span
                    className={
                      evt.type === "flight"
                        ? "text-[color:var(--as-yellow)]"
                        : evt.type === "network"
                          ? "text-[color:var(--as-orange)]"
                          : evt.type === "emotion"
                            ? "text-[color:var(--as-pink)]"
                            : evt.type === "activity"
                              ? "text-[color:var(--as-violet)]"
                              : "text-[color:var(--as-neon)]/80"
                    }
                  >
                    {evt.text}
                  </span>
                </div>
              ))}
          </div>
          <div className="mt-4 pt-3 border-t border-[color:var(--as-neon)]/10 font-mono-pixel text-xs text-[color:var(--as-neon)]/60">
            Summary: Today you spent {status.coding} minutes focused in development mode, detecting{" "}
            {status.flights} flight targets.
          </div>
        </div>
      </div>
    </div>
  );
}

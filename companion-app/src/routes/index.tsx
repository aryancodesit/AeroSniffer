import { createFileRoute } from "@tanstack/react-router";
import { useEffect, useRef, useState } from "react";
import { BotFace, FACE_META, type FaceState } from "@/components/BotFace";
import { PixelBackdrop } from "@/components/PixelBackdrop";
import { LiveRadarPanel } from "@/components/LiveRadarPanel";
import { serialAPI } from "@/lib/serial";

export const Route = createFileRoute("/")({
  head: () => ({
    meta: [
      { title: "AeroSniffer — Pixel Companion" },
      {
        name: "description",
        content:
          "Meet AeroSniffer: a multi-boot ESP32-S3 desk gadget. Cyber-Pet, Network Auditor, and Flight Radar — in one tiny pixel-faced friend.",
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

const FACE_ORDER: FaceState[] = [
  "startup",
  "idle",
  "happy",
  "excited",
  "surprised",
  "thinking",
  "love",
  "alert",
  "sleepy",
  "sad",
];

function Index() {
  const [face, setFace] = useState<FaceState>("startup");
  const [booted, setBooted] = useState(false);
  const [bootLines, setBootLines] = useState<string[]>([]);
  const [mode, setMode] = useState<0 | 1 | 2 | 3 | 4>(0);
  const [isConnected, setIsConnected] = useState(false);
  const heroRef = useRef<HTMLDivElement>(null);

  // Boot sequence → idle
  useEffect(() => {
    const lines = [
      "> XIAO_ESP32S3  boot   OK",
      "> FreeRTOS     init   OK",
      "> AeroShell 2.0  OK",
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
                  <span className="neon-text">● {FACE_META[face].label.toUpperCase()}</span>
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
                ┌── AEROSHELL 2.0 ──┐
              </div>
              <h1 className="font-pixel neon-text text-3xl md:text-5xl leading-tight">
                AERO
                <br />
                SNIFFER
              </h1>
              <p className="font-mono-pixel mt-4 text-[color:var(--as-neon)]/80 text-xl md:text-2xl max-w-md">
                Three devices, one pixel soul. A multi-boot ESP32-S3 friend that lives on your desk
                and remembers your face.
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
                <a href="#modes" className="pixel-btn">
                  CHOOSE A MODE
                </a>
                <button onClick={() => document.querySelector('nav button')?.dispatchEvent(new MouseEvent('click', { bubbles: true }))} className="pixel-btn pixel-btn-ghost text-[color:var(--as-yellow)] border-[color:var(--as-yellow)]">
                  ⚙️ FIRST BOOT SETUP
                </button>
              </div>
            </div>
          </div>

          {/* marquee */}
          <Marquee />
        </section>

        {/* MODES */}
        <section id="modes" className="py-20 px-6">
          <SectionHeader kicker="01 // OPERATING MODES" title="THREE SOULS, ONE SHELL" />
          <div className="max-w-6xl mx-auto grid md:grid-cols-3 gap-5 mt-10">
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
              name="Network Auditor"
              face="alert"
              desc="802.11 packet sniffer with an animated radar sweep + companion web app for full security control. Plug in, scan, learn."
              tags={["WiFi sniffer", "Payload UI", "Web Serial"]}
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
              name="Flight Radar"
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

          {/* Mode 2 radar visual */}
          {mode === 1 && <LiveRadarPanel />}
          {mode === 2 && <FlightPanel />}
          {mode === 0 && <PetPanel />}
          {mode === 3 && <PayloadPanel />}
          {mode === 4 && <TutorialPanel />}
        </section>

        {/* FACES GALLERY */}
        <section id="faces" className="py-20 px-6 border-t border-[color:var(--as-neon)]/15">
          <SectionHeader kicker="02 // EMOTION REGISTER" title="HIS LITTLE FEELINGS" />
          <p className="max-w-2xl mx-auto text-center font-mono-pixel text-[color:var(--as-neon)]/70 text-lg mt-3">
            Ten states. Three groups. Click one — he'll wear it. Each face is a 240×240 .bmp drawn
            from a 16-bit RGB565 palette.
          </p>

          {(["PASSIVE", "REACTIVE", "SYSTEM"] as const).map((g) => (
            <div key={g} className="max-w-6xl mx-auto mt-12">
              <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/60 mb-4">
                ▲ {g} STATES
              </div>
              <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 gap-4">
                {FACE_ORDER.filter((f) => FACE_META[f].group === g).map((f) => (
                  <button
                    key={f}
                    onClick={() => {
                      setFace(f);
                      if (isConnected) {
                        const WEB_TO_ESP_FACE: Record<string, string> = {
                          startup: "STARTUP_BOOT",
                          idle: "IDLE",
                          happy: "HAPPY",
                          excited: "EXCITED",
                          surprised: "SURPRISED",
                          thinking: "THINKING",
                          love: "LOVE_BONDING",
                          alert: "ALERT_WARNING",
                          sleepy: "SLEEPY",
                          sad: "SAD_ERROR",
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
                            boxShadow: `0 0 0 2px ${FACE_META[f].color}, 0 0 28px ${FACE_META[f].color}66`,
                          }
                        : undefined
                    }
                  >
                    <BotFace state={f} size={220} />
                    <div className="mt-3 font-pixel text-xs" style={{ color: FACE_META[f].color }}>
                      {FACE_META[f].label}
                    </div>
                    <div className="font-mono-pixel text-[color:var(--as-neon)]/60 text-base mt-1">
                      {FACE_META[f].sub}
                    </div>
                  </button>
                ))}
              </div>
            </div>
          ))}

          {/* palette */}
          <div className="max-w-6xl mx-auto mt-14 pixel-card p-5">
            <div className="font-pixel text-[10px] text-[color:var(--as-neon)]/70 mb-3">
              display: 240×240px · palette: 16-bit RGB565 · all faces: 240×240 .bmp
            </div>
            <div className="flex flex-wrap gap-3 font-mono-pixel text-base">
              {Object.entries(FACE_META).map(([k, v]) => (
                <div key={k} className="flex items-center gap-2">
                  <span className="inline-block w-4 h-4" style={{ background: v.color }} />
                  <span style={{ color: v.color }}>{v.label}</span>
                </div>
              ))}
            </div>
          </div>
        </section>

        {/* SPECS */}
        <section id="companion" className="py-20 px-6 border-t border-[color:var(--as-neon)]/15">
          <SectionHeader kicker="03 // COMPANION TOOLS" title="TALK TO YOUR BOT" />
          <p className="max-w-2xl mx-auto text-center font-mono-pixel text-[color:var(--as-neon)]/70 text-lg mt-3">
            Four pixel terminals to drive AeroSniffer over USB-C / Web Serial. Plug him in, pick a
            console.
          </p>
          <div className="max-w-6xl mx-auto grid sm:grid-cols-2 lg:grid-cols-4 gap-5 mt-10">
            <ToolCard
              icon="⚡"
              color="var(--as-neon)"
              title="CONNECT & SCAN"
              desc="Launch the web-based Security Monitor dashboard."
              btn="LAUNCH DASHBOARD"
              onClick={() => setMode(1)}
              href="#modes"
            />
            <ToolCard
              icon="◎"
              color="var(--as-orange)"
              title="PAYLOADS"
              desc="Library of pre-configured scans and automated workflows."
              btn="VIEW PAYLOADS"
              onClick={() => setMode(3)}
              href="#modes"
            />
            <ToolCard
              icon="▤"
              color="var(--as-violet)"
              title="TUTORIAL"
              desc="Learn how to use all three modes of AeroSniffer."
              btn="READ GUIDE"
              onClick={() => setMode(4)}
              href="#modes"
            />
            <ToolCard
              icon="↻"
              color="var(--as-yellow)"
              title="UPDATES"
              desc="Check for firmware updates and flash via Web Serial."
              btn="CHECK UPDATES"
              href="#specs"
            />
          </div>
        </section>

        {/* SPECS */}
        <section id="specs" className="py-20 px-6 border-t border-[color:var(--as-neon)]/15">
          <SectionHeader kicker="03 // HARDWARE" title="WHAT'S INSIDE" />
          <div className="max-w-5xl mx-auto grid md:grid-cols-2 gap-5 mt-10">
            {[
              ["MCU", "XIAO ESP32-S3"],
              ["OS", "FreeRTOS multi-task"],
              ["Display", "240×240 OLED · RGB565"],
              ["Input", "Capacitive touch · 1.5s long-press"],
              ["Radios", "Wi-Fi 802.11 · monitor mode"],
              ["Power", "USB-C · LiPo standby"],
              ["Shell", "AeroShell 2.0 enclosure (FDM)"],
              ["Modes", "Cyber-Pet · Auditor · Radar"],
            ].map(([k, v]) => (
              <div key={k} className="pixel-card p-4 flex items-center justify-between">
                <span className="font-pixel text-[10px] text-[color:var(--as-neon)]/70">{k}</span>
                <span className="font-mono-pixel text-[color:var(--as-neon)] text-lg">{v}</span>
              </div>
            ))}
          </div>
        </section>

        {/* FOOTER */}
        <footer className="border-t border-[color:var(--as-neon)]/20 px-6 py-10">
          <div className="max-w-6xl mx-auto flex flex-wrap items-center justify-between gap-4">
            <div className="flex items-center gap-3">
              <BotFace state="idle" size={56} />
              <div>
                <div className="font-pixel text-xs neon-text">AeroSniffer</div>
                <div className="font-mono-pixel text-[color:var(--as-neon)]/60 text-sm">
                  pixel companion · v1.0
                </div>
              </div>
            </div>
            <div className="font-mono-pixel text-[color:var(--as-neon)]/60 text-base">
              Ensure your device is plugged in via USB-C · set Mode 2 for Security.
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
}

function Nav({ onFace, isConnected, setIsConnected }: NavProps) {
  const [showSetup, setShowSetup] = useState(false);
  const [deviceData, setDeviceData] = useState<any>(null);
  const [wifi, setWifi] = useState({ ssid: "", pass: "" });
  const [bbox, setBbox] = useState({ lamin: "", lomin: "", lamax: "", lomax: "" });
  const [setupMsg, setSetupMsg] = useState("");
  const [searchQuery, setSearchQuery] = useState("");
  const [isSearching, setIsSearching] = useState(false);

  useEffect(() => {
    serialAPI.onMessage = (type: string, data: any) => {
      if (type === "RES" && (data.fw || data.lamin !== undefined)) {
        setDeviceData(data);
        if (data.lamin !== undefined && !bbox.lamin) {
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
      }
    };
    serialAPI.onDisconnect = () => setIsConnected(false);
  }, [bbox.lamin]);

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
    setSetupMsg("Saved! Rebooting robot...");
    setTimeout(() => {
      serialAPI.sendCommand("REBOOT");
      serialAPI.disconnect();
      setSetupConnected(false);
      setShowSetup(false);
    }, 1000);
  };

  const items: { label: string; href: string; face: FaceState }[] = [
    { label: "modes", href: "#modes", face: "excited" },
    { label: "faces", href: "#faces", face: "happy" },
    { label: "specs", href: "#specs", face: "thinking" },
  ];
  return (
    <>
      <nav className="sticky top-0 z-40 backdrop-blur-md bg-[#06080e]/70 border-b border-[color:var(--as-neon)]/20">
        <div className="max-w-6xl mx-auto flex items-center justify-between px-6 py-3">
          <a href="#" className="flex items-center gap-3">
            <BotFace state="idle" size={44} />
            <span className="font-pixel text-xs neon-text hidden sm:inline">AeroSniffer</span>
          </a>
          <ul className="flex items-center gap-1 sm:gap-4">
            {items.map((i) => (
              <li key={i.label}>
                <a
                  href={i.href}
                  onMouseEnter={() => onFace(i.face)}
                  className="font-pixel text-[10px] text-[color:var(--as-neon)]/70 hover:text-[color:var(--as-neon)] px-3 py-2"
                >
                  {i.label.toUpperCase()}
                </a>
              </li>
            ))}
            <li>
              <button
                onClick={() => setShowSetup(true)}
                className="font-pixel text-[10px] text-[color:var(--as-yellow)] hover:underline px-3 py-2 border border-[color:var(--as-yellow)]"
                onMouseEnter={() => onFace("alert")}
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
    "✦ AeroShell 2.0",
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

function RadarPanel() {
  return (
    <div className="max-w-6xl mx-auto mt-10 pixel-card p-6">
      <div className="font-pixel text-[10px] text-[color:var(--as-orange)] mb-4">
        ▲ MODE 2 · NETWORK AUDITOR · live mock
      </div>
      <div className="grid md:grid-cols-2 gap-6 items-center">
        <div className="relative aspect-square max-w-[320px] mx-auto">
          <div className="absolute inset-0 rounded-full border-2 border-[color:var(--as-neon)]/40" />
          <div className="absolute inset-6 rounded-full border border-[color:var(--as-neon)]/30" />
          <div className="absolute inset-12 rounded-full border border-[color:var(--as-neon)]/20" />
          <div className="absolute inset-0 radar-sweep">
            <div
              className="absolute top-1/2 left-1/2 h-1/2 w-1/2 origin-top-left"
              style={{
                background: "conic-gradient(from 0deg, rgba(74,240,188,0.55), transparent 30%)",
              }}
            />
          </div>
          {[
            [20, 30],
            [70, 40],
            [55, 75],
            [35, 60],
            [80, 65],
          ].map(([x, y], i) => (
            <div
              key={i}
              className="absolute w-2 h-2 bg-[color:var(--as-neon)]"
              style={{ left: `${x}%`, top: `${y}%`, boxShadow: "0 0 10px var(--as-neon)" }}
            />
          ))}
        </div>
        <div className="font-mono-pixel text-lg space-y-1 text-[color:var(--as-neon)]/85">
          <div>[scan] channel 1 … 4 APs</div>
          <div>[scan] channel 6 … 11 APs</div>
          <div>[scan] channel 11 … 7 APs</div>
          <div className="text-[color:var(--as-orange)]">[!] deauth burst detected</div>
          <div>[ok] handshake captured</div>
          <div className="text-[color:var(--as-neon)]/60">// long-press to swap modes</div>
        </div>
      </div>
    </div>
  );
}

function FlightPanel() {
  const flights = [
    ["AIC127", "FL340", "478kt", "087°"],
    ["UAE205", "FL380", "510kt", "271°"],
    ["BAW19", "FL310", "462kt", "315°"],
    ["QFA9", "FL400", "499kt", "060°"],
  ];
  return (
    <div className="max-w-6xl mx-auto mt-10 pixel-card p-6">
      <div className="font-pixel text-[10px] text-[color:var(--as-yellow)] mb-4">
        ▲ MODE 3 · FLIGHT RADAR · ADS-B feed
      </div>
      <table className="w-full font-mono-pixel text-lg">
        <thead className="text-[color:var(--as-neon)]/60 font-pixel text-[10px]">
          <tr>
            <th className="text-left py-2">CALLSIGN</th>
            <th className="text-left">ALT</th>
            <th className="text-left">SPD</th>
            <th className="text-left">HDG</th>
          </tr>
        </thead>
        <tbody>
          {flights.map((f) => (
            <tr key={f[0]} className="border-t border-[color:var(--as-neon)]/15">
              <td className="py-2 text-[color:var(--as-yellow)]">{f[0]}</td>
              <td>{f[1]}</td>
              <td>{f[2]}</td>
              <td>{f[3]}</td>
            </tr>
          ))}
        </tbody>
      </table>
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

function PetPanel() {
  return (
    <div className="max-w-6xl mx-auto mt-10 pixel-card p-6 grid md:grid-cols-3 gap-6 items-center">
      <div className="font-pixel text-[10px] text-[color:var(--as-pink)]">
        ▲ MODE 1 · CYBER-PET · live
      </div>
      <div className="font-mono-pixel text-lg text-[color:var(--as-neon)]/85 md:col-span-2 space-y-1">
        <div>[host] typing detected → face: happy</div>
        <div>[host] CPU 12% · idle</div>
        <div>[host] new tab opened → face: surprised</div>
        <div className="text-[color:var(--as-pink)]">[host] you've been here 2h — face: love</div>
      </div>
    </div>
  );
}

function PayloadPanel() {
  const attacks = [
    {
      name: "Beacon Spam",
      desc: "Flood SSIDs to confuse nearby devices",
      cmd: "attack -t beacon -l",
    },
    { name: "Deauth All", desc: "Disconnect all clients from APs", cmd: "attack -t deauth -c" },
    { name: "Probe Request", desc: "Send random probe requests", cmd: "attack -t probe" },
    { name: "Rick Roll", desc: "Broadcast Rick Astley lyrics as APs", cmd: "attack -t rickroll" },
  ];
  return (
    <div className="max-w-6xl mx-auto mt-10 pixel-card p-6 border-[color:var(--as-orange)]">
      <div className="font-pixel text-[10px] text-[color:var(--as-orange)] mb-4">
        ▲ AERO-PAYLOAD ENGINE · OPTIONS
      </div>
      <div className="grid md:grid-cols-2 lg:grid-cols-4 gap-4 mt-6">
        {attacks.map((atk, i) => (
          <div
            key={i}
            className="bg-[#06080e] p-4 border border-[color:var(--as-orange)]/30 hover:border-[color:var(--as-orange)] transition-colors"
          >
            <h4 className="font-pixel text-xs text-[color:var(--as-orange)] mb-2">{atk.name}</h4>
            <p className="font-mono-pixel text-[color:var(--as-neon)]/70 text-sm mb-4 h-10">
              {atk.desc}
            </p>
            <div className="font-mono-pixel text-[10px] bg-black p-2 rounded text-[color:var(--as-pink)] mb-3">
              {atk.cmd}
            </div>
            <button
              onClick={async () => {
                if (isConnected) {
                  try {
                    await serialAPI.sendCommand(`ATTACK:${atk.name.toUpperCase().replace(" ", "_")}`);
                    alert(`Payload executed: ${atk.name}`);
                  } catch (e) {
                    console.error("Payload failed", e);
                  }
                } else {
                  alert("Please connect via Web Serial first using the setup button.");
                }
              }}
              className="w-full pixel-btn pixel-btn-ghost text-[9px] py-2"
              style={{ color: "var(--as-orange)", borderColor: "var(--as-orange)" }}
            >
              EXECUTE PAYLOAD
            </button>
          </div>
        ))}
      </div>
    </div>
  );
}

function TutorialPanel() {
  return (
    <div className="max-w-6xl mx-auto mt-10 pixel-card p-8">
      <div className="font-pixel text-[10px] text-[color:var(--as-violet)] mb-6">
        ▲ TUTORIAL · HOW TO TRAIN YOUR AEROSNIFFER
      </div>

      <div className="grid md:grid-cols-2 gap-10">
        <div className="space-y-6">
          <div className="bg-[#06080e] p-5 border border-[color:var(--as-violet)]/30">
            <h3 className="font-pixel text-sm text-[color:var(--as-violet)] mb-3">1. BOOTING UP</h3>
            <p className="font-mono-pixel text-lg text-[color:var(--as-neon)]/80">
              Plug the AeroSniffer into your PC using a USB-C data cable. The ESP32-S3 will
              initialize FreeRTOS and load the Cyber-Pet face by default.
            </p>
          </div>

          <div className="bg-[#06080e] p-5 border border-[color:var(--as-violet)]/30">
            <h3 className="font-pixel text-sm text-[color:var(--as-violet)] mb-3">
              2. SWITCHING MODES
            </h3>
            <p className="font-mono-pixel text-lg text-[color:var(--as-neon)]/80">
              Tap the capacitive touch pad on the top of the enclosure to cycle through faces.{" "}
              <strong className="text-[color:var(--as-pink)]">Long-press for 1.5 seconds</strong> to
              reboot into the next operating system (Cyber-Pet → Auditor → Radar).
            </p>
          </div>
        </div>

        <div className="space-y-6">
          <div className="bg-[#06080e] p-5 border border-[color:var(--as-violet)]/30">
            <h3 className="font-pixel text-sm text-[color:var(--as-violet)] mb-3">3. PC AGENT</h3>
            <p className="font-mono-pixel text-lg text-[color:var(--as-neon)]/80">
              To enable dynamic expressions, double-click the{" "}
              <code className="text-[color:var(--as-pink)]">Start_AeroSniffer.bat</code> file. It
              will silently track your typing, CPU load, and active windows in the background,
              bringing your pet to life! (Add a shortcut to{" "}
              <code className="text-[color:var(--as-pink)]">shell:startup</code> for auto-boot).
            </p>
          </div>

          <div className="bg-[#06080e] p-5 border border-[color:var(--as-violet)]/30">
            <h3 className="font-pixel text-sm text-[color:var(--as-violet)] mb-3">
              4. WEB SERIAL DASHBOARD
            </h3>
            <p className="font-mono-pixel text-lg text-[color:var(--as-neon)]/80">
              When in Mode 2 (Network Auditor), open the "Connect & Scan" tool on this website.
              Click "Connect", select the COM port of your ESP32, and you'll immediately see live
              packets and access points streaming onto the dashboard!
            </p>
          </div>
        </div>
      </div>
    </div>
  );
}

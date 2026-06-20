import { useEffect, useRef, useState } from "react";

export type FaceState =
  // Core Emotions
  | "idle"
  | "happy"
  | "excited"
  | "sleepy"
  | "thinking"
  | "sad"
  | "surprised"
  | "love"
  // Security
  | "sec_scanning"
  | "sec_intrusion"
  // Aviation
  | "avi_radar"
  | "avi_lock"
  | "avi_disabled"
  // System
  | "sys_boot"
  | "sys_prefs"
  | "sys_error"
  // Secret
  | "sec_matrix"
  | "sec_retro"
  | "sec_rainbow";

export const FACE_META: Record<
  FaceState,
  { label: string; sub: string; color: string; group: string }
> = {
  // Core Emotions
  idle: { label: "idle", sub: "neutral / standby", color: "#38e1ff", group: "CORE EMOTIONS" },
  happy: {
    label: "happy",
    sub: "positive action / greeting",
    color: "#4af0bc",
    group: "CORE EMOTIONS",
  },
  excited: {
    label: "excited",
    sub: "new connection / task",
    color: "#ffd23f",
    group: "CORE EMOTIONS",
  },
  sleepy: {
    label: "sleepy",
    sub: "idle timeout / low battery",
    color: "#6d8cff",
    group: "CORE EMOTIONS",
  },
  thinking: {
    label: "thinking",
    sub: "processing / waiting",
    color: "#b86bff",
    group: "CORE EMOTIONS",
  },
  sad: { label: "sad", sub: "disconnect / task fail", color: "#ff5466", group: "CORE EMOTIONS" },
  surprised: {
    label: "surprised",
    sub: "new notification / drop",
    color: "#e8fff5",
    group: "CORE EMOTIONS",
  },
  love: { label: "love", sub: "paired device / uptime", color: "#ff4fd8", group: "CORE EMOTIONS" },

  // Security
  sec_scanning: {
    label: "scanning",
    sub: "sweeping channels",
    color: "#ff8a3d",
    group: "SECURITY",
  },
  sec_intrusion: {
    label: "intrusion",
    sub: "network anomaly / unknown device",
    color: "#ff3b30",
    group: "SECURITY",
  },

  // Aviation
  avi_radar: {
    label: "radar sweep",
    sub: "tracking ADS-B airspace",
    color: "#ffcc00",
    group: "AVIATION",
  },
  avi_lock: { label: "target locked", sub: "icao24 lock-on", color: "#00e5ff", group: "AVIATION" },
  avi_disabled: {
    label: "aviation disabled",
    sub: "aviation mode is toggled off",
    color: "#ff453a",
    group: "AVIATION",
  },

  // System
  sys_boot: { label: "booting", sub: "loading kernel modules", color: "#a8b3cf", group: "SYSTEM" },
  sys_prefs: { label: "preferences", sub: "modifying settings", color: "#af52de", group: "SYSTEM" },
  sys_error: {
    label: "system error",
    sub: "hardware exception",
    color: "#ff453a",
    group: "SYSTEM",
  },

  // Secret
  sec_matrix: { label: "matrix", sub: "digital rain stream", color: "#30d158", group: "SECRET" },
  sec_retro: {
    label: "retro game",
    sub: "space invader arcade",
    color: "#bf5af2",
    group: "SECRET",
  },
  sec_rainbow: { label: "rainbow", sub: "chroma color cycle", color: "#ff375f", group: "SECRET" },
};

interface BotFaceProps {
  state: FaceState;
  size?: number;
  followCursor?: boolean;
  className?: string;
}

/**
 * Pixel-art face for AeroSniffer. Built on a 24x24 logical grid then scaled.
 * Eyes can track the cursor (+/- 1px) in passive states.
 */
export function BotFace({ state, size = 280, followCursor = false, className }: BotFaceProps) {
  const c = FACE_META[state]?.color || "#38e1ff";
  const ref = useRef<HTMLDivElement>(null);
  const [offset, setOffset] = useState({ x: 0, y: 0 });
  const [blink, setBlink] = useState(false);

  useEffect(() => {
    if (!followCursor) return;
    const onMove = (e: MouseEvent) => {
      const el = ref.current;
      if (!el) return;
      const r = el.getBoundingClientRect();
      const cx = r.left + r.width / 2;
      const cy = r.top + r.height / 2;
      const dx = (e.clientX - cx) / window.innerWidth;
      const dy = (e.clientY - cy) / window.innerHeight;
      const max = 1.6;
      setOffset({
        x: Math.max(-max, Math.min(max, dx * 4)),
        y: Math.max(-max, Math.min(max, dy * 4)),
      });
    };
    window.addEventListener("mousemove", onMove);
    return () => window.removeEventListener("mousemove", onMove);
  }, [followCursor]);

  useEffect(() => {
    const t = setInterval(
      () => {
        setBlink(true);
        setTimeout(() => setBlink(false), 140);
      },
      3200 + Math.random() * 1800,
    );
    return () => clearInterval(t);
  }, []);

  const eyeOffX = ["idle", "happy", "love", "surprised", "sec_rainbow"].includes(state)
    ? offset.x
    : 0;
  const eyeOffY = ["idle", "happy", "love", "surprised", "sec_rainbow"].includes(state)
    ? offset.y
    : 0;

  return (
    <div
      ref={ref}
      className={className}
      style={{
        width: size,
        height: size * (16 / 24),
        imageRendering: "pixelated",
      }}
    >
      <svg
        viewBox="0 0 24 16"
        width="100%"
        height="100%"
        shapeRendering="crispEdges"
        style={{
          imageRendering: "pixelated",
          boxShadow: `0 0 20px rgba(0,0,0,0.8)`,
          border: `1px solid rgba(255,255,255,0.05)`,
        }}
      >
        <defs>
          <style>{`
            @keyframes pulse {
              0%, 100% { opacity: 0.3; }
              50% { opacity: 1; }
            }
            @keyframes float-sweat {
              0% { transform: translateY(0); opacity: 0; }
              10% { opacity: 1; }
              90% { opacity: 1; }
              100% { transform: translateY(6px); opacity: 0; }
            }
            @keyframes matrix-rain {
              0% { transform: translateY(-16px); }
              100% { transform: translateY(16px); }
            }
            @keyframes sweep-line {
              0% { transform: translateY(0); }
              50% { transform: translateY(16px); }
              100% { transform: translateY(0); }
            }
            @keyframes rotate {
              0% { transform: rotate(0deg); }
              100% { transform: rotate(360deg); }
            }
            @keyframes float-z {
              0% { transform: translate(0, 0); opacity: 0; }
              20% { opacity: 1; }
              80% { opacity: 1; }
              100% { transform: translate(4px, -6px); opacity: 0; }
            }
            @keyframes slide-knob {
              0%, 100% { transform: translateX(0); }
              50% { transform: translateX(4px); }
            }
            @keyframes bounce {
              0%, 100% { transform: translateY(0); }
              50% { transform: translateY(-2px); }
            }
            @keyframes rainbow-cycle {
              0% { fill: #ff3b30; stroke: #ff3b30; }
              17% { fill: #ff9500; stroke: #ff9500; }
              33% { fill: #ffcc00; stroke: #ffcc00; }
              50% { fill: #4cd964; stroke: #4cd964; }
              67% { fill: #5ac8fa; stroke: #5ac8fa; }
              83% { fill: #5856d6; stroke: #5856d6; }
              100% { fill: #ff3b30; stroke: #ff3b30; }
            }
            @keyframes loading {
              0% { width: 0; }
              100% { width: 14px; }
            }
            .anim-pulse { animation: pulse 1s infinite; }
            .anim-sweat { animation: float-sweat 2s infinite ease-in-out; }
            .anim-matrix { animation: matrix-rain 2s infinite linear; }
            .anim-sweep { animation: sweep-line 3s infinite linear; }
            .anim-rotate { animation: rotate 4s infinite linear; transform-origin: 12px 8px; }
            .anim-z1 { animation: float-z 3s infinite linear; }
            .anim-z2 { animation: float-z 3s infinite linear; animation-delay: 1.5s; }
            .anim-knob { animation: slide-knob 2.5s infinite ease-in-out; }
            .anim-bounce { animation: bounce 1.2s infinite ease-in-out; }
            .anim-rainbow-fill { animation: rainbow-cycle 8s infinite linear; }
            .anim-loading { animation: loading 3s infinite steps(7); }
          `}</style>
        </defs>

        <rect width="24" height="16" fill="#05070c" />

        {/* subtle bezel dots */}
        {[1, 22].map((x) =>
          [1, 14].map((y) => (
            <rect
              key={`${x}-${y}`}
              x={x}
              y={y}
              width="1"
              height="1"
              fill={state === "sec_rainbow" ? "currentColor" : c}
              className={state === "sec_rainbow" ? "anim-rainbow-fill" : ""}
              opacity="0.25"
            />
          )),
        )}

        <g
          fill={state === "sec_rainbow" ? "currentColor" : c}
          stroke={state === "sec_rainbow" ? "currentColor" : c}
          className={state === "sec_rainbow" ? "anim-rainbow-fill" : ""}
        >
          <Face
            state={state}
            color={state === "sec_rainbow" ? "currentColor" : c}
            eyeOffX={eyeOffX}
            eyeOffY={eyeOffY}
            blink={blink}
          />
        </g>
      </svg>
    </div>
  );
}

function Eye({
  x,
  y,
  color,
  shape,
  blink,
}: {
  x: number;
  y: number;
  color: string;
  shape: "square" | "round" | "wink" | "closed" | "wide" | "heart" | "small" | "up" | "radar";
  blink: boolean;
}) {
  if (blink || shape === "closed") {
    return <rect x={x} y={y + 2} width="4" height="1" fill={color} stroke="none" />;
  }
  if (shape === "wink") {
    return <rect x={x} y={y + 2} width="4" height="1" fill={color} stroke="none" />;
  }
  if (shape === "round") {
    return (
      <g fill={color} stroke="none">
        <rect x={x + 1} y={y} width="2" height="1" />
        <rect x={x} y={y + 1} width="4" height="2" />
        <rect x={x + 1} y={y + 3} width="2" height="1" />
      </g>
    );
  }
  if (shape === "wide") {
    return (
      <g fill={color} stroke="none">
        <rect x={x} y={y} width="4" height="4" />
        <rect x={x + 1} y={y + 1} width="2" height="2" fill="#05070c" />
      </g>
    );
  }
  if (shape === "heart") {
    return (
      <g fill={color} stroke="none">
        <rect x={x} y={y} width="1" height="1" />
        <rect x={x + 2} y={y} width="1" height="1" />
        <rect x={x} y={y + 1} width="3" height="1" />
        <rect x={x + 1} y={y + 2} width="2" height="1" />
        <rect x={x + 1} y={y + 3} width="1" height="1" />
      </g>
    );
  }
  if (shape === "small") {
    return <rect x={x + 1} y={y + 1} width="2" height="2" fill={color} stroke="none" />;
  }
  if (shape === "up") {
    return (
      <g fill={color} stroke="none">
        <rect x={x} y={y + 1} width="4" height="1" />
        <rect x={x + 1} y={y} width="2" height="1" />
      </g>
    );
  }
  return <rect x={x} y={y} width="4" height="4" fill={color} stroke="none" />;
}

function Face({
  state,
  color,
  eyeOffX,
  eyeOffY,
  blink,
}: {
  state: FaceState;
  color: string;
  eyeOffX: number;
  eyeOffY: number;
  blink: boolean;
}) {
  const lx = 5 + eyeOffX;
  const rx = 15 + eyeOffX;
  const ey = 5 + eyeOffY;

  switch (state) {
    case "idle":
    case "sec_rainbow":
      return (
        <>
          <Eye x={lx} y={ey} color={color} shape="square" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="square" blink={blink} />
          <rect x={10} y={12} width="4" height="1" fill={color} stroke="none" />
        </>
      );
    case "happy":
      return (
        <>
          <Eye x={lx} y={ey} color={color} shape="round" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="round" blink={blink} />
          <rect x={8} y={11} width="1" height="1" fill={color} stroke="none" />
          <rect x={9} y={12} width="6" height="1" fill={color} stroke="none" />
          <rect x={15} y={11} width="1" height="1" fill={color} stroke="none" />
        </>
      );
    case "excited":
      return (
        <>
          {/* animated stars flashing */}
          <rect
            x={3}
            y={2}
            width="1"
            height="1"
            fill={color}
            stroke="none"
            className="anim-pulse"
          />
          <rect
            x={20}
            y={3}
            width="1"
            height="1"
            fill={color}
            stroke="none"
            className="anim-pulse"
            style={{ animationDelay: "0.5s" }}
          />
          <Eye x={lx} y={ey} color={color} shape="wide" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="wide" blink={blink} />
          <rect x={11} y={12} width="2" height="2" fill={color} stroke="none" />
        </>
      );
    case "sleepy":
      return (
        <>
          {/* Animated Zzz */}
          <g transform="translate(19, 1)">
            <g className="anim-z1" stroke="none" fill={color}>
              <rect x="0" y="0" width="3" height="1" />
              <rect x="2" y="1" width="1" height="1" />
              <rect x="1" y="2" width="1" height="1" />
              <rect x="0" y="3" width="3" height="1" />
            </g>
          </g>
          <g transform="translate(18, 3)">
            <g className="anim-z2" stroke="none" fill={color}>
              <rect x="0" y="0" width="2" height="1" />
              <rect x="1" y="1" width="1" height="1" />
              <rect x="0" y="2" width="2" height="1" />
            </g>
          </g>
          <Eye x={lx} y={ey} color={color} shape="closed" blink={false} />
          <Eye x={rx} y={ey} color={color} shape="closed" blink={false} />
          <rect x={10} y={12} width="4" height="1" fill={color} stroke="none" />
        </>
      );
    case "thinking":
      return (
        <>
          {/* animated floating thought dots */}
          <rect
            x={19}
            y={3}
            width="1"
            height="1"
            fill={color}
            stroke="none"
            className="anim-pulse"
          />
          <rect
            x={21}
            y={3}
            width="1"
            height="1"
            fill={color}
            stroke="none"
            className="anim-pulse"
            style={{ animationDelay: "0.4s" }}
          />
          <Eye x={lx} y={ey} color={color} shape="up" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="up" blink={blink} />
          <rect x={10} y={12} width="3" height="1" fill={color} stroke="none" />
        </>
      );
    case "sad":
      return (
        <>
          {/* animated teardrop falling */}
          <rect
            x={5}
            y={ey + 3}
            width="1"
            height="1"
            fill={color}
            stroke="none"
            className="anim-sweat"
          />
          <Eye x={lx} y={ey + 1} color={color} shape="small" blink={blink} />
          <Eye x={rx} y={ey + 1} color={color} shape="small" blink={blink} />
          <rect x={9} y={13} width="1" height="1" fill={color} stroke="none" />
          <rect x={10} y={12} width="4" height="1" fill={color} stroke="none" />
          <rect x={14} y={13} width="1" height="1" fill={color} stroke="none" />
        </>
      );
    case "surprised":
      return (
        <>
          <Eye x={lx} y={ey} color={color} shape="round" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="round" blink={blink} />
          <rect x={11} y={12} width="2" height="2" fill={color} stroke="none" />
        </>
      );
    case "love":
      return (
        <>
          <Eye x={lx} y={ey} color={color} shape="heart" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="heart" blink={blink} />
          <rect x={9} y={12} width="6" height="1" fill={color} stroke="none" />
          <rect x={10} y={13} width="4" height="1" fill={color} stroke="none" />
        </>
      );
    case "sys_boot":
      return (
        <>
          {/* Progress bar container */}
          <rect x="4" y="6" width="16" height="3" fill="none" stroke={color} strokeWidth="1" />
          {/* Progress filling */}
          <rect x="5" y="7" height="1" fill={color} stroke="none" className="anim-loading" />
          <rect x="9" y="12" width="6" height="1" fill={color} stroke="none" />
        </>
      );

    // ── NEW SECURITY STATES ──
    case "sec_scanning":
      return (
        <>
          {/* scanning line sweep */}
          <rect
            x="1"
            y="0"
            width="22"
            height="1"
            fill={color}
            stroke="none"
            className="anim-sweep"
            opacity="0.4"
          />
          {/* scan radar eyes */}
          <g fill={color} stroke="none">
            {/* shifting look */}
            <rect x={lx + 1} y={ey + 1} width="2" height="2" className="anim-pulse" />
            <rect x={rx + 1} y={ey + 1} width="2" height="2" className="anim-pulse" />
          </g>
          <rect x={9} y={12} width="6" height="1" fill={color} stroke="none" />
        </>
      );
    case "sec_intrusion":
      return (
        <>
          {/* angry brows */}
          <g fill={color} stroke="none">
            <rect x={5} y={ey - 1} width="2" height="1" />
            <rect x={7} y={ey} width="1" height="1" />
            <rect x={8} y={ey + 1} width="1" height="1" />

            <rect x={17} y={ey - 1} width="2" height="1" />
            <rect x={16} y={ey} width="1" height="1" />
            <rect x={15} y={ey + 1} width="1" height="1" />
          </g>
          <Eye x={lx} y={ey} color={color} shape="small" blink={false} />
          <Eye x={rx} y={ey} color={color} shape="small" blink={false} />

          {/* flashing alerts */}
          <rect
            x="2"
            y="3"
            width="1"
            height="6"
            fill={color}
            stroke="none"
            className="anim-pulse"
          />
          <rect
            x="2"
            y="10"
            width="1"
            height="1"
            fill={color}
            stroke="none"
            className="anim-pulse"
          />
          <rect
            x="21"
            y="3"
            width="1"
            height="6"
            fill={color}
            stroke="none"
            className="anim-pulse"
          />
          <rect
            x="21"
            y="10"
            width="1"
            height="1"
            fill={color}
            stroke="none"
            className="anim-pulse"
          />

          {/* angry mouth */}
          <g fill={color} stroke="none">
            <rect x="9" y="13" width="6" height="1" />
            <rect x="8" y="12" width="1" height="1" />
            <rect x="15" y="12" width="1" height="1" />
          </g>
        </>
      );

    // ── NEW AVIATION STATES ──
    case "avi_disabled":
      return (
        <>
          {/* Warning outline triangle */}
          <polygon points="12,2 20,13 4,13" fill="none" stroke={color} strokeWidth="1" />
          {/* Exclamation point inside warning sign */}
          <rect x="11" y="5" width="2" height="4" fill={color} stroke="none" />
          <rect x="11" y="10" width="2" height="2" fill={color} stroke="none" />

          <text
            x="12"
            y="15"
            fontFamily="monospace"
            fontSize="1.9"
            fill={color}
            textAnchor="middle"
            fontWeight="bold"
          >
            AVIATION DISABLED
          </text>
        </>
      );
    case "avi_radar":
      return (
        <>
          {/* Center circular radar overlay */}
          <circle cx="12" cy="8" r="6" stroke={color} strokeWidth="1" fill="none" opacity="0.3" />
          <circle cx="12" cy="8" r="3" stroke={color} strokeWidth="1" fill="none" opacity="0.2" />
          <line
            x1="12"
            y1="8"
            x2="18"
            y2="8"
            stroke={color}
            strokeWidth="1"
            className="anim-rotate"
          />
          {/* mini dot for target */}
          <rect
            x="15"
            y="5"
            width="1"
            height="1"
            fill={color}
            stroke="none"
            className="anim-pulse"
          />
        </>
      );
    case "avi_lock":
      return (
        <>
          {/* crosshair brackets around eyes */}
          <g fill={color} stroke="none" className="anim-pulse">
            {/* left eye targeter */}
            <rect x="3" y="3" width="3" height="1" />
            <rect x="3" y="3" width="1" height="3" />
            <rect x="3" y="9" width="1" height="3" />
            <rect x="3" y="11" width="3" height="1" />
            <rect x="8" y="3" width="1" height="3" />
            <rect x="6" y="11" width="3" height="1" />

            {/* right eye targeter */}
            <rect x="18" y="3" width="3" height="1" />
            <rect x="20" y="3" width="1" height="3" />
            <rect x="20" y="9" width="1" height="3" />
            <rect x="18" y="11" width="3" height="1" />
            <rect x="15" y="3" width="1" height="3" />
            <rect x="15" y="11" width="3" height="1" />
          </g>
          {/* focused target locked eyes */}
          <rect x={lx + 1} y={ey + 1} width="2" height="2" fill={color} stroke="none" />
          <rect x={rx + 1} y={ey + 1} width="2" height="2" fill={color} stroke="none" />
          <rect x="10" y="12" width="4" height="1" fill={color} stroke="none" />
        </>
      );

    // ── NEW SYSTEM STATES ──
    case "sys_prefs":
      return (
        <>
          {/* sliders */}
          <g fill={color} stroke="none">
            {/* track 1 */}
            <rect x="4" y="5" width="16" height="1" opacity="0.3" />
            {/* knob 1 */}
            <rect x="8" y="4" width="2" height="3" className="anim-knob" />

            {/* track 2 */}
            <rect x="4" y="10" width="16" height="1" opacity="0.3" />
            {/* knob 2 */}
            <rect
              x="13"
              y="9"
              width="2"
              height="3"
              className="anim-knob"
              style={{ animationDelay: "1s" }}
            />
          </g>
        </>
      );
    case "sys_error":
      return (
        <>
          {/* X eyes */}
          <g stroke={color} strokeWidth="1" fill="none">
            <line x1="5" y1="5" x2="8" y2="8" />
            <line x1="8" y1="5" x2="5" y2="8" />

            <line x1="15" y1="5" x2="18" y2="8" />
            <line x1="18" y1="5" x2="15" y2="8" />
          </g>
          {/* flat dead mouth */}
          <rect x="9" y="12" width="6" height="1" fill={color} stroke="none" />
        </>
      );

    // ── NEW SECRET STATES ──
    case "sec_matrix":
      return (
        <>
          {/* matrix rain columns */}
          <g fill={color} stroke="none" className="anim-matrix">
            <rect x="2" y="1" width="1" height="3" />
            <rect x="2" y="6" width="1" height="2" />
            <rect x="6" y="3" width="1" height="4" />
            <rect x="10" y="0" width="1" height="2" />
            <rect x="10" y="5" width="1" height="3" />
            <rect x="14" y="2" width="1" height="5" />
            <rect x="18" y="0" width="1" height="3" />
            <rect x="18" y="7" width="1" height="2" />
            <rect x="21" y="4" width="1" height="4" />
          </g>
          {/* outline of simple eyes */}
          <rect
            x={lx + 1}
            y={ey + 1}
            width="2"
            height="2"
            fill="#05070c"
            stroke={color}
            strokeWidth="1"
          />
          <rect
            x={rx + 1}
            y={ey + 1}
            width="2"
            height="2"
            fill="#05070c"
            stroke={color}
            strokeWidth="1"
          />
        </>
      );
    case "sec_retro":
      return (
        <>
          {/* Space invader alien bouncing */}
          <g transform="translate(4, 3)">
            <g fill={color} stroke="none" className="anim-bounce">
              {/* invader pixels on 16x8 scale */}
              <rect x="5" y="0" width="1" height="1" />
              <rect x="10" y="0" width="1" height="1" />

              <rect x="0" y="1" width="1" height="1" />
              <rect x="2" y="1" width="1" height="1" />
              <rect x="13" y="1" width="1" height="1" />
              <rect x="15" y="1" width="1" height="1" />

              <rect x="0" y="2" width="1" height="4" />
              <rect x="15" y="2" width="1" height="4" />

              <rect x="1" y="2" width="14" height="3" />
              {/* space invader eyes cutout */}
              <rect x="4" y="3" width="2" height="1" fill="#05070c" />
              <rect x="10" y="3" width="2" height="1" fill="#05070c" />

              {/* legs */}
              <rect x="2" y="5" width="2" height="2" />
              <rect x="12" y="5" width="2" height="2" />
              <rect x="5" y="5" width="6" height="1" />
            </g>
          </g>
        </>
      );
  }
  return null;
}

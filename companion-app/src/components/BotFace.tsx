import { useEffect, useRef, useState } from "react";

export type FaceState =
  | "idle"
  | "happy"
  | "excited"
  | "sleepy"
  | "thinking"
  | "sad"
  | "alert"
  | "love"
  | "startup"
  | "surprised";

export const FACE_META: Record<
  FaceState,
  { label: string; sub: string; color: string; group: string }
> = {
  idle:      { label: "idle",      sub: "neutral / standby",         color: "#38e1ff", group: "PASSIVE" },
  happy:     { label: "happy",     sub: "positive action / greeting",color: "#4af0bc", group: "PASSIVE" },
  excited:   { label: "excited",   sub: "new connection / task",     color: "#ffd23f", group: "PASSIVE" },
  sleepy:    { label: "sleepy",    sub: "idle timeout / low battery",color: "#6d8cff", group: "PASSIVE" },
  thinking:  { label: "thinking",  sub: "processing / waiting",      color: "#b86bff", group: "REACTIVE" },
  sad:       { label: "sad",       sub: "disconnect / task fail",    color: "#ff5466", group: "REACTIVE" },
  alert:     { label: "alert",     sub: "USB inserted / intrusion",  color: "#ff8a3d", group: "REACTIVE" },
  love:      { label: "love",      sub: "paired device / uptime",    color: "#ff4fd8", group: "REACTIVE" },
  startup:   { label: "startup",   sub: "power-on sequence",         color: "#a8b3cf", group: "SYSTEM" },
  surprised: { label: "surprised", sub: "new notification / drop",   color: "#e8fff5", group: "SYSTEM" },
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
  const c = FACE_META[state].color;
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
    const t = setInterval(() => {
      setBlink(true);
      setTimeout(() => setBlink(false), 140);
    }, 3200 + Math.random() * 1800);
    return () => clearInterval(t);
  }, []);

  // viewBox 24x24
  const eyeOffX = ["idle", "happy", "love", "surprised"].includes(state) ? offset.x : 0;
  const eyeOffY = ["idle", "happy", "love", "surprised"].includes(state) ? offset.y : 0;

  return (
    <div
      ref={ref}
      className={className}
      style={{
        width: size,
        height: size * (16 / 24),
        imageRendering: "pixelated",
        filter: `drop-shadow(0 0 18px ${c}55) drop-shadow(0 0 4px ${c}88)`,
      }}
    >
      <svg
        viewBox="0 0 24 16"
        width="100%"
        height="100%"
        shapeRendering="crispEdges"
        style={{ imageRendering: "pixelated" }}
      >
        <rect width="24" height="16" fill="#06080e" />
        {/* subtle bezel dots */}
        {[1, 22].map((x) =>
          [1, 14].map((y) => (
            <rect key={`${x}-${y}`} x={x} y={y} width="1" height="1" fill={`${c}40`} />
          ))
        )}
        <Face state={state} color={c} eyeOffX={eyeOffX} eyeOffY={eyeOffY} blink={blink} />
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
  shape: "square" | "round" | "wink" | "closed" | "wide" | "heart" | "small" | "up";
  blink: boolean;
}) {
  if (blink || shape === "closed") {
    return <rect x={x} y={y + 2} width="4" height="1" fill={color} />;
  }
  if (shape === "wink") {
    return <rect x={x} y={y + 2} width="4" height="1" fill={color} />;
  }
  if (shape === "round") {
    return (
      <>
        <rect x={x + 1} y={y} width="2" height="1" fill={color} />
        <rect x={x} y={y + 1} width="4" height="2" fill={color} />
        <rect x={x + 1} y={y + 3} width="2" height="1" fill={color} />
      </>
    );
  }
  if (shape === "wide") {
    return (
      <>
        <rect x={x} y={y} width="4" height="4" fill={color} />
        <rect x={x + 1} y={y + 1} width="2" height="2" fill="#06080e" />
      </>
    );
  }
  if (shape === "heart") {
    return (
      <>
        <rect x={x} y={y} width="1" height="1" fill={color} />
        <rect x={x + 2} y={y} width="1" height="1" fill={color} />
        <rect x={x} y={y + 1} width="3" height="1" fill={color} />
        <rect x={x + 1} y={y + 2} width="2" height="1" fill={color} />
        <rect x={x + 1} y={y + 3} width="1" height="1" fill={color} />
      </>
    );
  }
  if (shape === "small") {
    return <rect x={x + 1} y={y + 1} width="2" height="2" fill={color} />;
  }
  if (shape === "up") {
    return (
      <>
        <rect x={x} y={y + 1} width="4" height="1" fill={color} />
        <rect x={x + 1} y={y} width="2" height="1" fill={color} />
      </>
    );
  }
  return <rect x={x} y={y} width="4" height="4" fill={color} />;
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
      return (
        <>
          <Eye x={lx} y={ey} color={color} shape="square" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="square" blink={blink} />
          <rect x={10} y={12} width="4" height="1" fill={color} />
        </>
      );
    case "happy":
      return (
        <>
          <Eye x={lx} y={ey} color={color} shape="round" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="round" blink={blink} />
          <rect x={8} y={11} width="1" height="1" fill={color} />
          <rect x={9} y={12} width="6" height="1" fill={color} />
          <rect x={15} y={11} width="1" height="1" fill={color} />
        </>
      );
    case "excited":
      return (
        <>
          {/* stars */}
          <rect x={3} y={2} width="1" height="1" fill={color} />
          <rect x={20} y={3} width="1" height="1" fill={color} />
          <Eye x={lx} y={ey} color={color} shape="wide" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="wide" blink={blink} />
          <rect x={11} y={12} width="2" height="2" fill={color} />
        </>
      );
    case "sleepy":
      return (
        <>
          <rect x={3} y={2} width="1" height="1" fill={color} />
          <rect x={4} y={1} width="1" height="1" fill={color} />
          <Eye x={lx} y={ey} color={color} shape="closed" blink={false} />
          <Eye x={rx} y={ey} color={color} shape="closed" blink={false} />
          <rect x={10} y={12} width="4" height="1" fill={color} />
        </>
      );
    case "thinking":
      return (
        <>
          {/* dots */}
          <rect x={19} y={3} width="1" height="1" fill={color} />
          <rect x={21} y={3} width="1" height="1" fill={color} />
          <Eye x={lx} y={ey} color={color} shape="up" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="up" blink={blink} />
          <rect x={10} y={12} width="3" height="1" fill={color} />
        </>
      );
    case "sad":
      return (
        <>
          <Eye x={lx} y={ey + 1} color={color} shape="small" blink={blink} />
          <Eye x={rx} y={ey + 1} color={color} shape="small" blink={blink} />
          <rect x={9} y={13} width="1" height="1" fill={color} />
          <rect x={10} y={12} width="4" height="1" fill={color} />
          <rect x={14} y={13} width="1" height="1" fill={color} />
        </>
      );
    case "alert":
      return (
        <>
          <rect x={3} y={2} width="1" height="1" fill={color} />
          <rect x={20} y={2} width="1" height="1" fill={color} />
          <Eye x={lx} y={ey} color={color} shape="square" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="square" blink={blink} />
          <rect x={11} y={12} width="2" height="2" fill={color} />
        </>
      );
    case "love":
      return (
        <>
          <Eye x={lx} y={ey} color={color} shape="heart" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="heart" blink={blink} />
          <rect x={9} y={12} width="6" height="1" fill={color} />
          <rect x={10} y={13} width="4" height="1" fill={color} />
        </>
      );
    case "startup":
      return (
        <>
          <rect x={3} y={5} width="6" height="1" fill={color} />
          <rect x={3} y={7} width="6" height="1" fill={color} />
          <rect x={15} y={5} width="6" height="1" fill={color} />
          <rect x={15} y={7} width="6" height="1" fill={color} />
          <rect x={9} y={12} width="6" height="1" fill={color} />
        </>
      );
    case "surprised":
      return (
        <>
          <Eye x={lx} y={ey} color={color} shape="round" blink={blink} />
          <Eye x={rx} y={ey} color={color} shape="round" blink={blink} />
          <rect x={11} y={12} width="2" height="2" fill="#ffd23f" />
        </>
      );
  }
}
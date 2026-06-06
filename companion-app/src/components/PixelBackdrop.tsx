import { useEffect, useRef } from "react";

interface Props {
  mode: number;
}

const MODE_COLORS: Record<number, [number, number, number]> = {
  0: [255, 79, 216], // pink — Cyber-Pet
  1: [255, 138, 61], // orange — Auditor
  2: [255, 210, 63], // yellow — Radar
  3: [74, 240, 188], // neon green — Payload
  4: [153, 102, 255], // violet — Tutorial
};

/**
 * Full-viewport canvas pixelscape: a low-res grid of pixels that
 * reacts to cursor proximity and shifts hue with the selected mode.
 * Sits behind everything (z -1) and is purely decorative.
 */
export function PixelBackdrop({ mode }: Props) {
  const ref = useRef<HTMLCanvasElement>(null);
  const mouse = useRef({ x: -9999, y: -9999, down: false });
  const ripples = useRef<{ x: number; y: number; t: number }[]>([]);
  const modeRef = useRef(mode);

  useEffect(() => {
    modeRef.current = mode;
    // emit a "mode pulse" ripple from the center
    const c = ref.current;
    if (c) ripples.current.push({ x: c.width / 2, y: c.height / 2, t: performance.now() });
  }, [mode]);

  useEffect(() => {
    const canvas = ref.current!;
    const ctx = canvas.getContext("2d")!;
    let raf = 0;
    let w = 0,
      h = 0;
    const CELL = 18; // pixel grid cell size (CSS px)

    const resize = () => {
      w = canvas.width = window.innerWidth;
      h = canvas.height = window.innerHeight;
    };
    resize();

    const onMove = (e: MouseEvent) => {
      mouse.current.x = e.clientX;
      mouse.current.y = e.clientY;
    };
    const onLeave = () => {
      mouse.current.x = -9999;
      mouse.current.y = -9999;
    };
    const onClick = (e: MouseEvent) => {
      ripples.current.push({ x: e.clientX, y: e.clientY, t: performance.now() });
      if (ripples.current.length > 8) ripples.current.shift();
    };

    window.addEventListener("resize", resize);
    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseleave", onLeave);
    window.addEventListener("click", onClick);

    const NEON: [number, number, number] = [74, 240, 188];

    const draw = (now: number) => {
      ctx.clearRect(0, 0, w, h);
      const [mr, mg, mb] = MODE_COLORS[modeRef.current] || [74, 240, 188];
      const cols = Math.ceil(w / CELL);
      const rows = Math.ceil(h / CELL);
      const mx = mouse.current.x;
      const my = mouse.current.y;
      const t = now / 1000;

      // prune old ripples
      ripples.current = ripples.current.filter((r) => now - r.t < 2200);

      for (let i = 0; i < cols; i++) {
        for (let j = 0; j < rows; j++) {
          const cx = i * CELL + CELL / 2;
          const cy = j * CELL + CELL / 2;

          // ambient wave
          const wave = Math.sin((i + j) * 0.45 + t * 1.4) * 0.5 + 0.5;
          let a = 0.04 + wave * 0.05;

          // cursor proximity glow
          const dx = cx - mx,
            dy = cy - my;
          const dist = Math.sqrt(dx * dx + dy * dy);
          const cursor = Math.max(0, 1 - dist / 220);
          a += cursor * 0.55;

          // ripples
          let rippleMix = 0;
          for (const r of ripples.current) {
            const age = (now - r.t) / 2200;
            const radius = age * Math.max(w, h) * 0.9;
            const rd = Math.sqrt((cx - r.x) ** 2 + (cy - r.y) ** 2);
            const band = Math.max(0, 1 - Math.abs(rd - radius) / 40);
            rippleMix = Math.max(rippleMix, band * (1 - age));
          }
          a += rippleMix * 0.7;

          if (a < 0.05) continue;
          if (a > 0.95) a = 0.95;

          // blend mode color in with cursor/ripple influence
          const mix = Math.min(1, cursor * 0.9 + rippleMix);
          const r = Math.round(NEON[0] * (1 - mix) + mr * mix);
          const g = Math.round(NEON[1] * (1 - mix) + mg * mix);
          const b = Math.round(NEON[2] * (1 - mix) + mb * mix);

          const size = 2 + cursor * 4 + rippleMix * 3;
          ctx.fillStyle = `rgba(${r},${g},${b},${a})`;
          ctx.fillRect(cx - size / 2, cy - size / 2, size, size);
        }
      }

      raf = requestAnimationFrame(draw);
    };
    raf = requestAnimationFrame(draw);

    return () => {
      cancelAnimationFrame(raf);
      window.removeEventListener("resize", resize);
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseleave", onLeave);
      window.removeEventListener("click", onClick);
    };
  }, []);

  return (
    <canvas
      ref={ref}
      aria-hidden
      className="pointer-events-none fixed inset-0 z-0"
      style={{ mixBlendMode: "screen" }}
    />
  );
}

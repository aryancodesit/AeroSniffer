import { defineConfig } from "vite";
import { tanstackStart } from "@tanstack/react-start/plugin/vite";
import viteReact from "@vitejs/plugin-react";
import tsconfigPaths from "vite-tsconfig-paths";
import tailwindcss from "@tailwindcss/vite";

export default defineConfig({
  plugins: [
    tanstackStart({
      server: {
        // @ts-ignore
        preset: "vercel",
      },
    }),
    viteReact(),
    tsconfigPaths(),
    tailwindcss(),
  ],
});

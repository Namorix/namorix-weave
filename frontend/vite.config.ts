import { defineConfig } from "vite"
import react from "@vitejs/plugin-react"
import { federation } from "@module-federation/vite"

export default defineConfig({
  base: "./",
  plugins: [
    react(),
    federation({
      name: "addon_weave",
      manifest: true,
      exposes: {
        "./Addon": "./src/mount.tsx",
      },
      shared: {
        react: { singleton: true },
        "react-dom": { singleton: true },
        "@namorix/core": { singleton: true },
      },
      dts: false,
    }),
  ],
  resolve: {
    alias: {
      "@namorix/core":
      new URL("../../namorix/frontend/packages/core/src", import.meta.url)
          .pathname,
    },
  },
  optimizeDeps: {
    exclude: ["@namorix/core"],
  },
  server: {
    port: 5180,
  },
  build: {
    target: "esnext",
  },
  preview: {
    port: 5180,
    host: "0.0.0.0",
    cors: true,
  },
})

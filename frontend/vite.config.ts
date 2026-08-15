import { defineConfig, loadEnv } from "vite"
import react from "@vitejs/plugin-react"
import { federation } from "@module-federation/vite"

export default defineConfig((config) => {
  const env = loadEnv(config.mode, new URL(".", import.meta.url).pathname, "")
  const frontendPort = parseInt(env.ADDON_FRONTEND_PORT || "5102", 10)
  const backendPort = parseInt(env.ADDON_BACKEND_PORT || "5100", 10)
  const addonHost = env.ADDON_HOST ?? "http://localhost"
  const frontendUrl = `${addonHost}:${frontendPort}`
  const backendUrl = `${addonHost}:${backendPort}`

  return {
    base: "./",
    plugins: [
      react(),
      federation({
        name: "addon_weave",
        manifest: true,
        publicPath: `${frontendUrl}/`,
        exposes: {
          "./Addon": "./src/mount.tsx",
        },
        shared: {
          react: { singleton: true },
          i18next: { singleton: true },
          "react-dom": { singleton: true },
          "react-i18next": { singleton: true },
        },
        dts: false,
      }),
    ],
    resolve: {
      alias:
        config.mode !== "production"
          ? {
              "@namorix/core": new URL(
                "../../namorix/frontend/packages/core/src",
                import.meta.url,
              ).pathname,
            }
          : undefined,
    },
    optimizeDeps: {
      exclude: ["@namorix/core"],
    },
    server: {
      host: "0.0.0.0",
      port: frontendPort,
      hmr: {
        port: frontendPort,
        // Browser connects HMR websocket through Kestrel (backendPort) in dev,
        // which YARP forwards to this Vite server.
        clientPort: backendPort,
      },
      allowedHosts: true,
      cors: true,
      proxy: {
        // changeOrigin:false giữ nguyên Host gốc (VD localhost:5102) — backend cần nó
        // để dựng redirect_uri + cookie scoping đúng FE origin.
        "/.well-known": { target: backendUrl, changeOrigin: false },
        "/api": { target: backendUrl, changeOrigin: false },
        "/hubs": { target: backendUrl, changeOrigin: false },
      },
    },
    build: {
      target: "esnext",
    },
    preview: {
      port: frontendPort,
      host: "0.0.0.0",
      cors: true,
    },
  }
})

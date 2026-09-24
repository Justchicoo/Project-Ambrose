/*
 * Project Ambrose by Imjustchico
 * How the panel is built: a relative base so the identical output serves from a subpath or a web view, the shared plugin set so its tokens and icons are the same ones the launcher uses, the $lib alias the shadcn-svelte components are written against, and for development a proxy that sends the page's API calls to the admin API that AMBROSE_PANEL_API names, presenting that API's own origin so its sign-in and CSRF checks hold.
 */

import { fileURLToPath } from "node:url";
import { defineConfig } from "vite";
import { ambrosePlugins } from "../../packages/ui/vite.ts";

const api = process.env.AMBROSE_PANEL_API;

export default defineConfig({
    base: "./",
    server: api
        ? {
              proxy: {
                  "/api": {
                      target: api,
                      changeOrigin: true,
                      ws: true,
                      configure: (proxy) => proxy.on("proxyReq", (outgoing) => outgoing.setHeader("origin", new URL(api).origin)),
                  },
              },
          }
        : undefined,
    plugins: ambrosePlugins(),
    resolve: {
        alias: { $lib: fileURLToPath(new URL("./src/lib", import.meta.url)) },
    },
    build: {
        outDir: "dist",
        emptyOutDir: true,
        target: "es2022",
        sourcemap: false,
    },
});

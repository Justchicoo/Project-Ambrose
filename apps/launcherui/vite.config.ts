/*
 * Project Ambrose by Imjustchico
 * How the launcher window is built: a relative base so the same output works behind a virtual host and behind a custom scheme, and the shared plugin set so it cannot drift from the panel.
 */

import { defineConfig } from "vite";
import { ambrosePlugins } from "../../packages/ui/vite.ts";

export default defineConfig({
    base: "./",
    plugins: ambrosePlugins(),
    build: {
        outDir: "dist",
        emptyOutDir: true,
        target: "es2022",
        sourcemap: false,
    },
});

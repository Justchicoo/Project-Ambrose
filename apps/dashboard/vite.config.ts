/*
 * Project Ambrose by Imjustchico
 * How the panel is built: a relative base so the identical output serves from a subpath or a web view, and the shared plugin set so its tokens and icons are the same ones the launcher uses.
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

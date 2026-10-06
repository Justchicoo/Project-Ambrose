/*
 * Project Ambrose by Imjustchico
 * How the panel program's own screens are built: a relative base so the same output is served from memory on the program's own origin under either web view, and the shared plugin set so it cannot drift from the panel it opens.
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

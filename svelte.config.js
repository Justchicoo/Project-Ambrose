/*
 * Project Ambrose by Imjustchico
 * The Svelte compiler settings every surface shares: runes only for our own files, so none can fall back to legacy reactivity, while a dependency still compiles the way it was written.
 */

import { vitePreprocess } from "@sveltejs/vite-plugin-svelte";

export default {
    preprocess: vitePreprocess(),
    compilerOptions: {
        runes: true,
    },
    onwarn(warning, handler) {
        if (warning.code.startsWith("a11y")) {
            throw new Error(`${warning.filename}: ${warning.message} (${warning.code})`);
        }
        handler(warning);
    },
    vitePlugin: {
        inspector: false,
        dynamicCompileOptions({ filename }) {
            if (filename.replace(/\\/g, "/").includes("/node_modules/")) {
                return { runes: undefined };
            }
            return { runes: true };
        },
    },
};

/*
 * Project Ambrose by Imjustchico
 * The one plugin set every Ambrose surface, the gallery and the tests build with, so the token engine, the vendored icons and the Svelte compiler are configured in one place.
 */

import { svelte } from "@sveltejs/vite-plugin-svelte";
import tailwindcss from "@tailwindcss/vite";
import { FileSystemIconLoader } from "unplugin-icons/loaders";
import icons from "unplugin-icons/vite";
import { fileURLToPath } from "node:url";
import type { PluginOption } from "vite";

export const iconFolder = fileURLToPath(new URL("../../design/icons", import.meta.url));

export function ambroseTokenPlugins(): PluginOption[] {
    return [
        tailwindcss(),
        icons({
            compiler: "svelte",
            customCollections: {
                ambrose: FileSystemIconLoader(iconFolder),
            },
            iconCustomizer(_collection, _icon, props) {
                props.width = "1em";
                props.height = "1em";
            },
        }),
    ];
}

export function ambrosePlugins(): PluginOption[] {
    return [...ambroseTokenPlugins(), svelte()];
}

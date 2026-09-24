/*
 * Project Ambrose by Imjustchico
 * The gallery's configuration: where the stories are, the accessibility and test addons that gate them, and the token engine the surfaces build with, with telemetry off.
 */

import type { StorybookConfig } from "@storybook/svelte-vite";
import { ambrosePlugins } from "../vite.ts";

const config: StorybookConfig = {
    framework: {
        name: "@storybook/svelte-vite",
        options: {},
    },
    stories: ["../src/components/**/*.stories.svelte", "../src/gallery/**/*.stories.svelte"],
    addons: ["@storybook/addon-svelte-csf", "@storybook/addon-a11y", "@storybook/addon-vitest"],
    core: {
        disableTelemetry: true,
        disableWhatsNewNotifications: true,
    },
    async viteFinal(inner) {
        inner.plugins = [...ambrosePlugins(), ...(inner.plugins ?? [])];
        return inner;
    },
};

export default config;

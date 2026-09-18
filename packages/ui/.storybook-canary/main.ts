/*
 * Project Ambrose by Imjustchico
 * The canary gallery, which holds only the deliberately unlabelled story the canary run has to fail on, so the accessibility gate is proved to be running.
 */

import type { StorybookConfig } from "@storybook/svelte-vite";
import { ambrosePlugins } from "../vite.ts";

const config: StorybookConfig = {
    framework: {
        name: "@storybook/svelte-vite",
        options: {},
    },
    stories: ["../src/canary/*.stories.svelte"],
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

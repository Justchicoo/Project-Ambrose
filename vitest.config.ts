/*
 * Project Ambrose by Imjustchico
 * The test projects: the logic and token checks with no browser, every story in Chromium and in WebKit with the accessibility gate, because the launcher window runs in WebKitGTK, and the panel's own logic with no browser and its pages mounted in Chromium.
 */

import { storybookTest } from "@storybook/addon-vitest/vitest-plugin";
import { playwright } from "@vitest/browser-playwright";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vitest/config";
import { ambrosePlugins } from "./packages/ui/vite.ts";

const storybook = await storybookTest({ configDir: "packages/ui/.storybook" });
const browserSetup = fileURLToPath(new URL("./packages/ui/src/test/browser.setup.ts", import.meta.url));
const panelLib = { $lib: fileURLToPath(new URL("./apps/dashboard/src/lib", import.meta.url)) };

function browserProject(name: "chromium" | "webkit") {
    return {
        plugins: [...storybook],
        test: {
            name,
            setupFiles: [browserSetup],
            browser: {
                enabled: true,
                headless: true,
                provider: playwright(),
                instances: [{ browser: name }],
            },
        },
    };
}

export default defineConfig({
    test: {
        projects: [
            {
                plugins: ambrosePlugins(),
                test: {
                    name: "logic",
                    environment: "node",
                    include: ["packages/ui/src/**/*.test.ts"],
                },
            },
            browserProject("chromium"),
            browserProject("webkit"),
            {
                plugins: ambrosePlugins(),
                resolve: { alias: panelLib },
                test: {
                    name: "dashboard",
                    environment: "node",
                    server: { deps: { inline: ["@lucide/svelte"] } },
                    include: ["apps/dashboard/src/**/*.test.ts"],
                    exclude: ["apps/dashboard/src/**/*.browser.test.ts"],
                },
            },
            {
                plugins: ambrosePlugins(),
                resolve: { alias: panelLib },
                test: {
                    name: "dashboard-browser",
                    include: ["apps/dashboard/src/**/*.browser.test.ts"],
                    browser: {
                        enabled: true,
                        headless: true,
                        provider: playwright(),
                        instances: [{ browser: "chromium" }],
                    },
                },
            },
        ],
    },
});

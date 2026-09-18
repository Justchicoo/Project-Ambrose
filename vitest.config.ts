/*
 * Project Ambrose by Imjustchico
 * The four test projects: the logic and token checks with no browser, and every story in Chromium and in WebKit with the accessibility gate, because the launcher window runs in WebKitGTK.
 */

import { storybookTest } from "@storybook/addon-vitest/vitest-plugin";
import { playwright } from "@vitest/browser-playwright";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vitest/config";
import { ambrosePlugins } from "./packages/ui/vite.ts";

const storybook = await storybookTest({ configDir: "packages/ui/.storybook" });
const browserSetup = fileURLToPath(new URL("./packages/ui/src/test/browser.setup.ts", import.meta.url));

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
        ],
    },
});

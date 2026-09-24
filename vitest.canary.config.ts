/*
 * Project Ambrose by Imjustchico
 * The canary run, which has to fail: it renders one story with a deliberately unlabelled control, so a gate that stopped checking is caught rather than passing quietly.
 */

import { storybookTest } from "@storybook/addon-vitest/vitest-plugin";
import { playwright } from "@vitest/browser-playwright";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vitest/config";

const storybook = await storybookTest({ configDir: "packages/ui/.storybook-canary" });
const browserSetup = fileURLToPath(new URL("./packages/ui/src/test/browser.setup.ts", import.meta.url));

export default defineConfig({
    plugins: [...storybook],
    test: {
        name: "canary",
        setupFiles: [browserSetup],
        browser: {
            enabled: true,
            headless: true,
            provider: playwright(),
            instances: [{ browser: "chromium" }],
        },
    },
});

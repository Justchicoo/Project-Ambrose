/*
 * Project Ambrose by Imjustchico
 * The two runs that stay off the blocking path: the end-to-end project the surfaces will use against a real build, and the screenshots, which are taken only in the official container so a font or a driver cannot move a baseline.
 */

import { defineConfig, devices } from "@playwright/test";

const port = 4173;

export default defineConfig({
    testDir: "tests",
    fullyParallel: true,
    forbidOnly: !!process.env.CI,
    retries: process.env.CI ? 1 : 0,
    reporter: [["list"]],
    use: {
        baseURL: `http://localhost:${port}`,
        trace: "on-first-retry",
    },
    projects: [
        {
            name: "e2e",
            testDir: "tests/e2e",
            use: { ...devices["Desktop Chrome"] },
        },
        {
            name: "screenshots",
            testDir: "tests/screenshots",
            use: { ...devices["Desktop Chrome"] },
            snapshotPathTemplate: "tests/screenshots/baselines/{projectName}-{platform}/{testFilePath}/{arg}{ext}",
        },
    ],
    webServer: {
        command: `npm run preview --workspace apps/dashboard -- --port ${port} --strictPort`,
        url: `http://localhost:${port}`,
        reuseExistingServer: !process.env.CI,
        timeout: 120000,
    },
});

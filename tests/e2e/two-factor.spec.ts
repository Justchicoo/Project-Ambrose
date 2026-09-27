/*
 * Project Ambrose by Imjustchico
 * Two-factor sign-in on a real supervisor's panel listener that requires it of everyone: the owner made from the one-time link is sent to enrollment before any other page, turns it on with a code this run computes from the setup key the way an authenticator app does, sees the recovery codes once and only then reaches the overview; signing in again takes the password and then a recovery code, which works once and is refused the second time; and no code is left in the browser's storage at any point.
 */

import { createHmac } from "node:crypto";
import { expect, test, type Page } from "@playwright/test";
import { app, built, operator, port, startPanelListener, supervisor, type PanelListener } from "./panel-server";

test.skip(!supervisor || !app || !built, "needs the built panel, patchserver and supervisor");
test.describe.configure({ mode: "serial" });

let panel: PanelListener;
let codes: string[] = [];

function base32(text: string): Buffer {
    const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    const bytes: number[] = [];
    let buffer = 0;
    let bits = 0;
    for (const character of text.replace(/[\s=]/g, "").toUpperCase()) {
        const value = alphabet.indexOf(character);
        if (value < 0) throw new Error(`${character} is not base32`);
        buffer = (buffer << 5) | value;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            bytes.push((buffer >> bits) & 0xff);
        }
        buffer &= (1 << bits) - 1;
    }
    return Buffer.from(bytes);
}

function totp(secret: Buffer, unixSeconds: number): string {
    const counter = Buffer.alloc(8);
    counter.writeBigUInt64BE(BigInt(Math.floor(unixSeconds / 30)));
    const mac = createHmac("sha1", secret).update(counter).digest();
    const offset = mac[mac.length - 1] & 0x0f;
    const value = ((mac[offset] & 0x7f) << 24) | (mac[offset + 1] << 16) | (mac[offset + 2] << 8) | mac[offset + 3];
    return String(value % 1000000).padStart(6, "0");
}

async function storage(page: Page): Promise<string> {
    return page.evaluate(() => {
        const kept: string[] = [];
        for (const place of [window.localStorage, window.sessionStorage])
            for (let index = 0; index < place.length; index += 1) {
                const key = place.key(index) ?? "";
                kept.push(key, place.getItem(key) ?? "");
            }
        return kept.join("\n");
    });
}

async function password(page: Page) {
    await page.goto(`${panel.url}/#overview`);
    await page.locator("#sign-in-username").fill(operator.name);
    await page.locator("#sign-in-password").fill(operator.password);
    await page.getByRole("button", { name: "Sign in" }).click();
    await expect(page.getByRole("heading", { name: "Enter your code", level: 1 })).toBeVisible();
}

test.beforeAll(async () => {
    panel = await startPanelListener(port(40), port(41), port(42), ["Panel.TwoFactorRequired = everyone"]);
});

test.afterAll(async () => {
    await panel?.stop();
});

test("a panel requiring two-factor for everyone sends its new owner to enrollment before any other page", async ({ page }) => {
    await page.goto(`${panel.url}/#claim?token=${panel.claim}`);
    await page.locator("#sign-in-token").fill(panel.claim);
    await page.locator("#sign-in-username").fill(operator.name);
    await page.locator("#sign-in-password").fill(operator.password);
    await page.getByRole("button", { name: "Make me the owner" }).click();

    await expect(page.getByRole("heading", { name: "Turn on two-factor sign-in", level: 1 })).toBeVisible();
    await expect(page.getByRole("heading", { name: "Overview", level: 1 })).toHaveCount(0);
    const key = await page.getByLabel("Setup key").textContent();
    const secret = base32(key ?? "");
    expect(secret.length).toBeGreaterThanOrEqual(16);

    await page.locator("#two-factor-password").fill(operator.password);
    await page.locator("#two-factor-code").fill(totp(secret, Date.now() / 1000));
    await page.getByRole("button", { name: "Turn on two-factor sign-in" }).click();

    const dialog = page.getByRole("dialog");
    await expect(dialog.getByText("Save your recovery codes")).toBeVisible();
    codes = (await dialog.getByRole("list", { name: "Recovery codes" }).getByRole("listitem").allTextContents()).map((code) => code.trim());
    expect(codes).toHaveLength(10);
    await page.keyboard.press("Escape");
    await expect(dialog).toBeVisible();
    await dialog.getByRole("switch").click();
    await dialog.getByRole("button", { name: "Done" }).click();

    await expect(page.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();
    const kept = await storage(page);
    for (const code of codes) expect(kept).not.toContain(code);
    expect(await page.content()).not.toContain(codes[0]);
});

test("a recovery code signs in once and is refused the second time", async ({ browser }) => {
    expect(codes).toHaveLength(10);
    const first = await browser.newPage();
    await password(first);
    await first.getByRole("button", { name: "Use a recovery code instead" }).click();
    await first.locator("#sign-in-recovery-code").fill(codes[0].toLowerCase());
    await first.getByRole("button", { name: "Confirm" }).click();
    await expect(first.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();
    expect(await storage(first)).not.toContain(codes[0]);
    await first.close();

    const second = await browser.newPage();
    await password(second);
    await second.getByRole("button", { name: "Use a recovery code instead" }).click();
    await second.locator("#sign-in-recovery-code").fill(codes[0]);
    await second.getByRole("button", { name: "Confirm" }).click();
    await expect(second.getByRole("alert")).toContainText("That code does not sign you in.");
    await expect(second.getByRole("heading", { name: "Overview", level: 1 })).toHaveCount(0);

    await second.locator("#sign-in-recovery-code").fill(codes[1]);
    await second.getByRole("button", { name: "Confirm" }).click();
    await expect(second.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();
    for (const code of codes) expect(await storage(second)).not.toContain(code);
    await second.close();
});

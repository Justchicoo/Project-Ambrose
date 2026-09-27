/*
 * Project Ambrose by Imjustchico
 * The built panel loaded from the panel's own listener rather than an app's admin API: its first operator is made from the link the supervisor printed and reaches the overview with nothing written to the browser console, every request it makes goes back to the listener it came from, every response carries the policy, frame denial, nosniff and referrer headers, the servers page reaches the app the supervisor runs through that listener rather than the app list of the supervisor alone, and a signed-in page opens exactly one event socket, to the listener's own events path with nothing in its address, whose first frame is hello and whose first answer is ready.
 */

import { expect, test } from "@playwright/test";
import { app, built, operator, port, startPanelListener, supervisor, type PanelListener } from "./panel-server";

test.skip(!supervisor || !app || !built, "needs the built panel, patchserver and supervisor");
test.describe.configure({ mode: "serial" });

let panel: PanelListener;

test.beforeAll(async () => {
    panel = await startPanelListener(port(30), port(31), port(32));
});

test.afterAll(async () => {
    await panel?.stop();
});

test("the panel loads from its own listener with no console errors and no request to another host", async ({ page }) => {
    const complaints: string[] = [];
    const elsewhere: string[] = [];
    page.on("console", (message) => {
        if (message.type() === "error" || message.type() === "warning") complaints.push(`${message.type()}: ${message.text()}`);
    });
    page.on("pageerror", (failure) => complaints.push(`pageerror: ${failure.message}`));
    page.on("request", (request) => {
        if (!request.url().startsWith(panel.url) && !request.url().startsWith("data:")) elsewhere.push(request.url());
    });
    page.on("response", (answer) => {
        if (answer.status() >= 400) complaints.push(`${answer.status()} ${answer.url()}`);
    });

    await page.goto(`${panel.url}/#claim?token=${panel.claim}`);
    await page.locator("#sign-in-token").fill(panel.claim);
    await page.locator("#sign-in-username").fill(operator.name);
    await page.locator("#sign-in-password").fill(operator.password);
    await page.getByRole("button", { name: "Make me the owner" }).click();
    await expect(page.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();

    expect(complaints).toEqual([]);
    expect(elsewhere).toEqual([]);
});

test("every response from the panel listener carries its security headers", async () => {
    const answer = await fetch(`${panel.url}/`);
    expect(answer.status).toBe(200);
    expect(answer.headers.get("content-security-policy")).toContain("frame-ancestors 'none'");
    expect(answer.headers.get("x-content-type-options")).toBe("nosniff");
    expect(answer.headers.get("x-frame-options")).toBe("DENY");
    expect(answer.headers.get("referrer-policy")).toBe("same-origin");
    expect(answer.headers.get("strict-transport-security")).toBeNull();

    const refused = await fetch(`${panel.url}/api/health`);
    expect(refused.status).toBe(401);
    expect(refused.headers.get("content-security-policy")).toContain("frame-ancestors 'none'");
    expect(refused.headers.get("x-content-type-options")).toBe("nosniff");
});

test("the servers page reaches the app the supervisor runs through the panel listener", async ({ page }) => {
    await page.goto(`${panel.url}/#servers`);
    await page.locator("#sign-in-username").fill(operator.name);
    await page.locator("#sign-in-password").fill(operator.password);
    await page.getByRole("button", { name: "Sign in" }).click();
    await expect(page.getByRole("heading", { name: "Servers", level: 1 })).toBeVisible();

    const row = page.getByRole("row").filter({ hasText: "patchserver" });
    await expect(row.getByText("Running")).toBeVisible({ timeout: 20000 });
});

test("the signed-in panel opens one event socket with nothing in its address, says hello first and gets ready", async ({ page }) => {
    const sockets: { url: string; sent: string[]; received: string[] }[] = [];
    page.on("websocket", (socket) => {
        const seen = { url: socket.url(), sent: [] as string[], received: [] as string[] };
        sockets.push(seen);
        socket.on("framesent", (frame) => seen.sent.push(String(frame.payload)));
        socket.on("framereceived", (frame) => seen.received.push(String(frame.payload)));
    });
    const typeOf = (text: string | undefined) => (text === undefined ? "" : (JSON.parse(text) as { type?: string }).type);

    await page.goto(`${panel.url}/#overview`);
    await page.locator("#sign-in-username").fill(operator.name);
    await page.locator("#sign-in-password").fill(operator.password);
    await page.getByRole("button", { name: "Sign in" }).click();
    await expect(page.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();

    await expect
        .poll(() => sockets.some((socket) => socket.received.some((text) => typeOf(text) === "ready")), { timeout: 20000 })
        .toBe(true);
    expect(sockets).toHaveLength(1);
    expect(sockets[0]?.url).toBe(`${panel.url.replace(/^http/, "ws")}/api/panel/events`);
    expect(typeOf(sockets[0]?.sent[0])).toBe("hello");
    expect(typeOf(sockets[0]?.received[0])).toBe("ready");
});

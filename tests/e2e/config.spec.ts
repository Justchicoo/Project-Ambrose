/*
 * Project Ambrose by Imjustchico
 * The configuration page against a real gameserver run by the supervisor behind its own panel listener, when AMBROSE_E2E_GAMESERVER_CONF names a game server config with its databases and client: from a phone-sized browser, a value out of bounds cannot be reviewed even with a reason given, and changing Rate.Drop.Item with a reason applies it to the running gameserver, the same process before and after, whose own admin API then reports it set live, the newest history row shows the operator who changed it, from what and why, the page never scrolls sideways, and the value is still in force after the gameserver restarts. The setting is reset to its config value before and after, and each reset is checked, so the run can be repeated against the same database; a gameserver that stops before it is ready fails the run at once with its own output.
 */

import { devices, expect, test, type Page } from "@playwright/test";
import { built, gameserver, type GameStack, operator, port, startGameStack, supervisor, token } from "./panel-server";

const gameConfig = process.env.AMBROSE_E2E_GAMESERVER_CONF ?? "";
const AdminPort = port(40);
const PanelPort = port(41);
const GameAdminPort = port(42);
const WorldPort = port(43);
const Reason = `double drops for the weekend event, run ${Date.now().toString(36)}`;

test.skip(
    !supervisor || !gameserver || !built || gameConfig === "",
    "needs the built panel, supervisor and gameserver, and AMBROSE_E2E_GAMESERVER_CONF naming a game server config with its databases and client",
);
test.describe.configure({ mode: "serial", timeout: 45 * 60_000 });
test.use({ actionTimeout: 30_000, navigationTimeout: 60_000, screenshot: "only-on-failure", trace: "retain-on-failure" });

let stack: GameStack;

type Held = { key: string; value: string; layer: string };

async function gameSettings(): Promise<Held[]> {
    const answer = await fetch(`http://127.0.0.1:${GameAdminPort}/api/settings`, {
        headers: { Authorization: `Bearer ${token}` },
        signal: AbortSignal.timeout(10000),
    });
    return ((await answer.json()) as { settings: Held[] }).settings;
}

async function dropRate(): Promise<Held | undefined> {
    return (await gameSettings()).find((setting) => setting.key === "Rate.Drop.Item");
}

type GameState = { state: string; pid: number | null; failedStarts: number };

async function gameState(): Promise<GameState> {
    const answer = await fetch(`${stack.admin}/api/apps/gameserver`, {
        headers: { Authorization: `Bearer ${token}` },
        signal: AbortSignal.timeout(10000),
    });
    const body = (await answer.json()) as { state: string; pid: number | null; failed_starts?: number };
    return { state: body.state, pid: body.pid, failedStarts: body.failed_starts ?? 0 };
}

async function waitUntilRunning(other: number | null | undefined, timeout: number) {
    const failedBefore = (await gameState().catch(() => ({ failedStarts: 0 }))).failedStarts;
    const until = Date.now() + timeout;
    for (;;) {
        const now = await gameState().catch(() => null);
        if (now && now.failedStarts > failedBefore) throw new Error(`the gameserver stopped before it was ready:\n${stack.output()}`);
        if (now && now.state === "running" && (other === undefined || now.pid !== other)) return;
        if (Date.now() > until) throw new Error(`the gameserver was not running after ${timeout / 60_000} minutes:\n${stack.output()}`);
        await new Promise((done) => setTimeout(done, 2000));
    }
}

async function resetDropRate() {
    const answer = await fetch(`${stack.admin}/api/apps/gameserver/api/settings/Rate.Drop.Item`, {
        method: "DELETE",
        headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json" },
        body: JSON.stringify({ reason: "the configuration end-to-end run puts it back" }),
        signal: AbortSignal.timeout(30000),
    });
    expect(answer.status, await answer.text()).toBe(200);
    expect((await dropRate())?.layer).not.toBe("live");
}

async function signIn(page: Page) {
    await page.goto(`${stack.url}/#claim?token=${stack.claim}`);
    await page.locator("#sign-in-token").fill(stack.claim);
    await page.locator("#sign-in-username").fill(operator.name);
    await page.locator("#sign-in-password").fill(operator.password);
    await page.getByRole("button", { name: "Make me the owner" }).click();
    await page.goto(`${stack.url}/#config`);
    await expect(page.getByRole("heading", { name: "Configuration", level: 1 })).toBeVisible();
}

test.beforeAll(async () => {
    test.setTimeout(40 * 60_000);
    stack = await startGameStack(AdminPort, PanelPort, GameAdminPort, WorldPort, gameConfig);
    await waitUntilRunning(undefined, 30 * 60_000);
    await resetDropRate();
});

test.afterAll(async () => {
    test.setTimeout(10 * 60_000);
    if (!stack) return;
    try {
        await waitUntilRunning(undefined, 5 * 60_000);
        await resetDropRate();
    } finally {
        await stack.stop();
    }
});

test("changing Rate.Drop.Item from a phone applies to the running gameserver without a restart, and its history shows who and why", async ({
    browser,
}) => {
    const context = await browser.newContext({ ...devices["Pixel 7"] });
    const page = await context.newPage();
    await signIn(page);
    const before = await dropRate();
    expect(before?.value).not.toBe("2.5");
    const running = await gameState();
    expect(running.state).toBe("running");

    await page.getByLabel("App whose settings are shown").click();
    await page.getByRole("option", { name: "gameserver" }).click();
    await page.getByRole("button", { name: "Change Rate.Drop.Item" }).click();
    await page.locator("#setting-reason").fill(Reason);
    await page.locator("#setting-value").fill("250");
    await expect(page.getByText("Rate.Drop.Item must be from 0 to 100 times")).toBeVisible();
    await expect(page.getByRole("button", { name: "Review" })).toBeDisabled();
    await page.locator("#setting-value").fill("2.5");
    await expect(page.getByRole("button", { name: "Review" })).toBeEnabled();
    await page.getByRole("button", { name: "Review" }).click();
    await page.getByRole("button", { name: "Apply" }).click();
    await expect(page.getByText("Rate.Drop.Item is now 2.5")).toBeVisible();

    const held = await dropRate();
    expect(held?.value).toBe("2.5");
    expect(held?.layer).toBe("live");
    const after = await gameState();
    expect(after.state).toBe("running");
    expect(after.pid).toBe(running.pid);

    await page.getByRole("button", { name: "History of Rate.Drop.Item" }).click();
    const newest = page.getByRole("dialog").locator("li").first();
    await expect(newest).toContainText(Reason);
    await expect(newest).toContainText(`${before?.value} → 2.5`);
    await expect(newest).toContainText(operator.name);
    const history = await fetch(`http://127.0.0.1:${GameAdminPort}/api/settings/Rate.Drop.Item/history`, {
        headers: { Authorization: `Bearer ${token}` },
        signal: AbortSignal.timeout(10000),
    });
    const entries = ((await history.json()) as { entries: { who: string; source: string; reason: string; new: string }[] }).entries;
    expect(entries[0]).toEqual(expect.objectContaining({ new: "2.5", source: "panel", reason: Reason }));
    expect(entries[0].who).toBe(operator.name);

    expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth + 1)).toBe(true);
    await context.close();
});

test("a value changed on the page survives a gameserver restart", async () => {
    const before = await gameState();
    expect((await dropRate())?.value).toBe("2.5");
    const restart = await fetch(`${stack.admin}/api/apps/gameserver/power`, {
        method: "POST",
        headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json" },
        body: JSON.stringify({ action: "restart", seconds: 0 }),
        signal: AbortSignal.timeout(30000),
    });
    expect(restart.status).toBe(202);
    await waitUntilRunning(before.pid, 30 * 60_000);
    const held = await dropRate();
    expect(held?.value).toBe("2.5");
    expect(held?.layer).toBe("live");
});

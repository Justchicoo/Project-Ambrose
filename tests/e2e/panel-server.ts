/*
 * Project Ambrose by Imjustchico
 * Starts a real Ambrose app whose admin API serves the built panel, for the end-to-end and screenshot runs, every port counted from AMBROSE_E2E_PORT_BASE, 12600 unless it is set, and the one operator the runs make: the patchserver the C++ build made, or the one AMBROSE_PANEL_APP names, on a port the caller picks with a known token and its logs in a folder of its own, waits until it answers and says it has finished starting, and stops it again; or the supervisor from the same build, given a folder of its own holding its config and one patchserver to run, so the panel it serves carries another app's state and power buttons that reach it; or the supervisor with its own panel listener on, running one patchserver, with any further config lines the caller adds, such as a two-factor requirement, so the built page can be loaded from the door it will really be opened through and carry an app's state through it, with the one-time link that makes its first operator and the folder holding its config and logs; or that listener running a real gameserver from a config the caller names, with its databases and client, its admin API and world port moved to ports of the caller's choosing, bound to loopback and logging into the run's own folder, and reached for anything but the browser through the supervisor's own admin API, since the panel listener's token only signs a browser in, with the one-time link to make the panel's first operator read from the supervisor's log and the tail of the gameserver's output kept for a failure to name; every panel keeps its store and its keyring in the run's own folder, so no run reads or writes the keyring of the machine it runs on; a stack that fails to start, or is stopped, leaves no process running and no copy of the caller's config behind.
 */

import { spawn, type ChildProcess } from "node:child_process";
import { copyFileSync, existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";

export const token = "0123456789abcdef0123456789abcdef";

export const operator = { name: "merle", password: "a long passphrase for the end-to-end run" };

export function port(offset: number): number {
    const base = Number(process.env.AMBROSE_E2E_PORT_BASE ?? "12600");
    if (!Number.isInteger(base) || base < 1024 || base + offset > 65535) throw new Error("AMBROSE_E2E_PORT_BASE names no usable port");
    return base + offset;
}

const candidates = [
    process.env.AMBROSE_PANEL_APP,
    "build/windows-msvc-x64/bin/Debug/patchserver.exe",
    "build/windows-msvc-x64/bin/RelWithDebInfo/patchserver.exe",
    "build/linux-gcc/bin/Debug/patchserver",
    "build/linux-gcc/bin/Release/patchserver",
];

export const app = candidates
    .filter((file): file is string => typeof file === "string" && file !== "")
    .map((file) => path.resolve(file))
    .find((file) => existsSync(file));

const supervisors = [
    process.env.AMBROSE_SUPERVISOR,
    "build/windows-msvc-x64/bin/Debug/supervisor.exe",
    "build/windows-msvc-x64/bin/RelWithDebInfo/supervisor.exe",
    "build/linux-gcc/bin/Debug/supervisor",
    "build/linux-gcc/bin/Release/supervisor",
];

export const supervisor = supervisors
    .filter((file): file is string => typeof file === "string" && file !== "")
    .map((file) => path.resolve(file))
    .find((file) => existsSync(file));

const gameservers = [
    process.env.AMBROSE_GAMESERVER,
    "build/windows-msvc-x64/bin/Debug/gameserver.exe",
    "build/windows-msvc-x64/bin/RelWithDebInfo/gameserver.exe",
    "build/linux-gcc/bin/Debug/gameserver",
    "build/linux-gcc/bin/Release/gameserver",
];

export const gameserver = gameservers
    .filter((file): file is string => typeof file === "string" && file !== "")
    .map((file) => path.resolve(file))
    .find((file) => existsSync(file));

export const built = existsSync(path.resolve("apps/dashboard/dist/index.html"));

export type Panel = { url: string; stop: () => Promise<void> };

async function answers(url: string): Promise<boolean> {
    try {
        return (await fetch(`${url}/api/health`)).status === 401;
    } catch {
        return false;
    }
}

async function started(admin: string): Promise<boolean> {
    try {
        const answer = await fetch(`${admin}/api/health`, {
            headers: { Authorization: `Bearer ${token}` },
            signal: AbortSignal.timeout(5000),
        });
        return answer.ok && ((await answer.json()) as { state?: string }).state === "running";
    } catch {
        return false;
    }
}

function exited(child: ChildProcess): Promise<void> {
    return new Promise((done) => {
        if (child.exitCode !== null || child.signalCode !== null) done();
        else child.once("exit", () => done());
    });
}

export async function startPanel(port: number): Promise<Panel> {
    if (!app) throw new Error("no built patchserver; build the C++ tree or set AMBROSE_PANEL_APP");
    const logs = mkdtempSync(path.join(tmpdir(), "ambrose-panel-"));
    const child = spawn(
        app,
        [
            "-c",
            path.resolve("src/server/apps/patchserver/patchserver.conf.dist"),
            "--set",
            "BindIP=127.0.0.1",
            "--set",
            "Admin.Enable=1",
            "--set",
            `Admin.Port=${port}`,
            "--set",
            `Admin.Token=${token}`,
            "--set",
            `Admin.DashboardDir=${path.resolve("apps/dashboard/dist")}`,
            "--set",
            `LogsDir=${logs}`,
        ],
        { stdio: "ignore" },
    );
    const url = `http://127.0.0.1:${port}`;
    const deadline = Date.now() + 20000;
    while (!(await answers(url)) || !(await started(url))) {
        if (Date.now() > deadline || child.exitCode !== null) {
            child.kill();
            throw new Error(`the patchserver did not start its admin API on port ${port}`);
        }
        await new Promise((done) => setTimeout(done, 200));
    }
    return {
        url,
        stop: async () => {
            child.kill();
            await exited(child);
            rmSync(logs, { recursive: true, force: true });
        },
    };
}

export async function startSupervisor(port: number, appPort: number): Promise<Panel> {
    if (!supervisor || !app) throw new Error("no built supervisor and patchserver; build the C++ tree");
    const folder = mkdtempSync(path.join(tmpdir(), "ambrose-supervisor-"));
    const forward = (file: string) => path.resolve(file).split("\\").join("/");
    copyFileSync(path.resolve("src/server/apps/supervisor/supervisor.conf.dist"), path.join(folder, "supervisor.conf.dist"));
    copyFileSync(path.resolve("src/server/apps/patchserver/patchserver.conf.dist"), path.join(folder, "patchserver.conf.dist"));
    writeFileSync(
        path.join(folder, "supervisor.conf"),
        [
            "Supervisor.Apps = patchserver",
            `App.patchserver.Program = "${forward(app)}"`,
            `App.patchserver.Config = "${forward(path.join(folder, "patchserver.conf"))}"`,
            "Supervisor.StateFile = state.json",
            "Supervisor.OutputDir = output",
            "Console.Enable = 0",
            "Admin.Enable = 1",
            "Admin.BindIP = 127.0.0.1",
            `Admin.Port = ${port}`,
            `Admin.Token = ${token}`,
            `Admin.DashboardDir = "${forward("apps/dashboard/dist")}"`,
            `LogsDir = "${forward(path.join(folder, "logs"))}"`,
            "",
        ].join("\n"),
    );
    writeFileSync(
        path.join(folder, "patchserver.conf"),
        [
            "BindIP = 127.0.0.1",
            `PatchServerPort = ${appPort}`,
            "Admin.Enable = 0",
            "Console.Colors = 0",
            `LogsDir = "${forward(path.join(folder, "logs"))}"`,
            "",
        ].join("\n"),
    );
    const child = spawn(supervisor, ["-c", path.join(folder, "supervisor.conf")], { cwd: folder, stdio: "ignore" });
    const url = `http://127.0.0.1:${port}`;
    const deadline = Date.now() + 20000;
    while (!(await answers(url)) || !(await started(url))) {
        if (Date.now() > deadline || child.exitCode !== null) {
            child.kill();
            throw new Error(`the supervisor did not start its admin API on port ${port}`);
        }
        await new Promise((done) => setTimeout(done, 200));
    }
    return {
        url,
        stop: async () => {
            await stopApp(url, folder, "patchserver", "kill");
            child.kill();
            await exited(child);
            rmSync(folder, { recursive: true, force: true, maxRetries: 20, retryDelay: 500 });
        },
    };
}

export async function startPanelListener(
    adminPort: number,
    panelPort: number,
    appPort: number,
    extra: readonly string[] = [],
): Promise<PanelListener> {
    if (!supervisor || !app) throw new Error("no built supervisor and patchserver; build the C++ tree");
    const folder = mkdtempSync(path.join(tmpdir(), "ambrose-panel-listener-"));
    const forward = (file: string) => path.resolve(file).split("\\").join("/");
    copyFileSync(path.resolve("src/server/apps/supervisor/supervisor.conf.dist"), path.join(folder, "supervisor.conf.dist"));
    copyFileSync(path.resolve("src/server/apps/patchserver/patchserver.conf.dist"), path.join(folder, "patchserver.conf.dist"));
    writeFileSync(
        path.join(folder, "patchserver.conf"),
        [
            "BindIP = 127.0.0.1",
            `PatchServerPort = ${appPort}`,
            "Admin.Enable = 0",
            "Console.Colors = 0",
            `LogsDir = "${forward(path.join(folder, "logs"))}"`,
            "",
        ].join("\n"),
    );
    writeFileSync(
        path.join(folder, "supervisor.conf"),
        [
            "Supervisor.Apps = patchserver",
            `App.patchserver.Program = "${forward(app)}"`,
            `App.patchserver.Config = "${forward(path.join(folder, "patchserver.conf"))}"`,
            "Supervisor.StateFile = state.json",
            "Supervisor.OutputDir = output",
            "Console.Enable = 0",
            "Admin.Enable = 1",
            "Admin.BindIP = 127.0.0.1",
            `Admin.Port = ${adminPort}`,
            `Admin.Token = ${token}`,
            "Panel.Enable = 1",
            "Panel.BindIP = 127.0.0.1",
            `Panel.Port = ${panelPort}`,
            `Panel.Token = ${token}`,
            `Panel.DashboardDir = "${forward("apps/dashboard/dist")}"`,
            `Panel.StoreFile = "${forward(path.join(folder, "panel.sqlite3"))}"`,
            `Panel.KeyringFile = "${forward(path.join(folder, "keyring"))}"`,
            ...extra,
            `LogsDir = "${forward(path.join(folder, "logs"))}"`,
            "",
        ].join("\n"),
    );
    const child = spawn(supervisor, ["-c", path.join(folder, "supervisor.conf")], { cwd: folder, stdio: "ignore" });
    const url = `http://127.0.0.1:${panelPort}`;
    const deadline = Date.now() + 20000;
    while (!(await answers(url)) || !(await started(`http://127.0.0.1:${adminPort}`))) {
        if (Date.now() > deadline || child.exitCode !== null) {
            child.kill();
            throw new Error(`the supervisor did not start its panel listener on port ${panelPort}`);
        }
        await new Promise((done) => setTimeout(done, 200));
    }
    const claim = await claimLink(folder);
    return {
        url,
        claim,
        folder,
        stop: async () => {
            await stopApp(`http://127.0.0.1:${adminPort}`, folder, "patchserver", "kill");
            child.kill();
            await exited(child);
            rmSync(folder, { recursive: true, force: true, maxRetries: 20, retryDelay: 500 });
        },
    };
}

export type PanelListener = Panel & { claim: string; folder: string };
export type GameStack = PanelListener & { admin: string; output: () => string };

async function claimLink(folder: string): Promise<string> {
    const log = path.join(folder, "logs", "Supervisor.log");
    const deadline = Date.now() + 20000;
    while (Date.now() < deadline) {
        const found = existsSync(log) ? /#claim\?token=([A-Za-z0-9_-]+)/.exec(readFileSync(log, "utf8")) : null;
        if (found) return found[1];
        await new Promise((done) => setTimeout(done, 200));
    }
    throw new Error("the supervisor printed no link to make the panel's first operator");
}

async function stopApp(admin: string, folder: string, name: string, action: "stop" | "kill"): Promise<void> {
    await fetch(`${admin}/api/apps/${name}/power`, {
        method: "POST",
        headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json" },
        body: JSON.stringify({ action, seconds: 0 }),
        signal: AbortSignal.timeout(10000),
    }).catch(() => undefined);
    const until = Date.now() + (action === "stop" ? 60000 : 15000);
    while (Date.now() < until) {
        const running = await fetch(`${admin}/api/supervisor`, {
            headers: { Authorization: `Bearer ${token}` },
            signal: AbortSignal.timeout(5000),
        })
            .then((answer) => answer.json() as Promise<{ apps?: { name: string; state: string }[] }>)
            .then(
                (snapshot) =>
                    snapshot.apps?.some((entry) => entry.name === name && entry.state !== "offline" && entry.state !== "crashed") ?? true,
            )
            .catch(() => true);
        if (!running) return;
        await new Promise((done) => setTimeout(done, 500));
    }
    if (action === "stop") return stopApp(admin, folder, name, "kill");
    const kept = path.join(folder, "state.json");
    const pid = existsSync(kept)
        ? (JSON.parse(readFileSync(kept, "utf8")) as { apps?: Record<string, { process?: { id?: number } | null }> }).apps?.[name]?.process
              ?.id
        : undefined;
    if (typeof pid === "number") {
        try {
            process.kill(pid);
        } catch {
            return;
        }
    }
}

export async function startGameStack(
    adminPort: number,
    panelPort: number,
    gameAdminPort: number,
    worldPort: number,
    gameConfig: string,
): Promise<GameStack> {
    if (!supervisor || !gameserver) throw new Error("no built supervisor and gameserver; build the C++ tree");
    const folder = mkdtempSync(path.join(tmpdir(), "ambrose-game-stack-"));
    const forward = (file: string) => path.resolve(file).split("\\").join("/");
    copyFileSync(path.resolve("src/server/apps/supervisor/supervisor.conf.dist"), path.join(folder, "supervisor.conf.dist"));
    copyFileSync(path.resolve("src/server/apps/gameserver/gameserver.conf.dist"), path.join(folder, "gameserver.conf.dist"));
    const kept = readFileSync(gameConfig, "utf8")
        .split(/\r?\n/)
        .filter((line) => !/^\s*(Admin\.|Console\.Enable\b|BindIP\b|WorldServerPort\b|LogsDir\b)/.test(line));
    writeFileSync(
        path.join(folder, "gameserver.conf"),
        [
            ...kept,
            "BindIP = 127.0.0.1",
            `WorldServerPort = ${worldPort}`,
            `LogsDir = "${forward(path.join(folder, "logs", "gameserver"))}"`,
            "Admin.Enable = 1",
            "Admin.BindIP = 127.0.0.1",
            `Admin.Port = ${gameAdminPort}`,
            `Admin.Token = ${token}`,
            "Console.Enable = 0",
            "",
        ].join("\n"),
    );
    writeFileSync(
        path.join(folder, "supervisor.conf"),
        [
            "Supervisor.Apps = gameserver",
            `App.gameserver.Program = "${forward(gameserver)}"`,
            `App.gameserver.Config = "${forward(path.join(folder, "gameserver.conf"))}"`,
            "App.gameserver.StartTimeout = 1800",
            "Supervisor.StateFile = state.json",
            "Supervisor.OutputDir = output",
            "Console.Enable = 0",
            "Admin.Enable = 1",
            "Admin.BindIP = 127.0.0.1",
            `Admin.Port = ${adminPort}`,
            `Admin.Token = ${token}`,
            "Panel.Enable = 1",
            "Panel.BindIP = 127.0.0.1",
            `Panel.Port = ${panelPort}`,
            `Panel.Token = ${token}`,
            `Panel.DashboardDir = "${forward("apps/dashboard/dist")}"`,
            `Panel.StoreFile = "${forward(path.join(folder, "panel.sqlite3"))}"`,
            `Panel.KeyringFile = "${forward(path.join(folder, "keyring"))}"`,
            `LogsDir = "${forward(path.join(folder, "logs"))}"`,
            "",
        ].join("\n"),
    );
    const child = spawn(supervisor, ["-c", path.join(folder, "supervisor.conf")], { cwd: folder, stdio: "ignore" });
    const url = `http://127.0.0.1:${panelPort}`;
    const admin = `http://127.0.0.1:${adminPort}`;
    const stop = async () => {
        await stopApp(admin, folder, "gameserver", "stop");
        child.kill();
        await exited(child);
        rmSync(folder, { recursive: true, force: true, maxRetries: 20, retryDelay: 500 });
    };
    const output = () => {
        const captured = path.join(folder, "output", "gameserver", "current.out");
        return existsSync(captured) ? readFileSync(captured, "utf8").split(/\r?\n/).slice(-20).join("\n") : "";
    };
    try {
        const deadline = Date.now() + 20000;
        while (!(await answers(url)) || !(await started(admin))) {
            if (Date.now() > deadline || child.exitCode !== null)
                throw new Error(`the supervisor did not start its panel listener on port ${panelPort}`);
            await new Promise((done) => setTimeout(done, 200));
        }
        const claim = await claimLink(folder);
        return { url, admin, claim, folder, output, stop };
    } catch (failure) {
        await stop();
        throw failure;
    }
}

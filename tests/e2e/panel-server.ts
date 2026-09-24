/*
 * Project Ambrose by Imjustchico
 * Starts a real Ambrose app whose admin API serves the built panel, for the end-to-end and screenshot runs: the patchserver the C++ build made, or the one AMBROSE_PANEL_APP names, on a port the caller picks with a known token and its logs in a folder of its own, waits until it answers, and stops it again; or the supervisor from the same build, given a folder of its own holding its config and one patchserver to run, so the panel it serves carries another app's state and power buttons that reach it; or the supervisor with its own panel listener on, running one patchserver, so the built page can be loaded from the door it will really be opened through and carry an app's state through it.
 */

import { spawn, type ChildProcess } from "node:child_process";
import { copyFileSync, existsSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";

export const token = "0123456789abcdef0123456789abcdef";

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

export const built = existsSync(path.resolve("apps/dashboard/dist/index.html"));

export type Panel = { url: string; stop: () => Promise<void> };

async function answers(url: string): Promise<boolean> {
    try {
        return (await fetch(`${url}/api/health`)).status === 401;
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
    while (!(await answers(url))) {
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
        ["BindIP = 127.0.0.1", `PatchServerPort = ${appPort}`, "Admin.Enable = 0", "Console.Colors = 0", `LogsDir = "${forward(path.join(folder, "logs"))}"`, ""].join("\n"),
    );
    const child = spawn(supervisor, ["-c", path.join(folder, "supervisor.conf")], { cwd: folder, stdio: "ignore" });
    const url = `http://127.0.0.1:${port}`;
    const deadline = Date.now() + 20000;
    while (!(await answers(url))) {
        if (Date.now() > deadline || child.exitCode !== null) {
            child.kill();
            throw new Error(`the supervisor did not start its admin API on port ${port}`);
        }
        await new Promise((done) => setTimeout(done, 200));
    }
    return {
        url,
        stop: async () => {
            await fetch(`${url}/api/apps/patchserver/power`, {
                method: "POST",
                headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json" },
                body: JSON.stringify({ action: "kill" }),
            }).catch(() => undefined);
            child.kill();
            await exited(child);
            rmSync(folder, { recursive: true, force: true });
        },
    };
}

export async function startPanelListener(adminPort: number, panelPort: number, appPort: number): Promise<Panel> {
    if (!supervisor || !app) throw new Error("no built supervisor and patchserver; build the C++ tree");
    const folder = mkdtempSync(path.join(tmpdir(), "ambrose-panel-listener-"));
    const forward = (file: string) => path.resolve(file).split("\\").join("/");
    copyFileSync(path.resolve("src/server/apps/supervisor/supervisor.conf.dist"), path.join(folder, "supervisor.conf.dist"));
    copyFileSync(path.resolve("src/server/apps/patchserver/patchserver.conf.dist"), path.join(folder, "patchserver.conf.dist"));
    writeFileSync(
        path.join(folder, "patchserver.conf"),
        ["BindIP = 127.0.0.1", `PatchServerPort = ${appPort}`, "Admin.Enable = 0", "Console.Colors = 0", `LogsDir = "${forward(path.join(folder, "logs"))}"`, ""].join("\n"),
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
            `LogsDir = "${forward(path.join(folder, "logs"))}"`,
            "",
        ].join("\n"),
    );
    const child = spawn(supervisor, ["-c", path.join(folder, "supervisor.conf")], { cwd: folder, stdio: "ignore" });
    const url = `http://127.0.0.1:${panelPort}`;
    const deadline = Date.now() + 20000;
    while (!(await answers(url))) {
        if (Date.now() > deadline || child.exitCode !== null) {
            child.kill();
            throw new Error(`the supervisor did not start its panel listener on port ${panelPort}`);
        }
        await new Promise((done) => setTimeout(done, 200));
    }
    return {
        url,
        stop: async () => {
            await fetch(`${url}/api/apps/patchserver/power`, {
                method: "POST",
                headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json" },
                body: JSON.stringify({ action: "kill" }),
            }).catch(() => undefined);
            child.kill();
            await exited(child);
            rmSync(folder, { recursive: true, force: true });
        },
    };
}

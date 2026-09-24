/*
 * Project Ambrose by Imjustchico
 * The panel's route table: every page with its path, title, icon, the permission it needs, whether it appears in the side bar and under which heading, and how it is drawn: its own page loaded on first visit, a page whose milestone has not landed that names it and offers a design preview, or the access-denied page; the check that holds every entry complete, and the one decision of what a path shows a caller with a given set of permissions.
 */

import type { Component } from "svelte";
import ActivityIcon from "@lucide/svelte/icons/activity";
import ArchiveIcon from "@lucide/svelte/icons/archive";
import DatabaseIcon from "@lucide/svelte/icons/database";
import FileTextIcon from "@lucide/svelte/icons/file-text";
import GaugeIcon from "@lucide/svelte/icons/gauge";
import GlobeIcon from "@lucide/svelte/icons/globe";
import HardDriveIcon from "@lucide/svelte/icons/hard-drive";
import LockIcon from "@lucide/svelte/icons/lock";
import ServerIcon from "@lucide/svelte/icons/server";
import SettingsIcon from "@lucide/svelte/icons/settings";
import ShieldIcon from "@lucide/svelte/icons/shield";
import SquareTerminalIcon from "@lucide/svelte/icons/square-terminal";
import UserIcon from "@lucide/svelte/icons/user";
import UsersIcon from "@lucide/svelte/icons/users";

export type PreviewProps = { which: string; title: string };
export type PageLoader = () => Promise<{ default: Component }>;
export type ListLoader = () => Promise<{ default: Component<PreviewProps> }>;
export type Preview = { kind: "page"; load: PageLoader } | { kind: "list"; load: ListLoader };

export type RouteView = { kind: "page"; load: PageLoader } | { kind: "arrives"; milestone: string; preview: Preview } | { kind: "denied" };

export type Route = {
    path: string;
    title: string;
    icon: Component;
    permission: string;
    nav: boolean;
    group: "Servers" | "Game" | "Panel";
    view: RouteView;
};

const list: Preview = { kind: "list", load: () => import("./pages/Lists.svelte") };

export const routes: Route[] = [
    {
        path: "overview",
        title: "Overview",
        icon: GaugeIcon,
        permission: "status.read",
        nav: true,
        group: "Servers",
        view: { kind: "page", load: () => import("./pages/Overview.svelte") },
    },
    {
        path: "servers",
        title: "Servers",
        icon: ServerIcon,
        permission: "apps.read",
        nav: true,
        group: "Servers",
        view: { kind: "page", load: () => import("./pages/Servers.svelte") },
    },
    {
        path: "logs",
        title: "Logs",
        icon: FileTextIcon,
        permission: "logs.read",
        nav: true,
        group: "Servers",
        view: { kind: "arrives", milestone: "17.07", preview: { kind: "page", load: () => import("./pages/Logs.svelte") } },
    },
    {
        path: "console",
        title: "Console",
        icon: SquareTerminalIcon,
        permission: "commands.run",
        nav: true,
        group: "Servers",
        view: { kind: "arrives", milestone: "17.07", preview: { kind: "page", load: () => import("./pages/Console.svelte") } },
    },
    {
        path: "database",
        title: "Database",
        icon: DatabaseIcon,
        permission: "database.read",
        nav: true,
        group: "Servers",
        view: { kind: "page", load: () => import("./pages/Database.svelte") },
    },
    {
        path: "backups",
        title: "Backups",
        icon: ArchiveIcon,
        permission: "backups.read",
        nav: true,
        group: "Servers",
        view: { kind: "arrives", milestone: "17.16", preview: list },
    },
    {
        path: "realms",
        title: "Realms and zones",
        icon: GlobeIcon,
        permission: "realms.read",
        nav: true,
        group: "Game",
        view: { kind: "arrives", milestone: "17.31", preview: list },
    },
    {
        path: "players",
        title: "Players online",
        icon: UserIcon,
        permission: "players.read",
        nav: true,
        group: "Game",
        view: { kind: "arrives", milestone: "17.21", preview: list },
    },
    {
        path: "accounts",
        title: "Accounts and bans",
        icon: ShieldIcon,
        permission: "accounts.read",
        nav: true,
        group: "Game",
        view: { kind: "arrives", milestone: "17.21", preview: list },
    },
    {
        path: "client",
        title: "Client data",
        icon: HardDriveIcon,
        permission: "client.read",
        nav: true,
        group: "Game",
        view: { kind: "arrives", milestone: "17.20", preview: { kind: "page", load: () => import("./pages/ClientData.svelte") } },
    },
    {
        path: "users",
        title: "Panel users",
        icon: UsersIcon,
        permission: "panel.users.read",
        nav: true,
        group: "Panel",
        view: { kind: "arrives", milestone: "17.50", preview: list },
    },
    {
        path: "settings",
        title: "Settings",
        icon: SettingsIcon,
        permission: "settings.read",
        nav: true,
        group: "Panel",
        view: { kind: "page", load: () => import("./pages/Settings.svelte") },
    },
    {
        path: "activity",
        title: "Activity",
        icon: ActivityIcon,
        permission: "audit.read",
        nav: true,
        group: "Panel",
        view: { kind: "arrives", milestone: "17.25", preview: list },
    },
    { path: "denied", title: "Access denied", icon: LockIcon, permission: "none", nav: false, group: "Panel", view: { kind: "denied" } },
];

export const everything = new Set(["*"]);

export function checkRoutes(table: readonly Partial<Route>[]): string[] {
    const problems: string[] = [];
    const seen = new Set<string>();
    table.forEach((route, index) => {
        const name = typeof route.path === "string" && route.path !== "" ? route.path : `entry ${index}`;
        if (typeof route.path !== "string" || !/^[a-z][a-z-]*$/.test(route.path))
            problems.push(`${name}: the path is missing or not lower-case words`);
        else if (seen.has(route.path)) problems.push(`${name}: the path appears twice`);
        else seen.add(route.path);
        if (typeof route.title !== "string" || route.title.trim() === "") problems.push(`${name}: names no title`);
        if (typeof route.permission !== "string" || route.permission.trim() === "") problems.push(`${name}: names no permission`);
        if (typeof route.nav !== "boolean") problems.push(`${name}: leaves out its navigation flag`);
        if (route.group !== "Servers" && route.group !== "Game" && route.group !== "Panel")
            problems.push(`${name}: names no side bar group`);
        if (!route.view) problems.push(`${name}: says nothing about how it is drawn`);
        else if (route.view.kind === "arrives" && !/^\d+\.\d+$/.test(route.view.milestone))
            problems.push(`${name}: names no milestone that builds it`);
    });
    return problems;
}

export function canUse(route: Route, granted: ReadonlySet<string>): boolean {
    return route.permission === "none" || granted.has("*") || granted.has(route.permission);
}

export type Resolved = { kind: "missing"; path: string } | { kind: "refused"; route: Route } | { kind: "shown"; route: Route };

export function resolve(path: string, granted: ReadonlySet<string>): Resolved {
    const route = routes.find((entry) => entry.path === path);
    if (!route) return { kind: "missing", path };
    return canUse(route, granted) ? { kind: "shown", route } : { kind: "refused", route };
}

export function navigation(granted: ReadonlySet<string>): Route[] {
    return routes.filter((route) => route.nav && canUse(route, granted));
}

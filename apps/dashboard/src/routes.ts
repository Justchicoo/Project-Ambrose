/*
 * Project Ambrose by Imjustchico
 * The panel's route table: every page with its path, title, icon, the permission it needs, none for a page about the operator's own account, whether it appears in the side bar and under which heading, and how it is drawn: its own page loaded on first visit, a page whose milestone has not landed that names it and offers a design preview, or the access-denied page; the check that holds every entry complete, and the one decision of what a path shows a caller with a given set of permissions.
 */

import type { Component } from "svelte";
import ActivityIcon from "@lucide/svelte/icons/activity";
import ArchiveIcon from "@lucide/svelte/icons/archive";
import ChartLineIcon from "@lucide/svelte/icons/chart-line";
import DatabaseIcon from "@lucide/svelte/icons/database";
import BugIcon from "@lucide/svelte/icons/bug";
import FileTextIcon from "@lucide/svelte/icons/file-text";
import FolderIcon from "@lucide/svelte/icons/folder";
import GaugeIcon from "@lucide/svelte/icons/gauge";
import RefreshCwIcon from "@lucide/svelte/icons/refresh-cw";
import GlobeIcon from "@lucide/svelte/icons/globe";
import HardDriveIcon from "@lucide/svelte/icons/hard-drive";
import LockIcon from "@lucide/svelte/icons/lock";
import ServerIcon from "@lucide/svelte/icons/server";
import SettingsIcon from "@lucide/svelte/icons/settings";
import SlidersHorizontalIcon from "@lucide/svelte/icons/sliders-horizontal";
import ShieldIcon from "@lucide/svelte/icons/shield";
import ShieldCheckIcon from "@lucide/svelte/icons/shield-check";
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
        permission: "status.read",
        nav: true,
        group: "Servers",
        view: { kind: "page", load: () => import("./pages/Servers.svelte") },
    },
    {
        path: "logs",
        title: "Logs",
        icon: FileTextIcon,
        permission: "console.read",
        nav: true,
        group: "Servers",
        view: { kind: "page", load: () => import("./pages/Logs.svelte") },
    },
    {
        path: "console",
        title: "Console",
        icon: SquareTerminalIcon,
        permission: "console.write",
        nav: true,
        group: "Servers",
        view: { kind: "page", load: () => import("./pages/Console.svelte") },
    },
    {
        path: "metrics",
        title: "Metrics",
        icon: ChartLineIcon,
        permission: "metrics.read",
        nav: true,
        group: "Servers",
        view: { kind: "page", load: () => import("./pages/Metrics.svelte") },
    },
    {
        path: "resources",
        title: "Resources",
        icon: ChartLineIcon,
        permission: "metrics.read",
        nav: true,
        group: "Servers",
        view: { kind: "page", load: () => import("./pages/Graphs.svelte") },
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
        path: "config",
        title: "Configuration",
        icon: SlidersHorizontalIcon,
        permission: "settings.read",
        nav: true,
        group: "Servers",
        view: { kind: "page", load: () => import("./pages/Config.svelte") },
    },
    {
        path: "files",
        title: "Files",
        icon: FolderIcon,
        permission: "files.list",
        nav: true,
        group: "Servers",
        view: { kind: "page", load: () => import("./pages/Files.svelte") },
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
        view: { kind: "page", load: () => import("./pages/Realms.svelte") },
    },
    {
        path: "players",
        title: "Players online",
        icon: UserIcon,
        permission: "players.read",
        nav: true,
        group: "Game",
        view: { kind: "page", load: () => import("./pages/Players.svelte") },
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
        path: "registrations",
        title: "Player registrations",
        icon: UsersIcon,
        permission: "accounts.read&accounts.registration",
        nav: true,
        group: "Game",
        view: { kind: "page", load: () => import("./pages/Registrations.svelte") },
    },
    {
        path: "client",
        title: "Client data",
        icon: HardDriveIcon,
        permission: "clientdata.read",
        nav: true,
        group: "Game",
        view: { kind: "page", load: () => import("./pages/ClientData.svelte") },
    },
    {
        path: "users",
        title: "Panel users",
        icon: UsersIcon,
        permission: "users.read",
        nav: true,
        group: "Panel",
        view: { kind: "arrives", milestone: "17.50", preview: list },
    },
    {
        path: "settings",
        title: "Settings",
        icon: SettingsIcon,
        permission: "panel.settings",
        nav: true,
        group: "Panel",
        view: { kind: "page", load: () => import("./pages/Settings.svelte") },
    },
    {
        path: "reload",
        title: "Reload",
        icon: RefreshCwIcon,
        permission: "reload.read",
        nav: true,
        group: "Panel",
        view: { kind: "page", load: () => import("./pages/Reload.svelte") },
    },
    {
        path: "activity",
        title: "Activity",
        icon: ActivityIcon,
        permission: "activity.read",
        nav: true,
        group: "Panel",
        view: { kind: "page", load: () => import("./pages/Activity.svelte") },
    },
    {
        path: "errors",
        title: "Error reports",
        icon: BugIcon,
        permission: "errors.read",
        nav: true,
        group: "Panel",
        view: { kind: "page", load: () => import("./pages/Errors.svelte") },
    },
    {
        path: "two-factor",
        title: "Two-factor sign-in",
        icon: ShieldCheckIcon,
        permission: "none",
        nav: false,
        group: "Panel",
        view: { kind: "page", load: () => import("./pages/TwoFactor.svelte") },
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
    return route.permission === "none" || granted.has("*") || route.permission.split("&").every((permission) => granted.has(permission));
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

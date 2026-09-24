/*
 * Project Ambrose by Imjustchico
 * Tests the panel's route table: every entry names a permission, a navigation flag, a group and how it is drawn, the check itself fails on an entry that leaves any of them out, a page whose milestone has not landed names it, and a direct link to a route the caller may not use resolves to the access-denied page while the side bar leaves it out.
 */

import { describe, expect, it } from "vitest";
import { checkRoutes, everything, navigation, resolve, routes, type Route } from "./routes";

describe("the route table", () => {
    it("holds every entry complete", () => {
        expect(checkRoutes(routes)).toEqual([]);
    });

    it("fails an entry that names no permission or leaves out its navigation flag", () => {
        const complete = routes[0];
        const noPermission: Partial<Route> = { ...complete, path: "broken-one", permission: "" };
        const noFlag: Partial<Route> = { ...complete, path: "broken-two" };
        delete noFlag.nav;
        const problems = checkRoutes([...routes, noPermission, noFlag]);
        expect(problems).toContain("broken-one: names no permission");
        expect(problems).toContain("broken-two: leaves out its navigation flag");
        expect(problems).toHaveLength(2);
    });

    it("fails a repeated path, a missing group and a page with no milestone", () => {
        const complete = routes[0];
        const problems = checkRoutes([
            complete,
            { ...complete },
            { ...complete, path: "ungrouped", group: undefined },
            {
                ...complete,
                path: "unplanned",
                view: { kind: "arrives", milestone: "soon", preview: { kind: "list", load: async () => import("./pages/Lists.svelte") } },
            },
        ]);
        expect(problems).toEqual([
            "overview: the path appears twice",
            "ungrouped: names no side bar group",
            "unplanned: names no milestone that builds it",
        ]);
    });

    it("names a milestone for every page that does not read the server yet", () => {
        for (const route of routes) if (route.view.kind === "arrives") expect(route.view.milestone).toMatch(/^17\.\d+$/);
        expect(routes.find((route) => route.path === "overview")?.view.kind).toBe("page");
    });
});

describe("what a path shows", () => {
    it("shows a route the caller may use and refuses one it may not", () => {
        const statusOnly = new Set(["status.read"]);
        const overview = resolve("overview", statusOnly);
        expect(overview.kind).toBe("shown");
        const logs = resolve("logs", statusOnly);
        expect(logs.kind).toBe("refused");
        if (logs.kind === "refused") expect(logs.route.permission).toBe("logs.read");
        expect(resolve("logs", everything).kind).toBe("shown");
    });

    it("says a path that names no route is missing", () => {
        expect(resolve("nowhere", everything)).toEqual({ kind: "missing", path: "nowhere" });
    });

    it("leaves a route the caller may not use and every hidden route out of the side bar", () => {
        const statusOnly = new Set(["status.read"]);
        expect(navigation(statusOnly).map((route) => route.path)).toEqual(["overview"]);
        expect(navigation(everything).some((route) => route.path === "denied")).toBe(false);
        expect(navigation(everything)).toHaveLength(routes.filter((route) => route.nav).length);
    });
});

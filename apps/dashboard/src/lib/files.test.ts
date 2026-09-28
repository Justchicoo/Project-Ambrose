/*
 * Project Ambrose by Imjustchico
 * Tests what the files page decides before it asks the server: a path splits into breadcrumbs from its root, a query and the page's address carry each value encoded once and read back the same, each control takes the first reason that applies, the root's policy, then a missing permission, then the milestone that brings it, a rule on one row narrows its folder's policy, and a selection of the whole filtered set is counted from the server's total rather than from the rows on screen.
 */

import { describe, expect, it } from "vitest";
import {
    breadcrumbs,
    childPath,
    controlState,
    describeSelection,
    filesQuery,
    folderControls,
    isSelected,
    nothing,
    offersEveryMatch,
    pageHash,
    pageState,
    parentPath,
    readPageHash,
    rowControls,
    rowPolicy,
    selectMatching,
    selectPage,
    selectionCount,
    toggleRow,
    viewState,
    type PolicyLike,
} from "./files";

const writable: PolicyLike = {
    summary: "Writable: local SQL that is never upstreamed",
    operations: {
        list: { allowed: true },
        read: { allowed: true },
        upload: { allowed: true },
        create: { allowed: true },
        delete: { allowed: true },
    },
};

const readOnly: PolicyLike = {
    summary: "Read-only: the running build, which changes only through updates",
    operations: {
        list: { allowed: true },
        read: { allowed: true },
        upload: {
            allowed: false,
            code: "refused_by_policy",
            reason: "The install root is the running build; it changes only through updates",
        },
        create: {
            allowed: false,
            code: "refused_by_policy",
            reason: "The install root is the running build; it changes only through updates",
        },
    },
};

const everything = () => true;

describe("files page logic", () => {
    it("splits a path into breadcrumbs from the root", () => {
        expect(breadcrumbs("data", "")).toEqual([{ name: "data", path: "" }]);
        expect(breadcrumbs("data", "types/r806919")).toEqual([
            { name: "data", path: "" },
            { name: "types", path: "types" },
            { name: "r806919", path: "types/r806919" },
        ]);
        expect(childPath("", "types")).toBe("types");
        expect(childPath("types", "a b")).toBe("types/a b");
        expect(parentPath("types/r806919")).toBe("types");
        expect(parentPath("types")).toBe("");
    });

    it("writes the query with each value encoded once and reads the page's address back", () => {
        expect(filesQuery({ path: "../x", q: "a+b c", offset: 0, reveal: true, sort: "" })).toBe(
            "path=..%2Fx&q=a%2Bb%20c&offset=0&reveal=1",
        );
        expect(filesQuery({ path: "%2e%2e" })).toBe("path=%252e%252e");
        const hash = pageHash("logs", "apps/game server");
        expect(hash).toBe("#files?root=logs&path=apps%2Fgame%20server");
        expect(readPageHash(hash)).toEqual({ root: "logs", path: "apps/game server" });
        expect(readPageHash("#files")).toEqual({ root: "", path: "" });
    });

    it("gives each control its state: the root's policy first, then a missing permission, then the milestone that brings it", () => {
        const upload = folderControls.find((control) => control.id === "upload")!;
        const refused = controlState(upload, readOnly, () => false);
        expect(refused.enabled).toBe(false);
        expect(refused.because).toBe("policy");
        expect(refused.reason).toContain("running build");

        const missing = controlState(upload, writable, (permission) => permission !== "files.upload");
        expect(missing.because).toBe("permission");
        expect(missing.reason).toContain("files.upload");

        const later = controlState(upload, writable, everything);
        expect(later.because).toBe("milestone");
        expect(later.reason).toContain("17.54");
        expect(
            controlState(
                rowControls.find((control) => control.id === "edit")!,
                writable,
                everything,
            ).reason,
        ).toContain("17.53");
        expect(
            controlState(
                rowControls.find((control) => control.id === "delete")!,
                writable,
                everything,
            ).reason,
        ).toContain("17.55");
        expect(
            controlState(
                rowControls.find((control) => control.id === "archive")!,
                writable,
                everything,
            ).reason,
        ).toContain("17.39");

        expect(viewState(writable, everything).enabled).toBe(true);
        expect(viewState(writable, () => false).because).toBe("permission");
    });

    it("narrows a folder's policy by the rule on one row", () => {
        const derived = rowPolicy(writable, { effect: "client_derived", pattern: "*", why: "Built from your own Wizard101 install" });
        expect(derived.operations.list?.allowed).toBe(true);
        expect(derived.operations.read?.allowed).toBe(false);
        expect(viewState(derived, everything).reason).toBe("Built from your own Wizard101 install");
        const fixed = rowPolicy(writable, { effect: "read_only", pattern: "*.lock", why: "Locks stay" });
        expect(fixed.operations.read?.allowed).toBe(true);
        expect(fixed.operations.delete?.allowed).toBe(false);
        expect(rowPolicy(writable, null)).toBe(writable);
    });

    it("counts a selection of the whole filtered set from the server's total", () => {
        const visible = ["a.log", "b.log", "c.log"];
        let selection = toggleRow(nothing, "a.log", visible);
        expect(selectionCount(selection, 250)).toBe(1);
        expect(pageState(selection, visible)).toBe("some");
        selection = selectPage(selection, visible, true);
        expect(pageState(selection, visible)).toBe("all");
        expect(describeSelection(selection, 250)).toBe("3 selected");
        expect(offersEveryMatch(selection, visible, 250)).toBe(true);

        selection = selectMatching("log");
        expect(selectionCount(selection, 250)).toBe(250);
        expect(describeSelection(selection, 250)).toBe('All 250 matching "log" selected');
        expect(isSelected(selection, "anything.log")).toBe(true);
        expect(offersEveryMatch(selection, visible, 250)).toBe(false);

        selection = toggleRow(selection, "b.log", visible);
        expect(selection).toEqual({ kind: "rows", names: ["a.log", "c.log"] });
        expect(selectPage(selection, visible, false)).toEqual(nothing);
    });
});

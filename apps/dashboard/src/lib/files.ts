/*
 * Project Ambrose by Imjustchico
 * The files page's own logic, apart from the page so it is tested without a browser: a folder's path split into breadcrumbs from its root, a query written with each value encoded once for the server to decode once, the page's place in the address, the policy at one row from its folder's policy and the rule on the row, each control's state from the first reason that applies, the root's policy, then a missing permission, then the milestone that brings the operation, and a selection that is either the rows picked or everything the filter matches, counted from the server's total rather than the rows on screen.
 */

export type Crumb = { name: string; path: string };

export type OperationState = { allowed: boolean; code?: string; reason?: string; rule?: string | null };

export type PolicyLike = { summary: string; operations: Record<string, OperationState | undefined> };

export type RuleLike = { effect: string; pattern: string; why: string } | null | undefined;

export type Control = { id: string; label: string; operation: string; permission: string; milestone: string | null };

export type ControlState = { enabled: boolean; because: "policy" | "permission" | "milestone" | null; reason: string };

export type Selection = { kind: "none" } | { kind: "rows"; names: string[] } | { kind: "matching"; filter: string };

export const MostCopied = 1000;

export const changing = new Set([
    "write",
    "upload",
    "create",
    "rename",
    "move",
    "copy",
    "delete",
    "permissions",
    "extract",
    "truncate",
    "pull",
]);

export const folderControls: Control[] = [
    { id: "upload", label: "Upload", operation: "upload", permission: "files.upload", milestone: "17.54" },
    { id: "new-folder", label: "New folder", operation: "create", permission: "files.write", milestone: "17.55" },
    { id: "new-file", label: "New file", operation: "create", permission: "files.write", milestone: "17.55" },
];

export const rowControls: Control[] = [
    { id: "download", label: "Download", operation: "download", permission: "files.download", milestone: "17.54" },
    { id: "edit", label: "Edit", operation: "write", permission: "files.write", milestone: "17.53" },
    { id: "rename", label: "Rename", operation: "rename", permission: "files.write", milestone: "17.54" },
    { id: "move", label: "Move", operation: "move", permission: "files.write", milestone: "17.54" },
    { id: "copy", label: "Copy", operation: "copy", permission: "files.write", milestone: "17.54" },
    { id: "archive", label: "Archive", operation: "archive", permission: "files.archive", milestone: "17.39" },
    { id: "delete", label: "Delete", operation: "delete", permission: "files.delete", milestone: "17.55" },
];

export const massControls: Control[] = rowControls.filter((control) => control.id !== "edit" && control.id !== "rename");

export function breadcrumbs(root: string, path: string): Crumb[] {
    const crumbs: Crumb[] = [{ name: root, path: "" }];
    let walked = "";
    for (const part of path.split("/").filter((piece) => piece !== "")) {
        walked = walked === "" ? part : `${walked}/${part}`;
        crumbs.push({ name: part, path: walked });
    }
    return crumbs;
}

export function childPath(folder: string, name: string): string {
    return folder === "" ? name : `${folder}/${name}`;
}

export function parentPath(path: string): string {
    const slash = path.lastIndexOf("/");
    return slash < 0 ? "" : path.slice(0, slash);
}

export function filesQuery(values: Record<string, string | number | boolean | undefined>): string {
    return Object.entries(values)
        .filter(([, value]) => value !== undefined && value !== "" && value !== false)
        .map(([key, value]) => `${key}=${encodeURIComponent(value === true ? "1" : String(value))}`)
        .join("&");
}

export function pageHash(root: string, path: string): string {
    const query = filesQuery({ root, path });
    return query === "" ? "#files" : `#files?${query}`;
}

export function readPageHash(hash: string): { root: string; path: string } {
    const question = hash.indexOf("?");
    if (question < 0) return { root: "", path: "" };
    const values = new URLSearchParams(hash.slice(question + 1));
    return { root: values.get("root") ?? "", path: values.get("path") ?? "" };
}

export function rowPolicy(policy: PolicyLike, rule: RuleLike): PolicyLike {
    if (!rule) return policy;
    const operations: Record<string, OperationState | undefined> = {};
    for (const [operation, state] of Object.entries(policy.operations)) {
        const refused =
            rule.effect === "hide" ||
            rule.effect === "elsewhere" ||
            (rule.effect === "client_derived" && operation !== "list") ||
            (rule.effect === "read_only" && changing.has(operation));
        operations[operation] = refused ? { allowed: false, code: rule.effect, reason: rule.why, rule: rule.pattern } : state;
    }
    return { summary: policy.summary, operations };
}

export function controlState(control: Control, policy: PolicyLike | null, may: (permission: string) => boolean): ControlState {
    const verdict = policy?.operations[control.operation];
    if (policy && verdict && !verdict.allowed)
        return { enabled: false, because: "policy", reason: verdict.reason && verdict.reason !== "" ? verdict.reason : policy.summary };
    if (!may(control.permission))
        return {
            enabled: false,
            because: "permission",
            reason: `Needs the ${control.permission} permission, which this sign-in does not grant`,
        };
    if (control.milestone !== null)
        return { enabled: false, because: "milestone", reason: `${control.label} arrives with milestone ${control.milestone}` };
    return { enabled: true, because: null, reason: "" };
}

export function viewState(policy: PolicyLike | null, may: (permission: string) => boolean): ControlState {
    return controlState({ id: "view", label: "View", operation: "read", permission: "files.read", milestone: null }, policy, may);
}

export const nothing: Selection = { kind: "none" };

export function isSelected(selection: Selection, name: string): boolean {
    if (selection.kind === "matching") return true;
    return selection.kind === "rows" && selection.names.includes(name);
}

export function toggleRow(selection: Selection, name: string, visible: string[]): Selection {
    const current = selection.kind === "rows" ? selection.names : selection.kind === "matching" ? [...visible] : [];
    const names = current.includes(name) ? current.filter((one) => one !== name) : [...current, name];
    return names.length === 0 ? nothing : { kind: "rows", names };
}

export function selectPage(selection: Selection, visible: string[], on: boolean): Selection {
    const current = selection.kind === "rows" ? selection.names : [];
    const names = on
        ? [...current, ...visible.filter((name) => !current.includes(name))]
        : current.filter((name) => !visible.includes(name));
    return names.length === 0 ? nothing : { kind: "rows", names };
}

export function selectMatching(filter: string): Selection {
    return { kind: "matching", filter };
}

export function pageState(selection: Selection, visible: string[]): "all" | "some" | "none" {
    if (visible.length === 0) return "none";
    const picked = visible.filter((name) => isSelected(selection, name)).length;
    if (picked === 0) return "none";
    return picked === visible.length ? "all" : "some";
}

export function selectionCount(selection: Selection, total: number): number {
    if (selection.kind === "matching") return total;
    return selection.kind === "rows" ? selection.names.length : 0;
}

export function describeSelection(selection: Selection, total: number): string {
    const count = selectionCount(selection, total);
    if (selection.kind === "matching")
        return selection.filter === "" ? `All ${count} in this folder selected` : `All ${count} matching "${selection.filter}" selected`;
    return count === 1 ? "1 selected" : `${count} selected`;
}

export function offersEveryMatch(selection: Selection, visible: string[], total: number): boolean {
    return selection.kind !== "matching" && pageState(selection, visible) === "all" && total > visible.length;
}

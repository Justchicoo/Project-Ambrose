/*
 * Project Ambrose by Imjustchico
 * The network page's calls to the panel's port allocation API: the allocations per node, adding one or a range with every port proven free, assigning one to an app through its listen setting, deleting with the assigned ones refused on every path, and the per-node port pool that supplies ports for new realms.
 */

import * as v from "valibot";
import { request } from "./api.svelte.js";

export const Allocation = v.looseObject({
    id: v.number(),
    node: v.string(),
    bind_address: v.string(),
    port: v.number(),
    protocol: v.string(),
    public_alias: v.string(),
    app: v.string(),
    role: v.string(),
    notes: v.string(),
    assigned: v.boolean(),
});

export type Allocation = v.InferOutput<typeof Allocation>;

export const Pool = v.looseObject({
    node: v.string(),
    start_port: v.number(),
    end_port: v.number(),
});

export type Pool = v.InferOutput<typeof Pool>;

const NodeAnswer = v.looseObject({
    schema: v.number(),
    node: v.string(),
    allocations: v.array(Allocation),
    pool: v.nullable(Pool),
    apps: v.array(
        v.looseObject({
            app: v.string(),
            allocations: v.array(Allocation),
        }),
    ),
});

export type NodeAnswer = v.InferOutput<typeof NodeAnswer>;

const AppAnswer = v.looseObject({
    schema: v.number(),
    app: v.string(),
    allocations: v.array(Allocation),
    live_listener: v.optional(
        v.looseObject({
            bind_address: v.string(),
            port: v.number(),
        }),
    ),
});

export type AppAnswer = v.InferOutput<typeof AppAnswer>;

const PoolAnswer = v.looseObject({
    schema: v.number(),
    pool: v.nullable(Pool),
});

const CreatedAnswer = v.looseObject({
    schema: v.number(),
    allocation: v.looseObject({
        id: v.number(),
        warnings: v.optional(v.array(v.string())),
    }),
});

const RangeAnswer = v.looseObject({
    schema: v.number(),
    added: v.number(),
    warnings: v.optional(v.array(v.string())),
});

const TakenAnswer = v.looseObject({
    schema: v.number(),
    taken: v.array(Allocation),
});

const AssignedAnswer = v.looseObject({
    schema: v.number(),
    assigned: v.number(),
    app: v.string(),
    role: v.string(),
    port: v.number(),
    bind_address: v.string(),
});

export type NewAllocation = {
    bind_address: string;
    port: number;
    protocol?: string;
    public_alias?: string;
    app?: string;
    role?: string;
    notes?: string;
    node?: string;
};

export function nodeOf(node = "local", signal?: AbortSignal): Promise<NodeAnswer> {
    return request("GET", `api/panel/network/node/${encodeURIComponent(node)}`, NodeAnswer, undefined, signal);
}

export function appOf(app: string, signal?: AbortSignal): Promise<AppAnswer> {
    return request("GET", `api/panel/network/app/${encodeURIComponent(app)}`, AppAnswer, undefined, signal);
}

export function addAllocation(draft: NewAllocation) {
    return request("POST", "api/panel/network/allocations", CreatedAnswer, draft);
}

export function addRange(draft: NewAllocation & { end_port: number }) {
    return request("POST", "api/panel/network/allocations/range", RangeAnswer, draft);
}

export function assignAllocation(id: number, app: string, role: string, reason?: string) {
    return request("POST", `api/panel/network/allocations/${id}/assign`, AssignedAnswer, { app, role, reason });
}

export function unassignAllocation(id: number) {
    return request("POST", `api/panel/network/allocations/${id}/unassign`, null);
}

export function deleteAllocation(id: number) {
    return request("DELETE", `api/panel/network/allocations/${id}`, null);
}

export function bulkDeleteAllocations(ids: number[]) {
    return request("POST", "api/panel/network/allocations/bulk-delete", null, { ids });
}

export function setPool(node: string, start_port: number, end_port: number) {
    return request("PUT", "api/panel/network/pool", PoolAnswer, { node, start_port, end_port });
}

export function takeFromPool(node = "local", count = 1) {
    return request("POST", "api/panel/network/pool/take", TakenAnswer, { node, count });
}

export const roles = ["login listener", "game listener", "patch HTTP", "admin API", "panel", "metrics"];

export const apps = ["loginserver", "gameserver", "patchserver", "supervisor"];

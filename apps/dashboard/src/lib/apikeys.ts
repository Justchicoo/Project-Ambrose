/*
 * Project Ambrose by Imjustchico
 * The panel's API keys as the dashboard sees them: a key the server mints reads amb_ plus its public id plus its secret, and the secret rides home only in the answer that made the key, never again; the pages below list, make, revoke and rotate the operator's own keys, and the review page lists and revokes every operator's keys for holders of apikeys.manage, each answer checked against its shape.
 */

import * as v from "valibot";
import { request } from "./api.svelte.js";

export const ApiKey = v.looseObject({
    id: v.number(),
    public_id: v.string(),
    owner_user_id: v.number(),
    owner_username: v.optional(v.string()),
    name: v.string(),
    permissions: v.array(v.string()),
    scopes: v.array(v.string()),
    cidrs: v.array(v.string()),
    created_epoch_ms: v.number(),
    expires_epoch_ms: v.nullable(v.number()),
    revoked_epoch_ms: v.nullable(v.number()),
    grace_expires_epoch_ms: v.nullable(v.number()),
    last_used_epoch_ms: v.nullable(v.number()),
    last_used_address: v.nullable(v.string()),
    live: v.boolean(),
});
export type ApiKey = v.InferOutput<typeof ApiKey>;

const ApiKeyList = v.array(ApiKey);
const ApiKeyCreated = v.intersect([ApiKey, v.looseObject({ key: v.string() })]);
export type ApiKeyCreated = v.InferOutput<typeof ApiKeyCreated>;

export type ApiKeyDraft = {
    name: string;
    permissions: string[];
    scopes: string[];
    cidrs: string[];
    expires_epoch_ms?: number | null;
    no_expiry?: boolean;
};

export function listApiKeys(signal?: AbortSignal) {
    return request("GET", "api/panel/apikeys", ApiKeyList, undefined, signal);
}

export function createApiKey(draft: ApiKeyDraft) {
    return request("POST", "api/panel/apikeys", ApiKeyCreated, draft);
}

export function revokeApiKey(publicId: string) {
    return request("DELETE", `api/panel/apikeys/${publicId}`, v.looseObject({ revoked: v.boolean() }));
}

export function rotateApiKey(publicId: string, graceSeconds: number) {
    return request("POST", `api/panel/apikeys/${publicId}/rotate`, ApiKeyCreated, { grace_seconds: graceSeconds });
}

export function listAllApiKeys(signal?: AbortSignal) {
    return request("GET", "api/panel/admin/apikeys", ApiKeyList, undefined, signal);
}

export function revokeAnyApiKey(publicId: string) {
    return request("DELETE", `api/panel/admin/apikeys/${publicId}`, v.looseObject({ revoked: v.boolean() }));
}

export function isKeyText(text: string): boolean {
    return /^amb_[A-Z2-7]{12}\.[A-Z2-7]{52}$/.test(text);
}

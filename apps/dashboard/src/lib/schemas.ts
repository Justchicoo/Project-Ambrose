/*
 * Project Ambrose by Imjustchico
 * The shapes the panel accepts from the admin API, checked at the boundary with Valibot: the session, the app list with what the supervisor knows about each app, the status, the capabilities, the captured output, a power answer, the settings an app has loaded and its databases with their update files, each loose so a field a newer server adds is kept rather than refused, since these schemas only ever gain fields.
 */

import * as v from "valibot";

export const SessionAnswer = v.looseObject({
    app: v.string(),
    signed_in: v.boolean(),
    signed_in_with: v.nullable(v.picklist(["session", "token"])),
    csrf: v.nullable(v.string()),
    idle_seconds: v.number(),
    lifetime_seconds: v.number(),
});

export const PanelUser = v.looseObject({
    id: v.number(),
    username: v.string(),
    display_name: v.string(),
    owner: v.boolean(),
    role: v.optional(v.string(), "viewer"),
    permissions: v.optional(v.array(v.string()), []),
    grants: v.optional(v.record(v.string(), v.array(v.string())), {}),
    must_change_password: v.boolean(),
});

export const PanelSessionAnswer = v.looseObject({
    app: v.string(),
    signed_in: v.boolean(),
    signed_in_with: v.nullable(v.picklist(["session", "token"])),
    csrf: v.nullable(v.string()),
    idle_seconds: v.number(),
    lifetime_seconds: v.number(),
    needs_owner: v.optional(v.boolean()),
    user: v.optional(v.nullable(PanelUser)),
});

export const PanelSignedIn = v.looseObject({
    csrf: v.string(),
    user: PanelUser,
});

export const AppExit = v.looseObject({
    epoch_ms: v.number(),
    code: v.nullable(v.number()),
    signal: v.nullable(v.number()),
    requested: v.boolean(),
    during: v.string(),
    uptime_ms: v.number(),
});

export const Supervision = v.looseObject({
    name: v.string(),
    program: v.string(),
    config: v.string(),
    state: v.string(),
    watching: v.boolean(),
    desired: v.string(),
    pid: v.nullable(v.number()),
    adopted: v.boolean(),
    started_epoch_ms: v.nullable(v.number()),
    ready_epoch_ms: v.nullable(v.number()),
    admin: v.looseObject({
        enabled: v.boolean(),
        address: v.nullable(v.string()),
        port: v.nullable(v.number()),
        problem: v.nullable(v.string()),
    }),
    stop: v.nullable(v.looseObject({ method: v.string(), requested_epoch_ms: v.number() })),
    restart_epoch_ms: v.nullable(v.number()),
    crashes: v.number(),
    failed_starts: v.number(),
    restarts: v.number(),
    last_exit: v.nullable(AppExit),
    exits: v.array(AppExit),
    message: v.nullable(v.string()),
});

export const AppEntry = v.looseObject({
    name: v.string(),
    role: v.string(),
    realm: v.string(),
    address: v.string(),
    port: v.number(),
    revision: v.string(),
    supervision: v.optional(v.nullable(Supervision)),
});

export const AppList = v.array(AppEntry);

export const LogRecord = v.looseObject({
    sequence: v.number(),
    time: v.string(),
    epoch_ms: v.number(),
    level: v.string(),
    category: v.string(),
    message: v.string(),
    template: v.optional(v.string()),
    source: v.optional(v.nullable(v.looseObject({ file: v.string(), line: v.number(), function: v.string() }))),
});

export const LogAnswer = v.looseObject({
    schema: v.number(),
    oldest: v.number(),
    latest: v.number(),
    records: v.array(LogRecord),
    dropped: v.nullable(v.looseObject({ from: v.number(), to: v.number(), count: v.number() })),
});

export const CommandAnswer = v.looseObject({
    command: v.string(),
    success: v.boolean(),
    refused: v.boolean(),
    reason: v.string(),
    request_id: v.string(),
    lines: v.array(v.string()),
});

export const Problem = v.looseObject({
    code: v.string(),
    message: v.string(),
    subject: v.string(),
});

export const Status = v.looseObject({
    schema: v.number(),
    app: v.string(),
    role: v.string(),
    realm: v.string(),
    revision: v.string(),
    state: v.string(),
    uptime: v.number(),
    memory: v.nullable(v.looseObject({ resident_bytes: v.number() })),
    threads: v.nullable(v.number()),
    sessions: v.nullable(v.number()),
    tick: v.nullable(v.looseObject({ average_ms: v.number(), max_ms: v.number(), samples: v.number(), window_seconds: v.number() })),
    stats: v.record(v.string(), v.union([v.number(), v.string(), v.boolean()])),
    problems: v.array(Problem),
});

export const Capabilities = v.looseObject({
    schema: v.number(),
    reload_targets: v.array(v.string()),
    schedule_actions: v.array(v.string()),
    announcement_channels: v.array(v.string()),
    problem_codes: v.array(v.looseObject({ code: v.string(), description: v.string() })),
});

export const OutputAnswer = v.looseObject({
    schema: v.number(),
    app: v.string(),
    run: v.string(),
    lines: v.array(v.looseObject({ seq: v.number(), stream: v.string(), text: v.string(), epoch_ms: v.nullable(v.number()) })),
});

export const PowerAnswer = v.looseObject({
    app: v.string(),
    action: v.string(),
    seconds: v.number(),
    accepted: v.boolean(),
});

export const SettingsAnswer = v.looseObject({
    schema: v.number(),
    file: v.string(),
    settings: v.array(
        v.looseObject({
            key: v.string(),
            value: v.string(),
            layer: v.string(),
            file: v.string(),
            line: v.number(),
            default: v.nullable(v.string()),
            default_file: v.nullable(v.string()),
            secret: v.boolean(),
            restart_reason: v.nullable(v.string()),
        }),
    ),
});

export const DatabaseAnswer = v.looseObject({
    schema: v.number(),
    databases: v.array(
        v.looseObject({
            name: v.string(),
            key: v.string(),
            state: v.string(),
            applying: v.boolean(),
            updates_enabled: v.boolean(),
            update_flag: v.number(),
            address: v.nullable(v.string()),
            pool: v.looseObject({
                async_connections: v.number(),
                sync_connections: v.number(),
                async_active: v.number(),
                sync_leased: v.number(),
                sync_waiting: v.number(),
                queued: v.number(),
                reconnects: v.number(),
            }),
            stores: v.array(v.string()),
        }),
    ),
});

export const AppliedUpdate = v.looseObject({
    name: v.string(),
    state: v.string(),
    hash: v.string(),
    applied_epoch_ms: v.number(),
    took_ms: v.number(),
    present: v.boolean(),
    changed: v.boolean(),
});

export const PendingUpdate = v.looseObject({
    name: v.string(),
    state: v.string(),
    file: v.string(),
    hash: v.string(),
    kind: v.string(),
    transactional: v.boolean(),
    line: v.nullable(v.number()),
    statement: v.nullable(v.string()),
    problem: v.nullable(v.string()),
    renamed_from: v.nullable(v.string()),
    restart_required: v.boolean(),
    waits_for: v.nullable(v.string()),
});

export const DatabaseUpdatesAnswer = v.looseObject({
    schema: v.number(),
    databases: v.array(
        v.looseObject({
            name: v.string(),
            listed: v.boolean(),
            error: v.nullable(v.string()),
            applied: v.array(AppliedUpdate),
            pending: v.array(PendingUpdate),
        }),
    ),
});

export const DatabaseApplyAnswer = v.looseObject({
    database: v.string(),
    succeeded: v.boolean(),
    applied: v.array(v.string()),
    stopped_at: v.nullable(v.string()),
    failed_at: v.nullable(v.string()),
    failure: v.nullable(v.string()),
    stores: v.array(v.looseObject({ name: v.string(), loaded: v.boolean(), errors: v.array(v.string()), warnings: v.array(v.string()) })),
});

export type SessionAnswer = v.InferOutput<typeof SessionAnswer>;
export type PanelUser = v.InferOutput<typeof PanelUser>;
export type PanelSessionAnswer = v.InferOutput<typeof PanelSessionAnswer>;
export type AppEntry = v.InferOutput<typeof AppEntry>;
export type LogRecord = v.InferOutput<typeof LogRecord>;
export type LogAnswer = v.InferOutput<typeof LogAnswer>;
export type Problem = v.InferOutput<typeof Problem>;
export type Status = v.InferOutput<typeof Status>;
export type Capabilities = v.InferOutput<typeof Capabilities>;
export type Supervision = v.InferOutput<typeof Supervision>;
export type AppExit = v.InferOutput<typeof AppExit>;
export type OutputAnswer = v.InferOutput<typeof OutputAnswer>;
export type SettingsAnswer = v.InferOutput<typeof SettingsAnswer>;
export type DatabaseAnswer = v.InferOutput<typeof DatabaseAnswer>;
export type DatabaseUpdatesAnswer = v.InferOutput<typeof DatabaseUpdatesAnswer>;
export type DatabaseApplyAnswer = v.InferOutput<typeof DatabaseApplyAnswer>;
export type PendingUpdate = v.InferOutput<typeof PendingUpdate>;

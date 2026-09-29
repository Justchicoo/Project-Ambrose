/*
 * Project Ambrose by Imjustchico
 * The shapes the panel accepts from the admin API, checked at the boundary with Valibot: the session, a sign-in that asks for a second factor, the operator's two-factor state, its setup secret, the recovery codes shown once, a step-up check and the refusal that asks for one, the app list with what the supervisor knows about each app, the status, the capabilities, the captured output, a power answer, the settings an app has loaded with their changes, batches, history and events, its databases with their update files, and the supervisor's file roots with their policies, a folder's listing, a window of a file and a root's protected patterns, each loose so a field a newer server adds is kept rather than refused, since these schemas only ever gain fields.
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
    two_factor: v.optional(v.boolean(), false),
    two_factor_required: v.optional(v.boolean(), false),
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

export const SecondFactorAsked = v.looseObject({
    second_factor: v.literal(true),
    methods: v.array(v.string()),
    expires_seconds: v.number(),
});

export const PanelSignInAnswer = v.union([SecondFactorAsked, PanelSignedIn]);

export const TwoFactorState = v.looseObject({
    enabled: v.boolean(),
    pending: v.boolean(),
    required: v.boolean(),
    recovery_codes_left: v.number(),
    enabled_epoch_ms: v.nullable(v.number()),
    window_steps: v.optional(v.number(), 1),
    issuer: v.optional(v.string(), ""),
});

export const TwoFactorSetup = v.looseObject({
    secret: v.string(),
    uri: v.string(),
    issuer: v.string(),
    account: v.string(),
    algorithm: v.string(),
    digits: v.number(),
    period: v.number(),
    fresh: v.boolean(),
});

export const RecoveryCodesIssued = v.looseObject({
    recovery_codes: v.array(v.string()),
    recovery_codes_left: v.number(),
    user: v.optional(v.nullable(PanelUser)),
});

export const TwoFactorTurnedOff = v.looseObject({
    user: v.nullable(PanelUser),
});

export const StepUpAnswer = v.looseObject({
    checked_epoch_ms: v.number(),
    window_seconds: v.number(),
});

export const StepUpAsked = v.looseObject({
    error: v.literal("step_up_required"),
    permission: v.string(),
    methods: v.array(v.string()),
    window_seconds: v.number(),
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
    start: v.optional(v.nullable(v.looseObject({ stage: v.string(), until_ms: v.number() })), null),
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
    needs_confirm: v.optional(v.boolean(), false),
    reason: v.string(),
    request_id: v.string(),
    lines: v.array(v.string()),
});

export const CommandHistoryAnswer = v.looseObject({
    schema: v.number(),
    app: v.string(),
    commands: v.array(v.string()),
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

export const SettingLock = v.looseObject({
    layer: v.string(),
    origin: v.string(),
});

export const SettingsAnswer = v.looseObject({
    schema: v.number(),
    file: v.string(),
    revealed: v.optional(v.boolean(), false),
    revealed_keys: v.optional(v.array(v.string()), []),
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
            declared: v.optional(v.boolean(), false),
            origin: v.optional(v.string()),
            type: v.optional(v.picklist(["bool", "integer", "unsigned", "float", "string"])),
            declared_default: v.optional(v.string()),
            min: v.optional(v.nullable(v.string())),
            max: v.optional(v.nullable(v.string())),
            bounds: v.optional(v.string()),
            unit: v.optional(v.string()),
            category: v.optional(v.string()),
            description: v.optional(v.string()),
            apply: v.optional(v.picklist(["live", "next_use", "restart"])),
            lock: v.optional(v.nullable(SettingLock)),
            visibility: v.optional(v.picklist(["normal", "secret"])),
            edit: v.optional(v.picklist(["normal", "restricted"])),
            persisted: v.optional(v.nullable(v.string())),
            revealed: v.optional(v.boolean(), false),
        }),
    ),
});

export const SettingChangeAnswer = v.looseObject({
    schema: v.number(),
    key: v.string(),
    changed: v.boolean(),
    value: v.string(),
    layer: v.string(),
    apply: v.string(),
    restart_reason: v.nullable(v.string()),
    message: v.string(),
});

export const SettingBatchAnswer = v.looseObject({
    schema: v.number(),
    dry_run: v.optional(v.boolean(), false),
    changed: v.array(v.looseObject({ key: v.string(), old: v.string(), new: v.string() })),
    unchanged: v.array(v.string()),
    message: v.string(),
});

export const SettingHistoryAnswer = v.looseObject({
    schema: v.number(),
    key: v.string(),
    visibility: v.picklist(["normal", "secret"]),
    entries: v.array(
        v.looseObject({
            id: v.number(),
            old: v.string(),
            new: v.string(),
            who: v.string(),
            account_id: v.number(),
            source: v.string(),
            reason: v.string(),
            epoch_seconds: v.number(),
        }),
    ),
});

export const AdminEvent = v.looseObject({
    type: v.literal("event"),
    sequence: v.number(),
    epoch_ms: v.number(),
    kind: v.string(),
    subject: v.string(),
    data: v.looseObject({}),
});

export const EventsAnswer = v.looseObject({
    schema: v.number(),
    oldest: v.number(),
    latest: v.number(),
    dropped: v.nullable(v.looseObject({ from: v.number(), to: v.number(), count: v.number() })),
    records: v.array(AdminEvent),
});

export const SettingProblem = v.looseObject({
    key: v.string(),
    code: v.string(),
    message: v.string(),
    layer: v.optional(v.string()),
    origin: v.optional(v.string()),
});

export const PanelSettingsAnswer = v.looseObject({
    schema: v.number(),
    settings: v.array(
        v.looseObject({
            key: v.string(),
            group: v.picklist(["general", "mail", "security"]),
            value: v.string(),
            default: v.string(),
            secret: v.boolean(),
            locked: v.boolean(),
            layer: v.string(),
            minimum: v.number(),
            maximum: v.number(),
        }),
    ),
});

export const ErrorGroup = v.looseObject({
    id: v.number(),
    app: v.string(),
    category: v.string(),
    level: v.string(),
    file: v.string(),
    line: v.number(),
    function: v.string(),
    template: v.string(),
    revision: v.string(),
    count: v.number(),
    total_count: v.number(),
    first_epoch_ms: v.number(),
    last_epoch_ms: v.number(),
    last_message: v.string(),
    context_before: v.array(LogRecord),
    new_since_cleared: v.boolean(),
});

export const ErrorsAnswer = v.looseObject({
    schema: v.number(),
    groups: v.array(ErrorGroup),
});

export const ErrorClearAnswer = v.looseObject({
    schema: v.number(),
    cleared: v.boolean(),
});

export const ErrorReport = v.looseObject({
    format: v.string(),
    schema: v.number(),
    product: v.looseObject({
        name: v.string(),
        version: v.string(),
        commit: v.string(),
        branch: v.string(),
    }),
    operating_system: v.string(),
    apps: v.record(v.string(), v.string()),
    groups: v.array(
        v.looseObject({
            app: v.string(),
            revision: v.string(),
            level: v.string(),
            category: v.string(),
            source: v.looseObject({ file: v.string(), line: v.number(), function: v.string() }),
            template: v.string(),
            count: v.number(),
            total_count: v.number(),
            first_epoch_ms: v.number(),
            last_epoch_ms: v.number(),
            rendered_message: v.optional(v.string()),
            log_lines_before: v.optional(v.array(LogRecord)),
        }),
    ),
});

export const ErrorReportAnswer = v.looseObject({
    schema: v.number(),
    report: ErrorReport,
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
export type SecondFactorAsked = v.InferOutput<typeof SecondFactorAsked>;
export type PanelSignInAnswer = v.InferOutput<typeof PanelSignInAnswer>;
export type TwoFactorState = v.InferOutput<typeof TwoFactorState>;
export type TwoFactorSetup = v.InferOutput<typeof TwoFactorSetup>;
export type RecoveryCodesIssued = v.InferOutput<typeof RecoveryCodesIssued>;
export type StepUpAnswer = v.InferOutput<typeof StepUpAnswer>;
export type StepUpAsked = v.InferOutput<typeof StepUpAsked>;
export type AppEntry = v.InferOutput<typeof AppEntry>;
export type LogRecord = v.InferOutput<typeof LogRecord>;
export type LogAnswer = v.InferOutput<typeof LogAnswer>;
export type Problem = v.InferOutput<typeof Problem>;
export type Status = v.InferOutput<typeof Status>;
export type Capabilities = v.InferOutput<typeof Capabilities>;
export type Supervision = v.InferOutput<typeof Supervision>;
export type AppExit = v.InferOutput<typeof AppExit>;
export const ReloadTarget = v.looseObject({
    target: v.string(),
    generation: v.number(),
    ran: v.boolean(),
    ok: v.boolean(),
    errors: v.array(v.string()),
    finished_ms: v.optional(v.nullable(v.number())),
});

export const ReloadAnswer = v.looseObject({
    schema: v.number(),
    targets: v.array(ReloadTarget),
});

export const ReloadRunAnswer = v.looseObject({
    schema: v.number(),
    ok: v.boolean(),
    targets: v.array(ReloadTarget),
});

export const OnlinePlayer = v.looseObject({
    character_guid: v.number(),
    account_id: v.number(),
    realm_id: v.number(),
    account: v.string(),
    realm: v.string(),
    name: v.string(),
    zone: v.string(),
    level: v.number(),
    school_id: v.number(),
    found: v.boolean(),
    since_epoch_seconds: v.number(),
    seconds: v.number(),
});

export const PlayersAnswer = v.looseObject({
    schema: v.number(),
    counted: v.number(),
    players: v.array(OnlinePlayer),
});

export const RealmRow = v.looseObject({
    id: v.number(),
    name: v.string(),
    address: v.string(),
    local_address: v.string(),
    port: v.number(),
    flags: v.number(),
    population: v.number(),
    player_limit: v.number(),
    full: v.boolean(),
    marked_offline: v.boolean(),
    online: v.boolean(),
    last_heartbeat_epoch_seconds: v.number(),
    heartbeat_age_seconds: v.number(),
});

export const RealmsAnswer = v.looseObject({
    schema: v.number(),
    counted: v.number(),
    online: v.number(),
    players: v.number(),
    heartbeat_seconds: v.number(),
    offline_after_intervals: v.number(),
    realms: v.array(RealmRow),
});

export const GraphSubject = v.looseObject({
    subject: v.string(),
    series: v.optional(v.array(v.string()), []),
});

export const GraphsAnswer = v.looseObject({
    schema: v.number(),
    now_epoch_ms: v.optional(v.number(), 0),
    subjects: v.optional(v.array(GraphSubject), []),
});

export const GraphRangeAnswer = v.looseObject({
    schema: v.number(),
    subject: v.string(),
    series: v.string(),
    from_epoch_ms: v.optional(v.number(), 0),
    to_epoch_ms: v.optional(v.number(), 0),
    at_epoch_ms: v.optional(v.array(v.number()), []),
    values: v.optional(v.array(v.nullable(v.number())), []),
    lowest: v.optional(v.array(v.nullable(v.number())), []),
    highest: v.optional(v.array(v.nullable(v.number())), []),
    points: v.optional(v.number(), 0),
    present: v.optional(v.number(), 0),
});

export const MetricBucket = v.looseObject({
    le: v.number(),
    count: v.number(),
});

export const MetricSeries = v.looseObject({
    labels: v.optional(v.record(v.string(), v.string()), {}),
    value: v.optional(v.number(), 0),
    count: v.optional(v.number(), 0),
    sum: v.optional(v.number(), 0),
    buckets: v.optional(v.array(MetricBucket), []),
});

export const MetricFamily = v.looseObject({
    name: v.string(),
    help: v.optional(v.string(), ""),
    kind: v.optional(v.string(), "untyped"),
    series: v.optional(v.array(MetricSeries), []),
});

export const MetricsAnswer = v.looseObject({
    schema: v.number(),
    metrics: v.optional(v.array(MetricFamily), []),
});

export const TickProfileAnswer = v.looseObject({
    schema: v.number(),
    active: v.boolean(),
    complete: v.boolean(),
    truncated: v.boolean(),
    requested_seconds: v.number(),
    events: v.number(),
});

export const TickProfileTraceAnswer = v.looseObject({
    schema: v.number(),
    requested_seconds: v.number(),
    truncated: v.boolean(),
    events: v.number(),
    trace: v.string(),
});

export const ActivityRow = v.looseObject({
    time: v.optional(v.string(), ""),
    epoch_ms: v.optional(v.number(), 0),
    app: v.optional(v.string(), ""),
    who: v.optional(v.string(), ""),
    address: v.optional(v.string(), ""),
    command: v.optional(v.string(), ""),
    level: v.optional(v.number(), 0),
    confirmed: v.optional(v.boolean(), false),
    ran: v.optional(v.boolean(), false),
    refused: v.optional(v.boolean(), false),
    reason: v.optional(v.string(), ""),
});

export const ActivityAnswer = v.looseObject({
    schema: v.number(),
    kept: v.boolean(),
    written: v.number(),
    unreadable: v.number(),
    activity: v.array(ActivityRow),
});

export const ClientAnswer = v.looseObject({
    schema: v.number(),
    pinned_revision: v.string(),
    install: v.looseObject({
        found: v.boolean(),
        root: v.optional(v.string(), ""),
        revision: v.optional(v.string(), ""),
        pinned: v.optional(v.boolean(), false),
        has_program: v.optional(v.boolean(), false),
        described: v.optional(v.string(), ""),
    }),
    type_dump: v.looseObject({
        found: v.boolean(),
        readable: v.boolean(),
        built_now: v.optional(v.boolean(), false),
        error: v.optional(v.string(), ""),
        path: v.optional(v.string(), ""),
        revision: v.optional(v.string(), ""),
        executable_sha256: v.optional(v.string(), ""),
        extractor: v.optional(v.string(), ""),
        matches_install: v.optional(v.boolean(), false),
    }),
    messages: v.looseObject({ loaded: v.boolean(), counted: v.number() }),
    saved: v.array(v.looseObject({ key: v.string(), value: v.string() })),
    saved_to: v.string(),
});

export const FileOperationState = v.looseObject({
    allowed: v.boolean(),
    code: v.optional(v.string(), ""),
    reason: v.optional(v.string(), ""),
    rule: v.optional(v.nullable(v.string()), null),
});

export const FilePolicy = v.looseObject({
    summary: v.string(),
    client_derived: v.optional(v.boolean(), false),
    read_only: v.optional(v.boolean(), false),
    operations: v.record(v.string(), FileOperationState),
});

export const FileRule = v.looseObject({
    pattern: v.string(),
    effect: v.string(),
    origin: v.optional(v.string(), "built_in"),
    why: v.string(),
});

export const FileVolume = v.looseObject({
    name: v.string(),
    free: v.number(),
    total: v.number(),
    minimum: v.number(),
    reserved: v.number(),
});

export const FileRoot = v.looseObject({
    id: v.string(),
    label: v.string(),
    kind: v.string(),
    apps: v.array(v.string()),
    present: v.boolean(),
    problem: v.nullable(v.string()),
    client_derived: v.boolean(),
    read_only: v.boolean(),
    policy: FilePolicy,
    volume: v.nullable(FileVolume),
    rules: v.array(FileRule),
});

export const FileRootsAnswer = v.looseObject({
    schema: v.number(),
    generation: v.optional(v.number(), 0),
    roots: v.array(FileRoot),
    apps: v.optional(v.array(v.looseObject({ name: v.string(), program: v.string() })), []),
    notes: v.optional(v.array(v.string()), []),
});

export const FileEntry = v.looseObject({
    name: v.string(),
    kind: v.string(),
    size: v.number(),
    modified_ms: v.number(),
    openable: v.boolean(),
    problem: v.nullable(v.string()),
    rule: v.nullable(FileRule),
});

export const FileListing = v.looseObject({
    schema: v.number(),
    root: v.string(),
    path: v.string(),
    modified_ms: v.optional(v.number(), 0),
    policy: FilePolicy,
    rule: v.optional(v.nullable(FileRule), null),
    entries: v.array(FileEntry),
    total: v.number(),
    offset: v.number(),
    limit: v.number(),
    truncated: v.boolean(),
    sort: v.string(),
    order: v.string(),
    filter: v.string(),
});

export const FileContent = v.looseObject({
    schema: v.number(),
    root: v.string(),
    path: v.string(),
    name: v.string(),
    size: v.number(),
    modified_ms: v.number(),
    etag: v.nullable(v.string()),
    offset: v.number(),
    length: v.number(),
    next_offset: v.nullable(v.number()),
    eof: v.boolean(),
    binary: v.boolean(),
    bom: v.optional(v.boolean(), false),
    text: v.nullable(v.string()),
    redacted: v.boolean(),
    redacted_keys: v.array(v.string()),
    revealed: v.boolean(),
    revealed_keys: v.optional(v.array(v.string()), []),
});

export const FileRulesAnswer = v.looseObject({
    schema: v.number(),
    root: v.string(),
    built_in: v.array(v.looseObject({ pattern: v.string(), effect: v.string(), why: v.string() })),
    patterns: v.array(v.string()),
    rebuilt: v.optional(v.boolean(), true),
    errors: v.optional(v.array(v.string()), []),
});

export type OutputAnswer = v.InferOutput<typeof OutputAnswer>;
export type SettingsAnswer = v.InferOutput<typeof SettingsAnswer>;
export type SettingLock = v.InferOutput<typeof SettingLock>;
export type SettingChangeAnswer = v.InferOutput<typeof SettingChangeAnswer>;
export type SettingBatchAnswer = v.InferOutput<typeof SettingBatchAnswer>;
export type SettingHistoryAnswer = v.InferOutput<typeof SettingHistoryAnswer>;
export type SettingProblem = v.InferOutput<typeof SettingProblem>;
export type AdminEvent = v.InferOutput<typeof AdminEvent>;
export type EventsAnswer = v.InferOutput<typeof EventsAnswer>;
export type DatabaseAnswer = v.InferOutput<typeof DatabaseAnswer>;
export type DatabaseUpdatesAnswer = v.InferOutput<typeof DatabaseUpdatesAnswer>;
export type DatabaseApplyAnswer = v.InferOutput<typeof DatabaseApplyAnswer>;
export type PendingUpdate = v.InferOutput<typeof PendingUpdate>;
export type ReloadTarget = v.InferOutput<typeof ReloadTarget>;
export type ReloadAnswer = v.InferOutput<typeof ReloadAnswer>;
export type ReloadRunAnswer = v.InferOutput<typeof ReloadRunAnswer>;
export type OnlinePlayer = v.InferOutput<typeof OnlinePlayer>;
export type PlayersAnswer = v.InferOutput<typeof PlayersAnswer>;
export type RealmRow = v.InferOutput<typeof RealmRow>;
export type RealmsAnswer = v.InferOutput<typeof RealmsAnswer>;
export type GraphSubject = v.InferOutput<typeof GraphSubject>;
export type GraphsAnswer = v.InferOutput<typeof GraphsAnswer>;
export type GraphRangeAnswer = v.InferOutput<typeof GraphRangeAnswer>;
export type MetricBucket = v.InferOutput<typeof MetricBucket>;
export type MetricSeries = v.InferOutput<typeof MetricSeries>;
export type MetricFamily = v.InferOutput<typeof MetricFamily>;
export type MetricsAnswer = v.InferOutput<typeof MetricsAnswer>;
export type TickProfileAnswer = v.InferOutput<typeof TickProfileAnswer>;
export type TickProfileTraceAnswer = v.InferOutput<typeof TickProfileTraceAnswer>;
export type ActivityRow = v.InferOutput<typeof ActivityRow>;
export type ActivityAnswer = v.InferOutput<typeof ActivityAnswer>;
export type ClientAnswer = v.InferOutput<typeof ClientAnswer>;
export type FileOperationState = v.InferOutput<typeof FileOperationState>;
export type FilePolicy = v.InferOutput<typeof FilePolicy>;
export type FileRule = v.InferOutput<typeof FileRule>;
export type FileVolume = v.InferOutput<typeof FileVolume>;
export type FileRoot = v.InferOutput<typeof FileRoot>;
export type FileRootsAnswer = v.InferOutput<typeof FileRootsAnswer>;
export type FileEntry = v.InferOutput<typeof FileEntry>;
export type FileListing = v.InferOutput<typeof FileListing>;
export type FileContent = v.InferOutput<typeof FileContent>;
export type FileRulesAnswer = v.InferOutput<typeof FileRulesAnswer>;

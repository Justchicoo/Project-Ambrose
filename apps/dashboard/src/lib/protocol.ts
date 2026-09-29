/*
 * Project Ambrose by Imjustchico
 * The panel event socket's protocol, rendered by PanelEventCatalog::RenderTypeScript from the supervisor's one catalog and never edited by hand: the version, the envelope, the close codes, every stream with the permission that reads it and whether this build serves it, every message in each direction with the ones this build handles and sends and the stream each server message belongs to, and a Valibot schema with its type for each message's data. PanelEventCatalogTest fails when this file differs from what the catalog renders, and writes it again when AMBROSE_WRITE_PANEL_EVENT_TYPES is 1.
 */

import * as v from "valibot";

export const protocolVersion = 1;

export const envelopeFields = ["v", "type", "id", "scope", "seq", "time", "data"] as const;

export const closeCodes = {
    "malformed": 4400,
    "session_ended": 4401,
    "access_lost": 4403,
    "limits": 4429,
} as const;

export const streams = ["logs", "status", "stats", "players", "setup", "backups", "updates", "settings", "reloads", "schedules", "nodes", "alerts", "audit", "files"] as const;
export type Stream = (typeof streams)[number];

export const servedStreams = ["status"] as const;

export const streamPermissions = {
    "logs": "console.read",
    "status": "status.read",
    "stats": "metrics.read",
    "players": "players.read",
    "setup": "clientdata.read",
    "backups": "backups.read",
    "updates": "updates.read",
    "settings": "settings.read",
    "reloads": "reload.read",
    "schedules": "schedules.read",
    "nodes": "nodes.read",
    "alerts": "alerts.read",
    "audit": "activity.read",
    "files": "files.list",
} as const;

export const clientTypes = ["hello", "subscribe", "unsubscribe", "resume", "backlog", "command", "power", "stats.now", "ping"] as const;
export type ClientType = (typeof clientTypes)[number];

export const serverTypes = ["ready", "status", "stats", "log", "dropped", "command.result", "power.accepted", "power.progress", "power.result", "players", "setup.output", "setup.state", "backup.progress", "backup.completed", "backup.failed", "restore.progress", "restore.completed", "restore.failed", "update.progress", "update.result", "setting.changed", "reload.result", "schedule.run.started", "schedule.run.waiting", "schedule.countdown.tick", "schedule.task.started", "schedule.task.output", "schedule.task.finished", "schedule.run.finished", "schedule.changed", "realm.changed", "announce.sent", "file.job", "node.status", "node.move", "alert", "audit", "permissions.changed", "throttled", "error", "session.expiring", "pong"] as const;
export type ServerType = (typeof serverTypes)[number];

export const handledTypes = ["hello", "resume", "ping"] as const;
export type HandledType = (typeof handledTypes)[number];

export const sentTypes = ["ready", "status", "dropped", "error", "pong"] as const;
export type SentType = (typeof sentTypes)[number];

export const streamOfType = {
    "status": "status",
    "stats": "stats",
    "log": "logs",
    "power.accepted": "status",
    "power.progress": "status",
    "power.result": "status",
    "players": "players",
    "setup.output": "setup",
    "setup.state": "setup",
    "backup.progress": "backups",
    "backup.completed": "backups",
    "backup.failed": "backups",
    "restore.progress": "backups",
    "restore.completed": "backups",
    "restore.failed": "backups",
    "update.progress": "updates",
    "update.result": "updates",
    "setting.changed": "settings",
    "reload.result": "reloads",
    "schedule.run.started": "schedules",
    "schedule.run.waiting": "schedules",
    "schedule.countdown.tick": "schedules",
    "schedule.task.started": "schedules",
    "schedule.task.output": "schedules",
    "schedule.task.finished": "schedules",
    "schedule.run.finished": "schedules",
    "schedule.changed": "schedules",
    "announce.sent": "players",
    "file.job": "files",
    "node.status": "nodes",
    "node.move": "nodes",
    "alert": "alerts",
    "audit": "audit",
} as const;

export const Scope = v.nullable(v.looseObject({ app: v.optional(v.string()) }));
export type Scope = v.InferOutput<typeof Scope>;

export const ServerFrame = v.looseObject({
    v: v.number(),
    type: v.string(),
    id: v.nullable(v.string()),
    scope: Scope,
    seq: v.nullable(v.number()),
    time: v.number(),
    data: v.looseObject({}),
});
export type ServerFrame = v.InferOutput<typeof ServerFrame>;

export const ClientFrame = v.looseObject({
    v: v.literal(1),
    type: v.picklist(clientTypes),
    id: v.optional(v.string()),
    scope: v.optional(Scope),
    seq: v.optional(v.number()),
    data: v.looseObject({}),
});
export type ClientFrame = v.InferOutput<typeof ClientFrame>;

export const HelloData = v.looseObject({
    version: v.pipe(v.number(), v.integer()),
    csrf: v.optional(v.string()),
    ticket: v.optional(v.string()),
});
export type HelloData = v.InferOutput<typeof HelloData>;

export const SubscribeData = v.looseObject({});
export type SubscribeData = v.InferOutput<typeof SubscribeData>;

export const UnsubscribeData = v.looseObject({});
export type UnsubscribeData = v.InferOutput<typeof UnsubscribeData>;

export const ResumeData = v.looseObject({
    stream: v.string(),
});
export type ResumeData = v.InferOutput<typeof ResumeData>;

export const BacklogData = v.looseObject({});
export type BacklogData = v.InferOutput<typeof BacklogData>;

export const CommandData = v.looseObject({});
export type CommandData = v.InferOutput<typeof CommandData>;

export const PowerData = v.looseObject({});
export type PowerData = v.InferOutput<typeof PowerData>;

export const StatsNowData = v.looseObject({});
export type StatsNowData = v.InferOutput<typeof StatsNowData>;

export const PingData = v.looseObject({});
export type PingData = v.InferOutput<typeof PingData>;

export const ReadyData = v.looseObject({
    version: v.pipe(v.number(), v.integer()),
    server_time: v.pipe(v.number(), v.integer()),
    instance: v.string(),
    permissions: v.looseObject({ panel: v.array(v.string()), apps: v.record(v.string(), v.array(v.string())) }),
    apps: v.array(v.looseObject({ name: v.string() })),
    realms: v.array(v.looseObject({})),
});
export type ReadyData = v.InferOutput<typeof ReadyData>;

export const StatusData = v.looseObject({
    app: v.string(),
    state: v.picklist(["offline", "starting", "running", "stopping", "crashed", "backoff", "crash_loop", "disabled", "setup", "updating", "restoring", "moving"]),
    since: v.pipe(v.number(), v.integer()),
    pid: v.nullable(v.pipe(v.number(), v.integer())),
    exit_code: v.nullable(v.pipe(v.number(), v.integer())),
    crashes: v.pipe(v.number(), v.integer()),
    next_restart: v.nullable(v.pipe(v.number(), v.integer())),
});
export type StatusData = v.InferOutput<typeof StatusData>;

export const StatsData = v.looseObject({});
export type StatsData = v.InferOutput<typeof StatsData>;

export const LogData = v.looseObject({});
export type LogData = v.InferOutput<typeof LogData>;

export const DroppedData = v.looseObject({
    stream: v.string(),
    count: v.pipe(v.number(), v.integer()),
    first: v.pipe(v.number(), v.integer()),
    last: v.pipe(v.number(), v.integer()),
});
export type DroppedData = v.InferOutput<typeof DroppedData>;

export const CommandResultData = v.looseObject({});
export type CommandResultData = v.InferOutput<typeof CommandResultData>;

export const PowerAcceptedData = v.looseObject({});
export type PowerAcceptedData = v.InferOutput<typeof PowerAcceptedData>;

export const PowerProgressData = v.looseObject({});
export type PowerProgressData = v.InferOutput<typeof PowerProgressData>;

export const PowerResultData = v.looseObject({});
export type PowerResultData = v.InferOutput<typeof PowerResultData>;

export const PlayersData = v.looseObject({});
export type PlayersData = v.InferOutput<typeof PlayersData>;

export const SetupOutputData = v.looseObject({});
export type SetupOutputData = v.InferOutput<typeof SetupOutputData>;

export const SetupStateData = v.looseObject({});
export type SetupStateData = v.InferOutput<typeof SetupStateData>;

export const BackupProgressData = v.looseObject({});
export type BackupProgressData = v.InferOutput<typeof BackupProgressData>;

export const BackupCompletedData = v.looseObject({});
export type BackupCompletedData = v.InferOutput<typeof BackupCompletedData>;

export const BackupFailedData = v.looseObject({});
export type BackupFailedData = v.InferOutput<typeof BackupFailedData>;

export const RestoreProgressData = v.looseObject({});
export type RestoreProgressData = v.InferOutput<typeof RestoreProgressData>;

export const RestoreCompletedData = v.looseObject({});
export type RestoreCompletedData = v.InferOutput<typeof RestoreCompletedData>;

export const RestoreFailedData = v.looseObject({});
export type RestoreFailedData = v.InferOutput<typeof RestoreFailedData>;

export const UpdateProgressData = v.looseObject({});
export type UpdateProgressData = v.InferOutput<typeof UpdateProgressData>;

export const UpdateResultData = v.looseObject({});
export type UpdateResultData = v.InferOutput<typeof UpdateResultData>;

export const SettingChangedData = v.looseObject({});
export type SettingChangedData = v.InferOutput<typeof SettingChangedData>;

export const ReloadResultData = v.looseObject({});
export type ReloadResultData = v.InferOutput<typeof ReloadResultData>;

export const ScheduleRunStartedData = v.looseObject({});
export type ScheduleRunStartedData = v.InferOutput<typeof ScheduleRunStartedData>;

export const ScheduleRunWaitingData = v.looseObject({});
export type ScheduleRunWaitingData = v.InferOutput<typeof ScheduleRunWaitingData>;

export const ScheduleCountdownTickData = v.looseObject({});
export type ScheduleCountdownTickData = v.InferOutput<typeof ScheduleCountdownTickData>;

export const ScheduleTaskStartedData = v.looseObject({});
export type ScheduleTaskStartedData = v.InferOutput<typeof ScheduleTaskStartedData>;

export const ScheduleTaskOutputData = v.looseObject({});
export type ScheduleTaskOutputData = v.InferOutput<typeof ScheduleTaskOutputData>;

export const ScheduleTaskFinishedData = v.looseObject({});
export type ScheduleTaskFinishedData = v.InferOutput<typeof ScheduleTaskFinishedData>;

export const ScheduleRunFinishedData = v.looseObject({});
export type ScheduleRunFinishedData = v.InferOutput<typeof ScheduleRunFinishedData>;

export const ScheduleChangedData = v.looseObject({});
export type ScheduleChangedData = v.InferOutput<typeof ScheduleChangedData>;

export const RealmChangedData = v.looseObject({});
export type RealmChangedData = v.InferOutput<typeof RealmChangedData>;

export const AnnounceSentData = v.looseObject({});
export type AnnounceSentData = v.InferOutput<typeof AnnounceSentData>;

export const FileJobData = v.looseObject({});
export type FileJobData = v.InferOutput<typeof FileJobData>;

export const NodeStatusData = v.looseObject({});
export type NodeStatusData = v.InferOutput<typeof NodeStatusData>;

export const NodeMoveData = v.looseObject({});
export type NodeMoveData = v.InferOutput<typeof NodeMoveData>;

export const AlertData = v.looseObject({});
export type AlertData = v.InferOutput<typeof AlertData>;

export const AuditData = v.looseObject({});
export type AuditData = v.InferOutput<typeof AuditData>;

export const PermissionsChangedData = v.looseObject({});
export type PermissionsChangedData = v.InferOutput<typeof PermissionsChangedData>;

export const ThrottledData = v.looseObject({});
export type ThrottledData = v.InferOutput<typeof ThrottledData>;

export const ErrorData = v.looseObject({
    request: v.nullable(v.string()),
    code: v.string(),
    message: v.string(),
    correlation: v.string(),
});
export type ErrorData = v.InferOutput<typeof ErrorData>;

export const SessionExpiringData = v.looseObject({});
export type SessionExpiringData = v.InferOutput<typeof SessionExpiringData>;

export const PongData = v.looseObject({});
export type PongData = v.InferOutput<typeof PongData>;

export const clientData = {
    "hello": HelloData,
    "subscribe": SubscribeData,
    "unsubscribe": UnsubscribeData,
    "resume": ResumeData,
    "backlog": BacklogData,
    "command": CommandData,
    "power": PowerData,
    "stats.now": StatsNowData,
    "ping": PingData,
} as const;

export const serverData = {
    "ready": ReadyData,
    "status": StatusData,
    "stats": StatsData,
    "log": LogData,
    "dropped": DroppedData,
    "command.result": CommandResultData,
    "power.accepted": PowerAcceptedData,
    "power.progress": PowerProgressData,
    "power.result": PowerResultData,
    "players": PlayersData,
    "setup.output": SetupOutputData,
    "setup.state": SetupStateData,
    "backup.progress": BackupProgressData,
    "backup.completed": BackupCompletedData,
    "backup.failed": BackupFailedData,
    "restore.progress": RestoreProgressData,
    "restore.completed": RestoreCompletedData,
    "restore.failed": RestoreFailedData,
    "update.progress": UpdateProgressData,
    "update.result": UpdateResultData,
    "setting.changed": SettingChangedData,
    "reload.result": ReloadResultData,
    "schedule.run.started": ScheduleRunStartedData,
    "schedule.run.waiting": ScheduleRunWaitingData,
    "schedule.countdown.tick": ScheduleCountdownTickData,
    "schedule.task.started": ScheduleTaskStartedData,
    "schedule.task.output": ScheduleTaskOutputData,
    "schedule.task.finished": ScheduleTaskFinishedData,
    "schedule.run.finished": ScheduleRunFinishedData,
    "schedule.changed": ScheduleChangedData,
    "realm.changed": RealmChangedData,
    "announce.sent": AnnounceSentData,
    "file.job": FileJobData,
    "node.status": NodeStatusData,
    "node.move": NodeMoveData,
    "alert": AlertData,
    "audit": AuditData,
    "permissions.changed": PermissionsChangedData,
    "throttled": ThrottledData,
    "error": ErrorData,
    "session.expiring": SessionExpiringData,
    "pong": PongData,
} as const;

export type ClientData<T extends ClientType> = v.InferOutput<(typeof clientData)[T]>;
export type ServerData<T extends ServerType> = v.InferOutput<(typeof serverData)[T]>;

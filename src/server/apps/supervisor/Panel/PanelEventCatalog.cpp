/*
 * Project Ambrose by Imjustchico
 * The catalog itself, every stream and message doc/PANEL.md lists under Event socket and in its order, with fixed fields only for the messages this build writes or reads, so no field name is settled before the milestone that produces it; the rest carry an open object their milestone fills in, and since fields are only ever added the schemas the dashboard gets are loose. Data from a client is held to its shape with every unknown key refused, and the TypeScript module is rendered line by line so the committed file can be compared with it byte for byte.
 */

#include "PanelEventCatalog.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

namespace
{
    using Shape = std::shared_ptr<PanelShape const>;

    Shape KindShape(PanelShapeKind kind)
    {
        auto shape = std::make_shared<PanelShape>();
        shape->Kind = kind;
        return shape;
    }

    Shape TextShape(std::vector<std::string_view> choices = {})
    {
        auto shape = std::make_shared<PanelShape>();
        shape->Kind = PanelShapeKind::Text;
        shape->Choices = std::move(choices);
        return shape;
    }

    Shape WholeShape()
    {
        return KindShape(PanelShapeKind::Whole);
    }

    Shape OpenShape()
    {
        return KindShape(PanelShapeKind::Open);
    }

    Shape ListOf(Shape item)
    {
        auto shape = std::make_shared<PanelShape>();
        shape->Kind = PanelShapeKind::List;
        shape->Item = std::move(item);
        return shape;
    }

    Shape MapOf(Shape item)
    {
        auto shape = std::make_shared<PanelShape>();
        shape->Kind = PanelShapeKind::Map;
        shape->Item = std::move(item);
        return shape;
    }

    Shape ObjectOf(std::vector<PanelField> fields)
    {
        auto shape = std::make_shared<PanelShape>();
        shape->Kind = PanelShapeKind::Object;
        shape->Fields = std::move(fields);
        return shape;
    }

    Shape NullableOf(Shape const& inner)
    {
        auto shape = std::make_shared<PanelShape>(*inner);
        shape->Nullable = true;
        return shape;
    }

    PanelField Field(std::string_view name, Shape shape)
    {
        return PanelField{ name, std::move(shape), false };
    }

    PanelField Maybe(std::string_view name, Shape shape)
    {
        return PanelField{ name, std::move(shape), true };
    }

    PanelEventType Takes(std::string_view name, std::string_view stream, PanelEventGate gate, std::string_view permission, bool handled, Shape data)
    {
        return PanelEventType{ name, PanelEventDirection::FromClient, stream, gate, permission, handled, std::move(data) };
    }

    PanelEventType Sends(std::string_view name, std::string_view stream, PanelEventGate gate, std::string_view permission, bool sent, Shape data)
    {
        return PanelEventType{ name, PanelEventDirection::FromServer, stream, gate, permission, sent, std::move(data) };
    }

    PanelEventType Later(std::string_view name, std::string_view stream, PanelEventGate gate, std::string_view permission)
    {
        return Sends(name, stream, gate, permission, false, OpenShape());
    }

    std::vector<PanelEventStream> BuildStreams()
    {
        return {
            { "logs", "console.read", StreamOverflow::DropOldest, false },
            { "status", "status.read", StreamOverflow::KeepAll, true },
            { "stats", "metrics.read", StreamOverflow::NewestPerKey, false },
            { "players", "players.read", StreamOverflow::DropOldest, false },
            { "setup", "clientdata.read", StreamOverflow::DropOldest, false },
            { "backups", "backups.read", StreamOverflow::DropOldest, false },
            { "updates", "updates.read", StreamOverflow::DropOldest, false },
            { "settings", "settings.read", StreamOverflow::DropOldest, false },
            { "reloads", "reload.read", StreamOverflow::DropOldest, false },
            { "schedules", "schedules.read", StreamOverflow::DropOldest, false },
            { "nodes", "nodes.read", StreamOverflow::DropOldest, false },
            { "alerts", "alerts.read", StreamOverflow::DropOldest, false },
            { "audit", "activity.read", StreamOverflow::DropOldest, false },
            { "files", "files.list", StreamOverflow::DropOldest, false },
        };
    }

    std::vector<PanelEventType> BuildTypes()
    {
        std::vector<std::string_view> const actions{ "start", "stop", "restart", "kill" };
        Shape const targetShape = ObjectOf({ Field("kind", TextShape({ "app", "realm", "stack" })), Field("name", NullableOf(TextShape())) });
        std::vector<std::string_view> const states{ "offline", "starting", "running", "stopping", "crashed", "backoff", "crash_loop", "disabled", "setup", "updating", "restoring", "moving" };
        return {
            Takes("hello", "", PanelEventGate::SignedIn, "", true, ObjectOf({ Field("version", WholeShape()), Maybe("csrf", TextShape()), Maybe("ticket", TextShape()) })),
            Takes("subscribe", "", PanelEventGate::StreamPermission, "", false, OpenShape()),
            Takes("unsubscribe", "", PanelEventGate::None, "", false, OpenShape()),
            Takes("resume", "", PanelEventGate::StreamPermission, "", true, ObjectOf({ Field("stream", TextShape()) })),
            Takes("backlog", "logs", PanelEventGate::Permission, "console.read", false, OpenShape()),
            Takes("command", "", PanelEventGate::Permission, "console.write", false, OpenShape()),
            Takes("power", "", PanelEventGate::ActionPermission, "", false, OpenShape()),
            Takes("stats.now", "stats", PanelEventGate::Permission, "metrics.read", false, OpenShape()),
            Takes("ping", "", PanelEventGate::None, "", true, ObjectOf({})),

            Sends("ready", "", PanelEventGate::User, "", true, ObjectOf({
                Field("version", WholeShape()),
                Field("server_time", WholeShape()),
                Field("instance", TextShape()),
                Field("permissions", ObjectOf({ Field("panel", ListOf(TextShape())), Field("apps", MapOf(ListOf(TextShape()))) })),
                Field("apps", ListOf(ObjectOf({ Field("name", TextShape()) }))),
                Field("realms", ListOf(OpenShape())),
            })),
            Sends("status", "status", PanelEventGate::Permission, "status.read", true, ObjectOf({
                Field("app", TextShape()),
                Field("state", TextShape(states)),
                Field("since", WholeShape()),
                Field("pid", NullableOf(WholeShape())),
                Field("exit_code", NullableOf(WholeShape())),
                Field("crashes", WholeShape()),
                Field("next_restart", NullableOf(WholeShape())),
            })),
            Later("stats", "stats", PanelEventGate::Permission, "metrics.read"),
            Later("log", "logs", PanelEventGate::Permission, "console.read"),
            Sends("dropped", "", PanelEventGate::StreamPermission, "", true, ObjectOf({
                Field("stream", TextShape()),
                Field("count", WholeShape()),
                Field("first", WholeShape()),
                Field("last", WholeShape()),
            })),
            Later("command.result", "", PanelEventGate::Sender, ""),
            Sends("power.accepted", "status", PanelEventGate::PermissionAndSender, "status.read", true, ObjectOf({
                Field("operation", TextShape()),
                Field("request", NullableOf(TextShape())),
                Field("target", targetShape),
                Field("action", TextShape(actions)),
                Field("apps", ListOf(TextShape())),
                Field("seconds", WholeShape()),
                Field("reason", NullableOf(TextShape())),
                Field("by", TextShape()),
                Field("window", NullableOf(TextShape())),
            })),
            Sends("power.progress", "status", PanelEventGate::PermissionAndSender, "status.read", true, ObjectOf({
                Field("operation", TextShape()),
                Field("app", TextShape()),
                Field("step", TextShape(actions)),
                Field("outcome", TextShape({ "begun", "done", "skipped", "failed" })),
                Field("message", TextShape()),
            })),
            Sends("power.result", "status", PanelEventGate::PermissionAndSender, "status.read", true, ObjectOf({
                Field("operation", TextShape()),
                Field("request", NullableOf(TextShape())),
                Field("target", targetShape),
                Field("action", TextShape(actions)),
                Field("apps", ListOf(TextShape())),
                Field("outcome", TextShape({ "succeeded", "failed" })),
                Field("message", TextShape()),
            })),
            Later("players", "players", PanelEventGate::Permission, "players.read"),
            Later("setup.output", "setup", PanelEventGate::Permission, "clientdata.read"),
            Later("setup.state", "setup", PanelEventGate::Permission, "clientdata.read"),
            Later("backup.progress", "backups", PanelEventGate::Permission, "backups.read"),
            Later("backup.completed", "backups", PanelEventGate::Permission, "backups.read"),
            Later("backup.failed", "backups", PanelEventGate::Permission, "backups.read"),
            Later("restore.progress", "backups", PanelEventGate::Permission, "backups.read"),
            Later("restore.completed", "backups", PanelEventGate::Permission, "backups.read"),
            Later("restore.failed", "backups", PanelEventGate::Permission, "backups.read"),
            Later("update.progress", "updates", PanelEventGate::Permission, "updates.read"),
            Later("update.result", "updates", PanelEventGate::Permission, "updates.read"),
            Later("setting.changed", "settings", PanelEventGate::Permission, "settings.read"),
            Later("reload.result", "reloads", PanelEventGate::Permission, "reload.read"),
            Later("schedule.run.started", "schedules", PanelEventGate::Permission, "schedules.read"),
            Later("schedule.run.waiting", "schedules", PanelEventGate::Permission, "schedules.read"),
            Later("schedule.countdown.tick", "schedules", PanelEventGate::Permission, "schedules.read"),
            Later("schedule.task.started", "schedules", PanelEventGate::Permission, "schedules.read"),
            Later("schedule.task.output", "schedules", PanelEventGate::Permission, "schedules.read"),
            Later("schedule.task.finished", "schedules", PanelEventGate::Permission, "schedules.read"),
            Later("schedule.run.finished", "schedules", PanelEventGate::Permission, "schedules.read"),
            Later("schedule.changed", "schedules", PanelEventGate::Permission, "schedules.read"),
            Later("realm.changed", "", PanelEventGate::Permission, "realms.read"),
            Later("announce.sent", "players", PanelEventGate::Permission, "players.read"),
            Later("file.job", "files", PanelEventGate::Permission, "files.list"),
            Later("node.status", "nodes", PanelEventGate::Permission, "nodes.read"),
            Later("node.move", "nodes", PanelEventGate::Permission, "nodes.read"),
            Later("alert", "alerts", PanelEventGate::Permission, "alerts.read"),
            Later("audit", "audit", PanelEventGate::Permission, "activity.read"),
            Later("permissions.changed", "", PanelEventGate::User, ""),
            Later("throttled", "", PanelEventGate::Sender, ""),
            Sends("error", "", PanelEventGate::Sender, "", true, ObjectOf({
                Field("request", NullableOf(TextShape())),
                Field("code", TextShape()),
                Field("message", TextShape()),
                Field("correlation", TextShape()),
            })),
            Later("session.expiring", "", PanelEventGate::User, ""),
            Sends("pong", "", PanelEventGate::Sender, "", true, ObjectOf({})),
        };
    }

    std::string Quoted(std::string_view text)
    {
        return fmt::format("\"{}\"", text);
    }

    std::string QuotedList(std::vector<std::string_view> const& names)
    {
        std::string text = "[";
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            if (index > 0)
                text += ", ";
            text += Quoted(names[index]);
        }
        return text + "]";
    }

    std::string FieldSchema(PanelField const& field)
    {
        std::string const schema = PanelEventCatalog::SchemaOf(*field.Shape);
        return field.Optional ? "v.optional(" + schema + ")" : schema;
    }

    std::string TopSchema(PanelShape const& shape)
    {
        if (shape.Kind != PanelShapeKind::Object || shape.Fields.empty() || shape.Nullable)
            return PanelEventCatalog::SchemaOf(shape);
        std::string text = "v.looseObject({\n";
        for (PanelField const& field : shape.Fields)
            text += fmt::format("    {}: {},\n", field.Name, FieldSchema(field));
        return text + "})";
    }

    bool Fail(std::string& error, std::string text)
    {
        error = std::move(text);
        return false;
    }
}

std::vector<std::string_view> const& PanelEventCatalog::EnvelopeFields()
{
    static std::vector<std::string_view> const fields{ "v", "type", "id", "scope", "seq", "time", "data" };
    return fields;
}

std::vector<PanelCloseCode> const& PanelEventCatalog::CloseCodes()
{
    static std::vector<PanelCloseCode> const codes{
        { Malformed, "malformed", "Malformed frame or unsupported protocol version" },
        { SessionEnded, "session_ended", "Session ended: sign-out, expiry, or a revoked session" },
        { AccessLost, "access_lost", "Access lost: the user's permissions no longer allow any subscribed stream" },
        { Limits, "limits", "Closed for exceeding limits repeatedly, or for falling so far behind a stream that never drops that its queue filled" },
    };
    return codes;
}

std::vector<PanelEventStream> const& PanelEventCatalog::Streams()
{
    static std::vector<PanelEventStream> const streams = BuildStreams();
    return streams;
}

std::vector<PanelEventType> const& PanelEventCatalog::Types()
{
    static std::vector<PanelEventType> const types = BuildTypes();
    return types;
}

PanelEventStream const* PanelEventCatalog::FindStream(std::string_view name)
{
    for (PanelEventStream const& stream : Streams())
        if (stream.Name == name)
            return &stream;
    return nullptr;
}

PanelEventType const* PanelEventCatalog::Find(PanelEventDirection direction, std::string_view name)
{
    for (PanelEventType const& type : Types())
        if (type.Direction == direction && type.Name == name)
            return &type;
    return nullptr;
}

std::vector<std::string_view> PanelEventCatalog::Names(PanelEventDirection direction)
{
    std::vector<std::string_view> names;
    for (PanelEventType const& type : Types())
        if (type.Direction == direction)
            names.push_back(type.Name);
    return names;
}

std::vector<std::string_view> PanelEventCatalog::BuiltNames(PanelEventDirection direction)
{
    std::vector<std::string_view> names;
    for (PanelEventType const& type : Types())
        if (type.Direction == direction && type.Built)
            names.push_back(type.Name);
    return names;
}

bool PanelEventCatalog::IsSent(std::string_view type, std::string_view stream)
{
    PanelEventType const* const found = Find(PanelEventDirection::FromServer, type);
    return found && found->Built && found->Stream == stream;
}

bool PanelEventCatalog::Validate(PanelEventDirection direction, std::string_view type, nlohmann::json const& data, std::string& error)
{
    PanelEventType const* const found = Find(direction, type);
    if (!found || !found->Data)
        return Fail(error, fmt::format("{} is not a message the panel's event socket {}", Ambrose::ForLog(type, 64), direction == PanelEventDirection::FromClient ? "takes" : "sends"));
    return Validate(*found->Data, data, "data", error);
}

bool PanelEventCatalog::Validate(PanelShape const& shape, nlohmann::json const& value, std::string const& where, std::string& error)
{
    if (value.is_null())
        return shape.Nullable ? true : Fail(error, where + " may not be null");
    switch (shape.Kind)
    {
        case PanelShapeKind::Text:
        {
            if (!value.is_string())
                return Fail(error, where + " must be text");
            if (shape.Choices.empty())
                return true;
            std::string const& text = value.get_ref<std::string const&>();
            if (std::find(shape.Choices.begin(), shape.Choices.end(), text) != shape.Choices.end())
                return true;
            std::string choices;
            for (std::string_view const choice : shape.Choices)
                choices += (choices.empty() ? "" : ", ") + std::string(choice);
            return Fail(error, fmt::format("{} must be one of {}", where, choices));
        }
        case PanelShapeKind::Whole:
            return value.is_number_integer() ? true : Fail(error, where + " must be a whole number");
        case PanelShapeKind::Number:
            return value.is_number() ? true : Fail(error, where + " must be a number");
        case PanelShapeKind::Flag:
            return value.is_boolean() ? true : Fail(error, where + " must be true or false");
        case PanelShapeKind::List:
        {
            if (!value.is_array())
                return Fail(error, where + " must be a list");
            for (std::size_t index = 0; index < value.size(); ++index)
                if (!shape.Item || !Validate(*shape.Item, value[index], fmt::format("{}[{}]", where, index), error))
                    return false;
            return true;
        }
        case PanelShapeKind::Map:
        {
            if (!value.is_object())
                return Fail(error, where + " must be an object");
            for (auto const& [key, item] : value.items())
                if (!shape.Item || !Validate(*shape.Item, item, where + "." + Ambrose::ForLog(key, 64), error))
                    return false;
            return true;
        }
        case PanelShapeKind::Object:
        {
            if (!value.is_object())
                return Fail(error, where + " must be an object");
            for (PanelField const& field : shape.Fields)
            {
                auto const found = value.find(std::string(field.Name));
                if (found == value.end())
                {
                    if (field.Optional)
                        continue;
                    return Fail(error, fmt::format("{}.{} is missing", where, field.Name));
                }
                if (!field.Shape || !Validate(*field.Shape, *found, fmt::format("{}.{}", where, field.Name), error))
                    return false;
            }
            for (auto item = value.begin(); item != value.end(); ++item)
            {
                std::string const& key = item.key();
                bool const known = std::any_of(shape.Fields.begin(), shape.Fields.end(), [&key](PanelField const& field) { return field.Name == key; });
                if (!known)
                    return Fail(error, fmt::format("{} takes no key named {}", where, Ambrose::ForLog(key, 64)));
            }
            return true;
        }
        case PanelShapeKind::Open:
            return value.is_object() ? true : Fail(error, where + " must be an object");
    }
    return Fail(error, where + " has a shape the catalog does not know");
}

std::string PanelEventCatalog::DataName(std::string_view type)
{
    std::string name;
    bool upper = true;
    for (char const character : type)
    {
        if (character == '.' || character == '_')
        {
            upper = true;
            continue;
        }
        name.push_back(upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(character))) : character);
        upper = false;
    }
    return name + "Data";
}

std::string PanelEventCatalog::SchemaOf(PanelShape const& shape)
{
    std::string inner;
    switch (shape.Kind)
    {
        case PanelShapeKind::Text:
            inner = shape.Choices.empty() ? std::string("v.string()") : "v.picklist(" + QuotedList(shape.Choices) + ")";
            break;
        case PanelShapeKind::Whole:
            inner = "v.pipe(v.number(), v.integer())";
            break;
        case PanelShapeKind::Number:
            inner = "v.number()";
            break;
        case PanelShapeKind::Flag:
            inner = "v.boolean()";
            break;
        case PanelShapeKind::List:
            inner = "v.array(" + (shape.Item ? SchemaOf(*shape.Item) : std::string("v.unknown()")) + ")";
            break;
        case PanelShapeKind::Map:
            inner = "v.record(v.string(), " + (shape.Item ? SchemaOf(*shape.Item) : std::string("v.unknown()")) + ")";
            break;
        case PanelShapeKind::Object:
        {
            if (shape.Fields.empty())
            {
                inner = "v.looseObject({})";
                break;
            }
            inner = "v.looseObject({ ";
            for (std::size_t index = 0; index < shape.Fields.size(); ++index)
            {
                if (index > 0)
                    inner += ", ";
                inner += fmt::format("{}: {}", shape.Fields[index].Name, FieldSchema(shape.Fields[index]));
            }
            inner += " })";
            break;
        }
        case PanelShapeKind::Open:
            inner = "v.looseObject({})";
            break;
    }
    return shape.Nullable ? "v.nullable(" + inner + ")" : inner;
}

std::string PanelEventCatalog::RenderTypeScript()
{
    std::vector<std::string_view> streams;
    std::vector<std::string_view> served;
    for (PanelEventStream const& stream : Streams())
    {
        streams.push_back(stream.Name);
        if (stream.Served)
            served.push_back(stream.Name);
    }

    std::string out;
    out += "/*\n";
    out += " * Project Ambrose by Imjustchico\n";
    out += " * The panel event socket's protocol, rendered by PanelEventCatalog::RenderTypeScript from the supervisor's one catalog and never edited by hand: the version, the envelope, the close codes, every stream with the permission that reads it and whether this build serves it, every message in each direction with the ones this build handles and sends and the stream each server message belongs to, and a Valibot schema with its type for each message's data. PanelEventCatalogTest fails when this file differs from what the catalog renders, and writes it again when AMBROSE_WRITE_PANEL_EVENT_TYPES is 1.\n";
    out += " */\n";
    out += "\n";
    out += "import * as v from \"valibot\";\n";
    out += "\n";
    out += fmt::format("export const protocolVersion = {};\n", Version);
    out += "\n";
    out += "export const envelopeFields = " + QuotedList(EnvelopeFields()) + " as const;\n";
    out += "\n";
    out += "export const closeCodes = {\n";
    for (PanelCloseCode const& code : CloseCodes())
        out += fmt::format("    {}: {},\n", Quoted(code.Name), code.Code);
    out += "} as const;\n";
    out += "\n";
    out += "export const streams = " + QuotedList(streams) + " as const;\n";
    out += "export type Stream = (typeof streams)[number];\n";
    out += "\n";
    out += "export const servedStreams = " + QuotedList(served) + " as const;\n";
    out += "\n";
    out += "export const streamPermissions = {\n";
    for (PanelEventStream const& stream : Streams())
        out += fmt::format("    {}: {},\n", Quoted(stream.Name), Quoted(stream.Permission));
    out += "} as const;\n";
    out += "\n";
    out += "export const clientTypes = " + QuotedList(Names(PanelEventDirection::FromClient)) + " as const;\n";
    out += "export type ClientType = (typeof clientTypes)[number];\n";
    out += "\n";
    out += "export const serverTypes = " + QuotedList(Names(PanelEventDirection::FromServer)) + " as const;\n";
    out += "export type ServerType = (typeof serverTypes)[number];\n";
    out += "\n";
    out += "export const handledTypes = " + QuotedList(BuiltNames(PanelEventDirection::FromClient)) + " as const;\n";
    out += "export type HandledType = (typeof handledTypes)[number];\n";
    out += "\n";
    out += "export const sentTypes = " + QuotedList(BuiltNames(PanelEventDirection::FromServer)) + " as const;\n";
    out += "export type SentType = (typeof sentTypes)[number];\n";
    out += "\n";
    out += "export const streamOfType = {\n";
    for (PanelEventType const& type : Types())
        if (type.Direction == PanelEventDirection::FromServer && !type.Stream.empty())
            out += fmt::format("    {}: {},\n", Quoted(type.Name), Quoted(type.Stream));
    out += "} as const;\n";
    out += "\n";
    out += "export const Scope = v.nullable(v.looseObject({ app: v.optional(v.string()) }));\n";
    out += "export type Scope = v.InferOutput<typeof Scope>;\n";
    out += "\n";
    out += "export const ServerFrame = v.looseObject({\n";
    out += "    v: v.number(),\n";
    out += "    type: v.string(),\n";
    out += "    id: v.nullable(v.string()),\n";
    out += "    scope: Scope,\n";
    out += "    seq: v.nullable(v.number()),\n";
    out += "    time: v.number(),\n";
    out += "    data: v.looseObject({}),\n";
    out += "});\n";
    out += "export type ServerFrame = v.InferOutput<typeof ServerFrame>;\n";
    out += "\n";
    out += "export const ClientFrame = v.looseObject({\n";
    out += fmt::format("    v: v.literal({}),\n", Version);
    out += "    type: v.picklist(clientTypes),\n";
    out += "    id: v.optional(v.string()),\n";
    out += "    scope: v.optional(Scope),\n";
    out += "    seq: v.optional(v.number()),\n";
    out += "    data: v.looseObject({}),\n";
    out += "});\n";
    out += "export type ClientFrame = v.InferOutput<typeof ClientFrame>;\n";
    for (PanelEventDirection const direction : { PanelEventDirection::FromClient, PanelEventDirection::FromServer })
    {
        for (PanelEventType const& type : Types())
        {
            if (type.Direction != direction || !type.Data)
                continue;
            std::string const name = DataName(type.Name);
            out += "\n";
            out += fmt::format("export const {} = {};\n", name, TopSchema(*type.Data));
            out += fmt::format("export type {} = v.InferOutput<typeof {}>;\n", name, name);
        }
    }
    out += "\n";
    out += "export const clientData = {\n";
    for (std::string_view const name : Names(PanelEventDirection::FromClient))
        out += fmt::format("    {}: {},\n", Quoted(name), DataName(name));
    out += "} as const;\n";
    out += "\n";
    out += "export const serverData = {\n";
    for (std::string_view const name : Names(PanelEventDirection::FromServer))
        out += fmt::format("    {}: {},\n", Quoted(name), DataName(name));
    out += "} as const;\n";
    out += "\n";
    out += "export type ClientData<T extends ClientType> = v.InferOutput<(typeof clientData)[T]>;\n";
    out += "export type ServerData<T extends ServerType> = v.InferOutput<(typeof serverData)[T]>;\n";
    return out;
}

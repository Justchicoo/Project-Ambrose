/*
 * Project Ambrose by Imjustchico
 * The one catalog of the panel's event socket, written out as data the way the permission catalog is: the protocol version, the envelope's fields, the close codes and what each means, every stream a page can follow with the permission that reads it, the overflow policy its queue keeps and whether this build serves it, and every message in each direction with its stream, who may send or receive it, whether this build handles or sends it, and the shape of its data. Incoming data is checked against that shape, unknown keys refused, the server writes only the messages the table marks as sent, each on its own stream, and the dashboard's protocol module is rendered from the same table, so the TypeScript types and Valibot schemas cannot drift from what the server writes.
 */

#ifndef AMBROSE_PANELEVENTCATALOG_H
#define AMBROSE_PANELEVENTCATALOG_H

#include "StreamSubscription.h"
#include "Types.h"

#include <nlohmann/json_fwd.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

enum class PanelShapeKind : uint8
{
    Text,
    Whole,
    Number,
    Flag,
    List,
    Map,
    Object,
    Open
};

struct PanelShape;

struct PanelField
{
    std::string_view Name = {};
    std::shared_ptr<PanelShape const> Shape = {};
    bool Optional = false;
};

struct PanelShape
{
    PanelShapeKind Kind = PanelShapeKind::Open;
    bool Nullable = false;
    std::vector<std::string_view> Choices = {};
    std::vector<PanelField> Fields = {};
    std::shared_ptr<PanelShape const> Item = {};
};

enum class PanelEventDirection : uint8
{
    FromClient,
    FromServer
};

enum class PanelEventGate : uint8
{
    None,
    SignedIn,
    Permission,
    StreamPermission,
    ActionPermission,
    PermissionAndSender,
    Sender,
    User
};

struct PanelEventStream
{
    std::string_view Name = {};
    std::string_view Permission = {};
    StreamOverflow Overflow = StreamOverflow::DropOldest;
    bool Served = false;
};

struct PanelEventType
{
    std::string_view Name = {};
    PanelEventDirection Direction = PanelEventDirection::FromServer;
    std::string_view Stream = {};
    PanelEventGate Gate = PanelEventGate::None;
    std::string_view Permission = {};
    bool Built = false;
    std::shared_ptr<PanelShape const> Data = {};
};

struct PanelCloseCode
{
    uint16 Code = 0;
    std::string_view Name = {};
    std::string_view Meaning = {};
};

class PanelEventCatalog
{
public:
    static constexpr uint64 Version = 1;
    static constexpr uint16 Malformed = 4400;
    static constexpr uint16 SessionEnded = 4401;
    static constexpr uint16 AccessLost = 4403;
    static constexpr uint16 Limits = 4429;

    PanelEventCatalog() = delete;

    static std::vector<std::string_view> const& EnvelopeFields();
    static std::vector<PanelCloseCode> const& CloseCodes();
    static std::vector<PanelEventStream> const& Streams();
    static std::vector<PanelEventType> const& Types();

    static PanelEventStream const* FindStream(std::string_view name);
    static PanelEventType const* Find(PanelEventDirection direction, std::string_view name);
    static std::vector<std::string_view> Names(PanelEventDirection direction);
    static std::vector<std::string_view> BuiltNames(PanelEventDirection direction);
    static bool IsSent(std::string_view type, std::string_view stream);

    static bool Validate(PanelEventDirection direction, std::string_view type, nlohmann::json const& data, std::string& error);
    static bool Validate(PanelShape const& shape, nlohmann::json const& value, std::string const& where, std::string& error);

    static std::string DataName(std::string_view type);
    static std::string SchemaOf(PanelShape const& shape);
    static std::string RenderTypeScript();
};

#endif

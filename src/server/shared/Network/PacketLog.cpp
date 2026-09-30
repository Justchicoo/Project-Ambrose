/*
 * Project Ambrose by Imjustchico
 * Names a message from the definitions the server loaded from the client's own XML, decodes its fields to write them, and writes a message the definitions do not hold, or one that does not decode, by its service and order alone; every message whose tag begins MSG_USER_AUTHEN carries a password or a key, so its field values are replaced before the line is formed.
 */

#include "PacketLog.h"

#include "DynamicMessage.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtil.h"

#include <algorithm>
#include <cctype>
#include <fmt/format.h>

namespace
{
    std::string Upper(std::string_view text)
    {
        std::string upper(text);
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return upper;
    }

    bool Holds(std::vector<std::string> const& names, std::string_view tag)
    {
        std::string const wanted = Upper(tag);
        return std::find(names.begin(), names.end(), wanted) != names.end();
    }
}

std::vector<std::string> PacketLog::Options::ParseNames(std::string_view list)
{
    std::vector<std::string> names;
    std::size_t start = 0;
    while (start <= list.size())
    {
        std::size_t const end = list.find_first_of(", ", start);
        std::string_view const name = Ambrose::Trim(list.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (!name.empty())
            names.push_back(Upper(name));
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }
    return names;
}

PacketLog::Options PacketLog::Options::FromSettings()
{
    Options options;
    options.Enabled = sSettings.Get<bool>("Network.PacketLog.Enable");
    if (!options.Enabled)
        return options;
    options.Filter = ParseNames(sSettings.Get<std::string>("Network.PacketLog.Filter"));
    options.Suppress = ParseNames(sSettings.Get<std::string>("Network.PacketLog.Suppress"));
    return options;
}

bool PacketLog::Options::Wants(std::string_view tag) const
{
    if (!Enabled || Holds(Suppress, tag))
        return false;
    return Filter.empty() || Holds(Filter, tag);
}

bool PacketLog::IsRedacted(std::string_view tag) noexcept
{
    return tag.starts_with("MSG_USER_AUTHEN");
}

std::string PacketLog::Format(Direction direction, uint16 sessionId, MessageCatalogPtr const& catalog, uint8 serviceId, uint8 order, std::span<uint8 const> body)
{
    std::string_view const arrow = direction == Direction::ClientToServer ? "C->S" : "S->C";
    MessageInfo const* const info = catalog ? catalog->Find(serviceId, order) : nullptr;
    if (!info || !info->Definition || !info->Protocol)
        return fmt::format("session {} {} unknown message ({}:{}), {} byte(s)", sessionId, arrow, serviceId, order, body.size());

    std::string line = fmt::format("session {} {} {} {} ({}:{})", sessionId, arrow, info->Protocol->ProtocolType, info->Definition->Tag, serviceId, order);
    bool const redacted = IsRedacted(info->Definition->Tag);
    std::optional<DynamicMessage> message = DynamicMessage::Create(catalog, serviceId, order);
    if (!message || message->Decode(body) != MessageDecodeStatus::Ok)
        return line + fmt::format(", {} byte(s) that do not decode", body.size());
    std::vector<DmlValue> const& values = message->GetValues();
    std::vector<FieldDef> const& fields = info->Definition->Fields;
    for (std::size_t index = 0; index < fields.size() && index < values.size(); ++index)
        line += fmt::format(" {}={}", fields[index].Name, redacted ? std::string("<redacted>") : DynamicMessage::FormatValue(values[index]));
    return line;
}

void PacketLog::Record(Direction direction, uint16 sessionId, uint8 serviceId, uint8 order, std::span<uint8 const> body)
{
    if (!sSettings.Get<bool>("Network.PacketLog.Enable"))
        return;
    Options const options = Options::FromSettings();
    MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
    MessageInfo const* const info = catalog ? catalog->Find(serviceId, order) : nullptr;
    std::string_view const tag = info && info->Definition ? std::string_view(info->Definition->Tag) : std::string_view();
    if (!tag.empty() && !options.Wants(tag))
        return;
    if (tag.empty() && !options.Filter.empty())
        return;
    LOG_INFO("network.packets", "{}", Format(direction, sessionId, catalog, serviceId, order, body));
}

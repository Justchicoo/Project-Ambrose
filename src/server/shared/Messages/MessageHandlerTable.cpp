/*
 * Project Ambrose by Imjustchico
 * Stores message rules, declares every handled and sent message with the registry, checks rules and the coverage of the app's own services against a catalog, and caches each catalog's service and order slots behind a lock-free read until a new catalog is loaded.
 */

#include "MessageHandlerTable.h"

#include <algorithm>

namespace
{
    constexpr std::size_t SlotCount = std::size_t{ 256 } * 256;

    std::size_t SlotOf(uint8 serviceId, uint8 order) noexcept
    {
        return std::size_t{ serviceId } * 256 + order;
    }
}

MessageHandlerTableBase::MessageHandlerTableBase(std::string appName, std::vector<uint8> ownServices, QueuedMessageDrain drain)
    : _appName(std::move(appName)), _ownServices(std::move(ownServices)), _drain(drain)
{
}

bool MessageHandlerTableBase::IsOwnService(uint8 serviceId) const noexcept
{
    return std::find(_ownServices.begin(), _ownServices.end(), serviceId) != _ownServices.end();
}

void MessageHandlerTableBase::Pending(uint8 serviceId, std::string_view tag, SessionStatusMask statuses)
{
    MessageRule rule;
    rule.ServiceId = serviceId;
    rule.Tag = std::string(tag);
    rule.Kind = MessageRuleKind::Pending;
    rule.Statuses = statuses;
    AddRule(std::move(rule));
}

void MessageHandlerTableBase::Refuse(uint8 serviceId, std::string_view tag)
{
    MessageRule rule;
    rule.ServiceId = serviceId;
    rule.Tag = std::string(tag);
    rule.Kind = MessageRuleKind::Refused;
    AddRule(std::move(rule));
}

std::size_t MessageHandlerTableBase::AddRule(MessageRule rule)
{
    std::lock_guard const lock(_resolutionMutex);
    _rules.push_back(std::move(rule));
    _resolution.store(nullptr);
    return _rules.size() - 1;
}

bool MessageHandlerTableBase::Declare(MessageRegistry& registry, std::vector<std::string>& errors) const
{
    bool declared = true;
    for (MessageRule const& rule : _rules)
        if (rule.Declare && !rule.Declare(registry, errors))
            declared = false;
    for (auto const declare : _sentDeclarations)
        if (!declare(registry, errors))
            declared = false;
    return declared;
}

bool MessageHandlerTableBase::Validate(MessageCatalog const& catalog, std::vector<std::string>& errors) const
{
    bool valid = true;
    std::vector<std::size_t> owners(SlotCount, _rules.size());
    for (std::size_t index = 0; index < _rules.size(); ++index)
    {
        MessageRule const& rule = _rules[index];
        MessageInfo const* const info = catalog.Find(rule.ServiceId, rule.Tag);
        if (!info)
        {
            errors.push_back(fmt::format("{} lists {} in service {}, which the message definitions do not have", _appName, rule.Tag, rule.ServiceId));
            valid = false;
            continue;
        }
        std::size_t const slot = SlotOf(rule.ServiceId, info->Definition->Order);
        if (owners[slot] != _rules.size())
        {
            errors.push_back(fmt::format("{} lists {} in service {} more than once", _appName, rule.Tag, rule.ServiceId));
            valid = false;
            continue;
        }
        owners[slot] = index;
        if (rule.Kind != MessageRuleKind::Refused && rule.Statuses == 0)
        {
            errors.push_back(fmt::format("{} accepts {} in no session status", _appName, rule.Tag));
            valid = false;
        }
        if (rule.Kind == MessageRuleKind::Handled && rule.Processing == MessageProcessing::Queued && _drain == QueuedMessageDrain::None)
        {
            errors.push_back(fmt::format("{} queues {}, but nothing drains its sessions' queues", _appName, rule.Tag));
            valid = false;
        }
    }
    for (MessageInfo const& info : catalog.GetMessages())
    {
        uint8 const serviceId = info.Protocol->ServiceId;
        if (IsOwnService(serviceId) && owners[SlotOf(serviceId, info.Definition->Order)] == _rules.size())
        {
            errors.push_back(fmt::format("{} has no rule for {}", _appName, DescribeMessage(info)));
            valid = false;
        }
    }
    return valid;
}

std::shared_ptr<MessageHandlerTableBase::Resolution const> MessageHandlerTableBase::Resolve(MessageCatalogPtr const& catalog) const
{
    std::shared_ptr<Resolution const> current = _resolution.load();
    if (current && current->Catalog.lock() == catalog)
        return current;

    std::lock_guard const lock(_resolutionMutex);
    current = _resolution.load();
    if (current && current->Catalog.lock() == catalog)
        return current;
    auto resolution = std::make_shared<Resolution>();
    resolution->Catalog = catalog;
    resolution->Slots.assign(SlotCount, 0);
    for (std::size_t index = 0; index < _rules.size(); ++index)
    {
        MessageInfo const* const info = catalog->Find(_rules[index].ServiceId, _rules[index].Tag);
        if (!info)
            continue;
        uint16& slot = resolution->Slots[SlotOf(_rules[index].ServiceId, info->Definition->Order)];
        if (slot == 0)
            slot = static_cast<uint16>(index + 1);
    }
    std::shared_ptr<Resolution const> published = std::move(resolution);
    _resolution.store(published);
    return published;
}

MessageRule const* MessageHandlerTableBase::FindRule(MessageCatalogPtr const& catalog, uint8 serviceId, uint8 order) const
{
    if (!catalog)
        return nullptr;
    std::shared_ptr<Resolution const> const resolution = Resolve(catalog);
    uint16 const slot = resolution->Slots[SlotOf(serviceId, order)];
    return slot == 0 ? nullptr : &_rules[slot - 1];
}

std::string MessageHandlerTableBase::DescribeMessage(MessageInfo const& info)
{
    return fmt::format("{} {} ({}:{})", info.Protocol->ProtocolType, info.Definition->Tag, info.Protocol->ServiceId, info.Definition->Order);
}

/*
 * Project Ambrose by Imjustchico
 * Registers, removes, and looks up appender type factories by type id.
 */

#include "AppenderRegistry.h"

#include <utility>

bool AppenderRegistry::Register(AppenderTypeInfo info)
{
    if (info.Type == AppenderType::None || !info.Create)
        return false;
    AppenderType const type = info.Type;
    return _types.emplace(type, std::move(info)).second;
}

bool AppenderRegistry::Unregister(AppenderType type)
{
    return _types.erase(type) > 0;
}

AppenderTypeInfo const* AppenderRegistry::Find(AppenderType type) const
{
    auto const it = _types.find(type);
    return it == _types.end() ? nullptr : &it->second;
}

std::vector<AppenderType> AppenderRegistry::GetTypes() const
{
    std::vector<AppenderType> types;
    types.reserve(_types.size());
    for (auto const& [type, info] : _types)
        types.push_back(type);
    return types;
}

/*
 * Project Ambrose by Imjustchico
 * Answers the settings command: bare or list prints each setting with its value and where the value comes from, get adds its type, bounds, when a change applies and what it does, set and reset say what they did or why they refused, and history prints a key's newest changes with who made them, from where and why. A refusal still counts as the command being used correctly; only a missing key or value is a usage error.
 */

#include "SettingsCommand.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <chrono>
#include <ctime>

namespace
{
    std::string Joined(std::vector<std::string> const& arguments, std::size_t from)
    {
        if (from >= arguments.size())
            return {};
        return fmt::format("{}", fmt::join(arguments.begin() + static_cast<std::ptrdiff_t>(from), arguments.end(), " "));
    }

    std::string WithUnit(SettingDeclaration const& declaration, std::string const& value)
    {
        return declaration.Unit.empty() || value.empty() ? value : fmt::format("{} {}", value, declaration.Unit);
    }

    std::string Stamp(int64 epochSeconds)
    {
        std::time_t const seconds = static_cast<std::time_t>(epochSeconds);
        std::tm parts{};
#ifdef _WIN32
        if (gmtime_s(&parts, &seconds) != 0)
            return "an unreadable time";
#else
        if (!gmtime_r(&seconds, &parts))
            return "an unreadable time";
#endif
        return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}Z", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
    }

    std::string Line(SettingView const& view)
    {
        std::string const value = view.Value.empty() ? std::string("empty") : WithUnit(view.Declaration, view.Value);
        return fmt::format("{} = {} ({})", view.Declaration.Key, value, Settings::LayerName(view.Layer));
    }

    bool List(Settings& settings, std::string_view category, SettingsCommand::Reply const& reply)
    {
        std::vector<SettingView> const views = settings.List(category);
        if (views.empty())
        {
            reply(category.empty() ? std::string("This app declares no live settings") : fmt::format("No live setting is in the category {}", category));
            return true;
        }
        std::string current;
        for (SettingView const& view : views)
        {
            if (view.Declaration.Category != current)
            {
                current = view.Declaration.Category;
                reply(fmt::format("{}:", current));
            }
            reply(fmt::format("  {}", Line(view)));
        }
        return true;
    }

    bool Get(Settings& settings, std::string_view key, SettingsCommand::Reply const& reply)
    {
        std::optional<SettingView> const view = settings.Describe(key);
        if (!view)
        {
            reply(fmt::format("No setting is named {}", Ambrose::ForLog(key, 128)));
            return true;
        }
        SettingDeclaration const& declaration = view->Declaration;
        reply(fmt::format("{}, from {}", Line(*view), view->Origin));
        std::string const bounds = Settings::DescribeBounds(declaration);
        reply(fmt::format("{}{}, default {}, applies {}", Settings::TypeName(declaration.Type), bounds.empty() ? std::string() : " " + bounds,
            declaration.Default.empty() ? std::string("empty") : WithUnit(declaration, declaration.Default), Settings::ApplyName(declaration.Apply)));
        if (view->Persisted && view->Layer != SettingLayer::Live)
            reply(fmt::format("The settings table holds {} for it, which {} overrides", *view->Persisted, view->Origin));
        reply(declaration.Description);
        return true;
    }

    bool History(Settings& settings, std::string_view key, SettingsCommand::Reply const& reply)
    {
        std::vector<SettingAuditEntry> entries;
        std::string error;
        if (!settings.History(key, entries, error))
        {
            reply(error);
            return true;
        }
        if (entries.empty())
        {
            reply(fmt::format("{} has never been changed live", key));
            return true;
        }
        for (SettingAuditEntry const& entry : entries)
            reply(fmt::format("{}  {} to {} by {} from {}{}", Stamp(entry.EpochSeconds), entry.OldValue.empty() ? std::string("empty") : entry.OldValue,
                entry.NewValue.empty() ? std::string("empty") : entry.NewValue, entry.Who, entry.Source, entry.Reason.empty() ? std::string() : fmt::format(": {}", entry.Reason)));
        return true;
    }
}

bool SettingsCommand::Run(Settings& settings, std::vector<std::string> const& arguments, SettingAuthor const& author, Reply const& reply)
{
    std::string const action = arguments.empty() ? std::string("list") : Ambrose::ToLower(arguments.front());
    if (action == "list")
        return arguments.size() <= 2 && List(settings, arguments.size() == 2 ? std::string_view(arguments[1]) : std::string_view(), reply);
    if (action == "get")
        return arguments.size() == 2 && Get(settings, arguments[1], reply);
    if (action == "history")
        return arguments.size() == 2 && History(settings, arguments[1], reply);
    if (action == "set")
    {
        if (arguments.size() < 3)
            return false;
        reply(settings.Set(arguments[1], arguments[2], author, Joined(arguments, 3)).Message);
        return true;
    }
    if (action == "reset")
    {
        if (arguments.size() < 2)
            return false;
        reply(settings.Reset(arguments[1], author, Joined(arguments, 2)).Message);
        return true;
    }
    return false;
}

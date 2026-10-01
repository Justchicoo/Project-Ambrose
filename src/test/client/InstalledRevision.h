/*
 * Project Ambrose by Imjustchico
 * Lets a client test take its expected counts from the revision it reads: the installed revision is read once from Bin/revision.dat of the install AMBROSE_CLIENT_DIR names, a revision matches a recorded one by its leading rNNNNNN, and a value is expected to equal the one recorded for its revision, or, for a revision with none recorded, is printed with that revision so it can be recorded and is expected above zero where the recorded ones are.
 */

#ifndef AMBROSE_INSTALLEDREVISION_H
#define AMBROSE_INSTALLEDREVISION_H

#include "ClientLocator.h"
#include "Environment.h"
#include "LogConfig.h"

#include <gtest/gtest.h>

#include <initializer_list>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace InstalledRevision
{
    inline std::string const& Get()
    {
        static std::string const revision = []
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            if (!client || client->empty())
                return std::string();
            LocalClientSystem const system;
            std::optional<ClientInstall> const install = ClientInstall::Inspect(system, LogConfig::Utf8Path(*client));
            return install ? install->Revision : std::string();
        }();
        return revision;
    }

    inline bool Matches(std::string_view revision, std::string_view recorded)
    {
        return revision.starts_with(recorded) && (revision.size() == recorded.size() || revision[recorded.size()] == '.');
    }

    inline bool Is(std::string_view recorded)
    {
        return Matches(Get(), recorded);
    }

    template<class T>
    void ExpectFor(std::string_view revision, T const& actual, std::initializer_list<std::pair<std::string_view, std::type_identity_t<T>>> recorded, std::string_view what)
    {
        bool positive = false;
        for (auto const& [name, value] : recorded)
        {
            if (Matches(revision, name))
            {
                EXPECT_EQ(actual, value) << what << " on " << revision;
                return;
            }
            if constexpr (std::is_arithmetic_v<T>)
                positive = positive || value > T{};
        }
        std::cout << "[ REVISION ] " << what << ": " << actual << " on " << (revision.empty() ? std::string_view("an unknown revision") : revision) << std::endl;
        if constexpr (std::is_arithmetic_v<T>)
        {
            if (positive)
            {
                EXPECT_GT(actual, T{}) << what << " on " << revision;
            }
        }
    }

    template<class T>
    void Expect(T const& actual, std::initializer_list<std::pair<std::string_view, std::type_identity_t<T>>> recorded, std::string_view what)
    {
        ExpectFor(Get(), actual, recorded, what);
    }
}

#endif

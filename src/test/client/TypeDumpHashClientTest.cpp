/*
 * Project Ambrose by Imjustchico
 * Checks every class hash and property hash in the user's own type dump against the client string hashes, when AMBROSE_TYPE_DUMP_PATH names the dump, and for r806919 checks its class and property counts against the shape its own header names, the reference dump's or the larger one this project's extractor writes.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "StringHash.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace
{
    constexpr std::size_t ReportedMismatches = 20;
    constexpr std::string_view PinnedRevision = "r806919.Wizard_1_610";
    constexpr std::string_view OurExtractor = "typeextract";

    std::optional<uint32> ReadHash(nlohmann::json const& value)
    {
        if (!value.is_number_unsigned() || value.get<uint64>() > 0xFFFFFFFFull)
            return std::nullopt;
        return static_cast<uint32>(value.get<uint64>());
    }
}

TEST(TypeDumpHashClientTest, EveryClassAndPropertyHashMatches)
{
    std::optional<std::string> const path = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!path || path->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to the r806919 type dump (format v2) from your own client to run this test";

    std::ifstream stream(LogConfig::Utf8Path(*path), std::ios::binary);
    ASSERT_TRUE(stream) << "cannot open " << *path;
    nlohmann::json const dump = nlohmann::json::parse(stream);
    ASSERT_EQ(dump.at("version").get<int>(), 2);

    std::size_t classes = 0;
    std::size_t properties = 0;
    std::size_t mismatches = 0;
    auto const report = [&mismatches](std::string const& what)
    {
        if (++mismatches <= ReportedMismatches)
            ADD_FAILURE() << what;
    };
    for (auto const& [key, info] : dump.at("classes").items())
    {
        ++classes;
        std::string const name = info.at("name").get<std::string>();
        std::optional<uint32> const hash = ReadHash(info.at("hash"));
        if (!hash || *hash != StringHash::KiStringHash(name) || std::to_string(*hash) != key)
            report("class " + name + " under key " + key + " does not hash to its recorded value");
        for (auto const& [propertyName, property] : info.at("properties").items())
        {
            ++properties;
            std::optional<uint32> const propertyHash = ReadHash(property.at("hash"));
            if (!propertyHash || *propertyHash != StringHash::PropertyHash(property.at("type").get<std::string>(), propertyName))
                report(name + "::" + propertyName + " does not hash to its recorded value");
        }
    }
    EXPECT_EQ(mismatches, 0u);
    std::string const revision = dump.value("revision", std::string());
    bool const ours = dump.value("extractor", std::string()).starts_with(OurExtractor);
    std::cout << "Checked " << classes << " classes and " << properties << " properties of "
              << (revision.empty() ? std::string("a dump naming no revision") : revision)
              << (ours ? ", written by this project's extractor" : ", not written by this project's extractor") << std::endl;
    if (revision.empty() || revision == PinnedRevision)
    {
        EXPECT_EQ(classes, ours ? 6986u : 6981u);
        EXPECT_EQ(properties, ours ? 49465u : 49461u);
    }
}

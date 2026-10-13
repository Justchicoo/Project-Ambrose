/*
 * Project Ambrose by Imjustchico
 * A patch server over loopback for tests: Ambrose-authored PATCH definitions, a synthetic manifest written where patchlist_generator would write it, a config file the test can rewrite and reload, the patch listener started from it, and helpers that send a declared message and read the next one back decoded.
 */

#ifndef AMBROSE_PATCHTESTHARNESS_H
#define AMBROSE_PATCHTESTHARNESS_H

#include "ConfigMgr.h"
#include "Crc32.h"
#include "FakeSessionClient.h"
#include "FrameWriter.h"
#include "LatestFileList.h"
#include "LogTestDirectory.h"
#include "MessageRegistry.h"
#include "PatchListener.h"
#include "PatchMessageFixtures.h"
#include "PatchMessageTable.h"
#include "PatchMgr.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace PatchTesting
{
    inline constexpr std::string_view Revision = "r806919.Wizard_1_610";
    inline constexpr std::string_view BaseUrl = "http://patch.example.test:12501";

    inline LatestFileList SyntheticList(uint32 extraFiles = 0)
    {
        LatestFileList list;
        LatestFileList::Package base;
        base.Name = "Base";
        base.Records = { { "Data/GameData/Root.wad", "", 3, 2048u, 512u, 256u, 0x12345678u, 0x9ABCDEF0u }, { "Bin/a.dll", "", 1, 64u, 0u, 0u, 0x01020304u, 0u } };
        for (uint32 index = 0; index < extraFiles; ++index)
            base.Records.push_back({ "Bin/extra" + std::to_string(index) + ".dll", "", 1, 32u + index, 0u, 0u, 0xA0000000u + index, 0u });
        LatestFileList::Package zone;
        zone.Name = "ZoneA";
        zone.Records = { { "Data/GameData/ZoneA.wad", "", 3, 1024u, 256u, 128u, 0xAABBCCDDu, 0x11223344u } };
        list.Packages = { base, zone };
        return list;
    }

    inline std::vector<uint8> ReadFile(std::filesystem::path const& file)
    {
        std::ifstream stream(file, std::ios::binary);
        return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
    }

    inline void WriteFile(std::filesystem::path const& file, std::vector<uint8> const& bytes)
    {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    inline uint32 ClientCrc(std::vector<uint8> const& bytes)
    {
        return Crc32::Client().Update(std::span<uint8 const>(bytes)).GetValue();
    }

    template<DeclaredMessage T>
    void Send(FakeSessionClient& client, T const& message)
    {
        ByteBuffer body;
        sMessageRegistry.Encode(message, body);
        MessageInfo const& info = sMessageRegistry.GetCatalog()->GetInfo<T>();
        ByteBuffer frame;
        FrameWriter::WriteDml(frame, info.Protocol->ServiceId, static_cast<uint8>(info.Definition->Order), body.GetData());
        client.Send(frame);
    }

    template<DeclaredMessage T>
    std::optional<T> ReadMessage(FakeSessionClient& client, std::chrono::milliseconds timeout = std::chrono::seconds(20))
    {
        std::optional<DmlMessageData> const data = ReadNextDml(client, timeout);
        if (!data)
            return std::nullopt;
        MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
        MessageInfo const& info = catalog->GetInfo<T>();
        EXPECT_EQ(data->ServiceId, info.Protocol->ServiceId);
        EXPECT_EQ(data->Order, info.Definition->Order) << "expected " << T::Tag;
        if (data->ServiceId != info.Protocol->ServiceId || data->Order != info.Definition->Order)
            return std::nullopt;
        T message;
        if (catalog->Decode(std::span<uint8 const>(data->Body), message) != MessageDecodeStatus::Ok)
            return std::nullopt;
        return message;
    }

    class PatchServerHarness
    {
    public:
        PatchServerHarness() : _config([](std::string const&) { return std::optional<std::string>(); })
        {
            sPatchMgr.Reset();
            sMessageRegistry.Clear();
            std::vector<std::string> errors;
            EXPECT_TRUE(PatchMessageTable::Get().Declare(sMessageRegistry, errors));
            MessageDefinitionSet definitions;
            EXPECT_TRUE(PatchMessageFixtures::AddTo(definitions));
            EXPECT_TRUE(sMessageRegistry.Load(std::move(definitions)));
            EXPECT_TRUE(PatchMessageTable::Get().Validate(*sMessageRegistry.GetCatalog(), errors));
            for (std::string const& error : errors)
                ADD_FAILURE() << error;

            WriteFile(GetListFile(), SyntheticList().WriteBinary());
            WriteConfig(0);
            EXPECT_TRUE(_config.LoadInitial(GetConfigFile()).Succeeded());
            sPatchMgr.LoadSettings(_config);
            EXPECT_TRUE(sPatchMgr.LoadManifest(errors));
            std::string error;
            EXPECT_TRUE(_listener.Start(_config, error)) << error;
        }

        ~PatchServerHarness()
        {
            _listener.Stop();
            sPatchMgr.UnregisterReloadTarget();
            sPatchMgr.Reset();
            sMessageRegistry.Clear();
        }

        PatchServerHarness(PatchServerHarness const&) = delete;
        PatchServerHarness& operator=(PatchServerHarness const&) = delete;

        std::filesystem::path GetOutputDir() const { return _directory.Path() / "patch"; }
        std::filesystem::path GetListFile() const { return GetOutputDir() / ("V_" + std::string(Revision)) / "LatestFileList.bin"; }
        std::filesystem::path GetConfigFile() const { return _directory.Path() / "patchserver.conf"; }
        ConfigMgr& GetConfig() noexcept { return _config; }
        PatchListener& GetListener() noexcept { return _listener; }

        void WriteConfig(uint16 port) const
        {
            std::string output = ConfigMgr::PathToUtf8(GetOutputDir());
            for (char& c : output)
                if (c == '\\')
                    c = '/';
            std::ofstream stream(GetConfigFile(), std::ios::binary | std::ios::trunc);
            stream << "BindIP = 127.0.0.1\nPatchServerPort = " << port << "\nNetwork.Threads = 1\nHttp.PublicBaseUrl = \"" << BaseUrl << "/\"\nPatch.OutputDir = \"" << output
                   << "\"\nPatch.SupportedLocales = English\n";
        }

        std::unique_ptr<FakeSessionClient> Connect(uint16 port = 0) const
        {
            return std::make_unique<FakeSessionClient>(port != 0 ? port : _listener.GetPort());
        }

    private:
        LogTestDirectory _directory;
        ConfigMgr _config;
        PatchListener _listener;
    };

    inline std::optional<PatchMessages::LatestFileListV2> AskForFileList(FakeSessionClient& client, std::string locale = "English")
    {
        if (client.Handshake() == 0)
            return std::nullopt;
        PatchMessages::LatestFileListV2 request;
        request.Locale = std::move(locale);
        Send(client, request);
        return ReadMessage<PatchMessages::LatestFileListV2>(client);
    }
}

#endif

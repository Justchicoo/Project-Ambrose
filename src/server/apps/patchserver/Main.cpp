/*
 * Project Ambrose by Imjustchico
 * Patch server entry point: finds the install for its message definitions, declares the patch message table and checks it against them, loads the Patch options and the manifest patchlist_generator wrote, serving none until one loads, listens for clients on TCP service 8, offers the manifest as a reload target for the console and the admin API that keeps the old one serving when a new one fails, and on a configuration reload applies the Patch options, reloading the manifest when its folder or revision changed, and moves the listener live when its address or port changed, keeping the old one when the new bind fails, until shutdown.
 */

#include "AdminCapabilities.h"
#include "ClientLocator.h"
#include "ClientSetup.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "Log.h"
#include "MessageRegistry.h"
#include "PatchListener.h"
#include "PatchMessageTable.h"
#include "PatchMgr.h"
#include "ServerApp.h"

#include <fmt/format.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    class PatchServerApp : public ServerApp
    {
    public:
        PatchServerApp() : ServerApp({ "patchserver", "patchserver.conf", 12510 }, sConfigMgr, sLog, std::cout, std::cerr)
        {
        }

    protected:
        std::vector<RestartRequiredOption> GetRestartRequiredOptions() const override
        {
            return { ClientSetup::RestartRequiredOptions.begin(), ClientSetup::RestartRequiredOptions.end() };
        }

        bool OnStart() override
        {
            LocalClientSystem const system;
            ClientSetup::Report const report = [](bool warning, std::string const& text)
            {
                if (warning)
                    LOG_WARN("server.patchserver", "{}", text);
                else
                    LOG_INFO("server.patchserver", "{}", text);
            };
            std::unique_ptr<SetupPrompt> const prompt = ClientSetup::ServerPrompt(std::cout, Config());
            prompt->SetCancellation([this] { return PollStopRequested(); });
            ClientSetupResult const setup = ClientSetup::ForServer(Config(), *prompt, system, ClientSetup::ServerTypeDumps(Config(), system, report, [this] { return PollStopRequested(); }),
                { "patchserver", true, false, false }, report);
            SetClientSetup(setup);
            if (PollStopRequested())
                return false;

            MessageHandlerTable<PatchSession> const& messages = PatchMessageTable::Get();
            std::vector<std::string> messageErrors;
            if (!messages.Declare(sMessageRegistry, messageErrors))
            {
                for (std::string const& error : messageErrors)
                    LOG_ERROR("server.patchserver", "{}", error);
                LOG_ERROR("server.patchserver", "The patch message table does not match the loaded message definitions");
                return false;
            }
            if (!setup.Install)
                LOG_WARN("server.patchserver", "No Wizard101 install is in use, so no message definitions are loaded and file list requests cannot be read or answered");
            else if (!sMessageRegistry.LoadFromClient(setup.Install->Root))
            {
                LOG_ERROR("server.patchserver", "Cannot load the message definitions from the client in {}", ClientLocator::PathText(setup.Install->Root));
                return false;
            }
            else if (!messages.Validate(*sMessageRegistry.GetCatalog(), messageErrors))
            {
                for (std::string const& error : messageErrors)
                    LOG_ERROR("server.patchserver", "{}", error);
                LOG_ERROR("server.patchserver", "The patch message table does not match the client's message definitions in {}", ClientLocator::PathText(setup.Install->Root));
                return false;
            }
            else
                SetMessageSource(setup.Install->Root);

            sPatchMgr.LoadSettings(Config());
            if (sPatchMgr.GetSettings()->OutputDir.empty())
                LOG_WARN("server.patchserver", "Patch.OutputDir is empty, so no manifest is served; point it at the folder patchlist_generator wrote and run 'reload {}'", PatchMgr::ReloadTarget);
            else if (std::vector<std::string> errors; !sPatchMgr.LoadManifest(errors))
                LOG_ERROR("server.patchserver", "No manifest is served until one loads; fix the {} problem(s) above and run 'reload {}'", errors.size(), PatchMgr::ReloadTarget);
            if (sPatchMgr.RegisterReloadTarget())
                sAdminCapabilities.AddReloadTarget(std::string(PatchMgr::ReloadTarget));

            std::string error;
            if (!_listener.Start(Config(), error))
            {
                LOG_ERROR("server.patchserver", "Cannot listen for clients: {}", error);
                sPatchMgr.UnregisterReloadTarget();
                return false;
            }
            SetListener(_listener.GetBindIp(), _listener.GetPort());
            SetClientSetup(setup.Install.has_value(), false, false, setup.TypeDumpError);
            return true;
        }

        void OnConfigChanged(std::vector<std::string> const& changed) override
        {
            sPatchMgr.ApplyConfig(Config(), changed);
            if (_listener.IsRunning() && std::any_of(changed.begin(), changed.end(), [](std::string const& key) { return PatchListener::IsListenerOption(key); }))
            {
                std::string error;
                if (_listener.Apply(Config(), error))
                    SetListener(_listener.GetBindIp(), _listener.GetPort());
                else
                    LOG_WARN("server.patchserver", "Cannot apply the network settings, so the listener on {}:{} goes on serving: {}", _listener.GetBindIp(), _listener.GetPort(), error);
            }
        }

        void OnStatus(std::vector<std::pair<std::string, std::string>>& fields) override
        {
            fields.emplace_back("sessions", fmt::format("{}", _listener.GetConnectionCount()));
            std::shared_ptr<PatchManifest const> const manifest = sPatchMgr.GetManifest();
            fields.emplace_back("manifest", manifest ? manifest->Revision : std::string("none"));
        }

        void OnStop() override
        {
            sPatchMgr.UnregisterReloadTarget();
            _listener.Stop();
        }

    private:
        PatchListener _listener;
    };
}

int main(int argc, char** argv)
{
    PatchServerApp app;
    return app.Run(Ambrose::GetArguments(argc, argv));
}

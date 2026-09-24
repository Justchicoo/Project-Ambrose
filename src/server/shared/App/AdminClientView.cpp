/*
 * Project Ambrose by Imjustchico
 * Answers from what the setup decided at start, plus the type dump's own header read again so the answer describes the file on disk rather than what was believed about it when the server started. A missing install or dump is answered as missing with the reason the setup gave, because an operator looking at this page is usually there because something is not where it should be.
 */

#include "AdminClientView.h"
#include "AdminRouter.h"
#include "ConfigMgr.h"
#include "MessageRegistry.h"
#include "TypeDumpCache.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <utility>

std::string AdminClientView::ClientJson(ClientSetupResult const& setup)
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["pinned_revision"] = std::string(ClientInstall::PinnedRevision);

    nlohmann::json install;
    install["found"] = setup.Install.has_value();
    if (setup.Install)
    {
        install["root"] = ConfigMgr::PathToUtf8(setup.Install->Root);
        install["revision"] = setup.Install->Revision;
        install["pinned"] = setup.Install->IsPinned();
        install["has_program"] = setup.Install->HasProgram;
        install["described"] = setup.Install->Describe();
    }
    body["install"] = std::move(install);

    nlohmann::json dump;
    dump["found"] = setup.TypeDump.has_value();
    dump["built_now"] = setup.TypeDumpBuilt;
    dump["error"] = setup.TypeDumpError;
    if (setup.TypeDump)
    {
        dump["path"] = ConfigMgr::PathToUtf8(*setup.TypeDump);
        std::optional<TypeDumpHeader> const header = TypeDumpCache::ReadHeader(*setup.TypeDump);
        dump["readable"] = header.has_value();
        if (header)
        {
            dump["revision"] = header->Revision;
            dump["executable_sha256"] = header->ExecutableSha256;
            dump["extractor"] = header->Extractor;
            dump["matches_install"] = setup.Install && !header->Revision.empty() && header->Revision == setup.Install->Revision;
        }
    }
    else
    {
        dump["readable"] = false;
    }
    body["type_dump"] = std::move(dump);

    nlohmann::json messages;
    messages["loaded"] = sMessageRegistry.IsLoaded();
    messages["counted"] = sMessageRegistry.GetMessageCount();
    body["messages"] = std::move(messages);

    nlohmann::json saved = nlohmann::json::array();
    for (auto const& [key, value] : setup.Saved)
        saved.push_back(nlohmann::json{ { "key", key }, { "value", value } });
    body["saved"] = std::move(saved);
    body["saved_to"] = setup.SavedTo.empty() ? std::string() : ConfigMgr::PathToUtf8(setup.SavedTo);
    return body.dump();
}

void AdminClientView::Register(AdminRouter& router, Source source)
{
    router.AddGuarded("GET", "/api/client", "client.read", [source = std::move(source)](AdminRequest const&)
    {
        return AdminResponse::Json(200, ClientJson(source()));
    });
}

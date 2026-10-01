/*
 * Project Ambrose by Imjustchico
 * What client data this app is running on, for GET /api/client: the install it found and the revision that install is, whether the app is watching that install for an update and what its last update did, the type dump in use with the revision, executable hash and extractor that made it, and how many message definitions were loaded from the client. It answers with facts about the install rather than anything out of it: no path is served as a link, no file's bytes are ever in a response, and the install root is named only so an operator knows which folder is in use. Rebuilding and switching installs belong to 17.20.
 */

#ifndef AMBROSE_ADMINCLIENTVIEW_H
#define AMBROSE_ADMINCLIENTVIEW_H

#include "ClientSetup.h"
#include "ClientRevisionWatch.h"

#include <functional>
#include <string>

class AdminRouter;

class AdminClientView
{
public:
    static constexpr int SchemaVersion = 1;

    AdminClientView() = delete;

    struct Answer
    {
        ClientSetupResult Setup;
        ClientRevisionStatus Revision;
    };

    static std::string ClientJson(Answer const& answer);
    using Source = std::function<Answer()>;

    static void Register(AdminRouter& router, Source source);
};

#endif

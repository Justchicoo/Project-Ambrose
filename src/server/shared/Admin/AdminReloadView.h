/*
 * Project Ambrose by Imjustchico
 * What every app answers about reloading on GET /api/reload and POST /api/reload/<target>: which stores can be rebuilt without a restart, which generation each is serving, how the last attempt went and every error it found, so the panel can show an operator what is live and put it back in place from the same page rather than from a terminal.
 */

#ifndef AMBROSE_ADMINRELOADVIEW_H
#define AMBROSE_ADMINRELOADVIEW_H

#include <string>

class AdminRouter;

class AdminReloadView
{
public:
    static constexpr int SchemaVersion = 1;

    AdminReloadView() = delete;

    static std::string TargetsJson();
    static void Register(AdminRouter& router);
};

#endif

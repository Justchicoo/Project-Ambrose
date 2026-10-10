/*
 * Project Ambrose by Imjustchico
 * Makes an NpcScript a service provider, catching what the script throws so one broken script leaves its NPC with the others' options rather than none.
 */

#include "NpcServiceProvider.h"
#include "Log.h"
#include "ScriptMgr.h"

#include <exception>

std::string_view ScriptNpcProvider::GetName() const
{
    return _script.GetName();
}

std::vector<NpcServiceOption> ScriptNpcProvider::GetServiceOptions(NpcServiceTarget const& target) const
{
    std::vector<NpcServiceOption> options;
    try
    {
        for (NpcServiceChoice& choice : _script.GetServiceOptions(target.Wizard, target.Npc))
            options.push_back({ std::move(choice.ServiceName), std::move(choice.IconKey), std::move(choice.DisplayKey) });
    }
    catch (std::exception const& failure)
    {
        LOG_ERROR("server.scripts", "The script {} threw from GetServiceOptions: {}", _script.GetName(), failure.what());
        options.clear();
    }
    return options;
}

void ScriptNpcProvider::OnServiceInteraction(NpcServiceTarget const& target, uint32 index)
{
    try
    {
        _script.OnServiceSelect(target.Wizard, target.Npc, index);
    }
    catch (std::exception const& failure)
    {
        LOG_ERROR("server.scripts", "The script {} threw from OnServiceSelect: {}", _script.GetName(), failure.what());
    }
}

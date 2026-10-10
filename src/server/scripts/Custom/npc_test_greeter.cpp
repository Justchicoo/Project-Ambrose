/*
 * Project Ambrose by Imjustchico
 * The sample NpcScript: WC-RAV-NPC06, template 38232 in Ravenwood, offers one greeting, and a wizard who picks it is logged with the index it picked, which is all a script needs to hang a service off an NPC. It offers nothing unless Npc.TestGreeter is on, as in play an NPC shows a prompt only for a real quest or shop.
 */

#include "Log.h"
#include "ScriptMgr.h"
#include "Settings.h"

namespace
{
    constexpr uint32 RavenwoodNpc06 = 38232;

    class NpcTestGreeter : public NpcScript
    {
    public:
        NpcTestGreeter() : NpcScript("npc_test_greeter", RavenwoodNpc06) {}

        std::vector<NpcServiceChoice> GetServiceOptions(uint64 wizard, uint64 npc) const override
        {
            (void)wizard;
            (void)npc;
            if (!sSettings.Get<bool>("Npc.TestGreeter"))
                return {};
            return { { "Greeting", "", "GUI_NPCInteractText" } };
        }

        void OnServiceSelect(uint64 wizard, uint64 npc, uint32 index) override
        {
            LOG_INFO("server.scripts", "npc_test_greeter: wizard {} greeted NPC {} with option {}", wizard, npc, index);
        }
    };
}

void AddSC_npc_test_greeter()
{
    new NpcTestGreeter();
}

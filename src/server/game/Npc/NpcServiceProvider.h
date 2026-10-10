/*
 * Project Ambrose by Imjustchico
 * What an NPC offers a wizard standing near it: each provider names the options it has for that wizard and that NPC, hears which of its own options the wizard picked, counted from 0 within the provider, and may set the NPC's name, text and icon keys, its interaction radius and how early its options come in the menu; ScriptNpcProvider makes an NpcScript one, and a script that throws is reported by name and offers nothing.
 */

#ifndef AMBROSE_NPCSERVICEPROVIDER_H
#define AMBROSE_NPCSERVICEPROVIDER_H

#include "Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class NpcScript;

struct NpcServiceOption
{
    std::string ServiceName;
    std::string IconKey;
    std::string DisplayKey;
};

struct NpcServiceTarget
{
    uint64 Wizard = 0;
    uint64 Npc = 0;
    uint32 TemplateId = 0;
};

class NpcServiceProvider
{
public:
    virtual ~NpcServiceProvider() = default;

    virtual std::string_view GetName() const = 0;
    virtual std::vector<NpcServiceOption> GetServiceOptions(NpcServiceTarget const& target) const = 0;
    virtual void OnServiceInteraction(NpcServiceTarget const& target, uint32 index) = 0;

    virtual int32 GetPriority() const { return 0; }
    virtual std::optional<float> GetInteractionRadius() const { return std::nullopt; }
    virtual std::optional<std::string> GetNameKey() const { return std::nullopt; }
    virtual std::optional<std::string> GetTextKey() const { return std::nullopt; }
    virtual std::optional<std::string> GetIcon() const { return std::nullopt; }
};

class ScriptNpcProvider : public NpcServiceProvider
{
public:
    explicit ScriptNpcProvider(NpcScript& script) : _script(script) { }

    std::string_view GetName() const override;
    std::vector<NpcServiceOption> GetServiceOptions(NpcServiceTarget const& target) const override;
    void OnServiceInteraction(NpcServiceTarget const& target, uint32 index) override;

private:
    NpcScript& _script;
};

#endif

/*
 * Project Ambrose by Imjustchico
 * Turns the WizardCharacterCreationInfo a client sends into a wizard this server is willing to store, or names why it will not. Nothing here reaches a database or a setting: the rows, the name tables, the limits and the rules are handed in, so the same judgement runs in a test and in the login server and neither can drift from the other. Every appearance value is judged against the width the type dump gives its property rather than a number written here, and gender and race against the options the dump names, so a client revision that widens a field is followed by reloading the dump rather than by editing this file. What the client is told is only that it worked or that it did not, because the error codes the retail server uses beyond those two are not known; the reason is written to the log where an operator can read it.
 */

#ifndef AMBROSE_CREATIONINFOREADER_H
#define AMBROSE_CREATIONINFOREADER_H

#include "CharacterCreateStore.h"
#include "CharacterSummary.h"

#include <string>
#include <string_view>

class CharacterNameSet;
class PropertyObject;

struct CreationLimits
{
    uint32 ExistingCharacters = 0;
    uint32 MaxPerAccount = 6;
    uint32 PurchasedSlots = 0;
};

struct CreationRules
{
    bool AllowCustomName = false;
    std::string Locale;
};

struct CreationOutcome
{
    bool Ok = false;
    std::string Refusal;
    CharacterSummary Character;
};

class CreationInfoReader
{
public:
    static constexpr std::string_view HumanRace = "Human";
    static constexpr std::string_view NeutralGender = "Neutral";
    static constexpr std::size_t MaxCustomNameCharacters = 30;

    CreationInfoReader() = delete;

    static CreationOutcome Read(PropertyObject const& info, CharacterCreateSet const& rows, CharacterNameSet const& names,
        CreationLimits const& limits, CreationRules const& rules);
};

#endif

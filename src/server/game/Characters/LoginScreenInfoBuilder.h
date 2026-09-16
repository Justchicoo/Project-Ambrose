/*
 * Project Ambrose by Imjustchico
 * Turns a stored wizard into the WizardCharacterCreationInfo the character select screen shows, with its WizardCharacterBehavior appearance and an equipment list, and encodes it the way MSG_CHARACTERINFO carries it.
 */

#ifndef AMBROSE_LOGINSCREENINFOBUILDER_H
#define AMBROSE_LOGINSCREENINFOBUILDER_H

#include "CharacterSummary.h"
#include "ObjectSerializer.h"

#include <string>

class LoginScreenInfoBuilder
{
public:
    static constexpr int32 TemplateId = 1;

    LoginScreenInfoBuilder() = delete;

    static PropertyObjectPtr Build(TypeCatalogPtr const& catalog, CharacterSummary const& character, std::string& problem);
    static EncodeResult Encode(TypeCatalogPtr const& catalog, CharacterSummary const& character);
};

#endif

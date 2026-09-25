/*
 * Project Ambrose by Imjustchico
 * Writes a stored wizard's look into a WizardCharacterBehavior, every appearance field the behavior carries, for the character select screen and the wizard standing in the world alike, naming the first property that refuses its value.
 */

#ifndef AMBROSE_AVATARAPPEARANCE_H
#define AMBROSE_AVATARAPPEARANCE_H

#include "CharacterSummary.h"
#include "PropertyObject.h"

#include <string>

namespace AvatarAppearance
{
    bool Write(PropertyObject& behavior, CharacterAppearance const& look, std::string& problem);
}

#endif

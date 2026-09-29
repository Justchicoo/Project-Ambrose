/*
 * Project Ambrose by Imjustchico
 * Who hears what a wizard said or played in a tick: every other open wizard in the speaker's instance within the say range of it, anywhere in the instance when the range is 0, and the speaker itself only when what it did is something its own client does not show, such as an emote it did not ask to be left out of; a speaker in no instance is heard by nobody.
 */

#ifndef AMBROSE_SPEECHRELAY_H
#define AMBROSE_SPEECHRELAY_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <vector>

struct SpeechListener
{
    bool Open = false;
    std::optional<uint32> MapId;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
};

std::vector<std::size_t> PlanHearers(std::vector<SpeechListener> const& listeners, std::size_t speaker, bool speakerSees, float range);

#endif

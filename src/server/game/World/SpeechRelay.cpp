/*
 * Project Ambrose by Imjustchico
 * Walks the listeners in order, so hearers come back in the order the world holds its sessions, and measures each from the speaker with the chat rules' range test.
 */

#include "SpeechRelay.h"
#include "ChatMgr.h"

std::vector<std::size_t> PlanHearers(std::vector<SpeechListener> const& listeners, std::size_t speaker, bool speakerSees, float range)
{
    std::vector<std::size_t> hearers;
    if (speaker >= listeners.size() || !listeners[speaker].MapId)
        return hearers;
    SpeechListener const& from = listeners[speaker];
    for (std::size_t index = 0; index < listeners.size(); ++index)
    {
        SpeechListener const& listener = listeners[index];
        if (!listener.Open || listener.MapId != from.MapId)
            continue;
        if (index == speaker)
        {
            if (speakerSees)
                hearers.push_back(index);
            continue;
        }
        if (ChatMgr::CanHear(listener.X - from.X, listener.Y - from.Y, listener.Z - from.Z, range))
            hearers.push_back(index);
    }
    return hearers;
}

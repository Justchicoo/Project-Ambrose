/*
 * Project Ambrose by Imjustchico
 * The only thing the Studio's window may say to the Studio and what it hears back: a message names a path under /studio and its values, and the answer is the content's JSON with the call's id, its status and whether it succeeded, so the page and a test read one shape. Nothing is decided here: a message is read, handed to the content and written down.
 */

#ifndef AMBROSE_STUDIOCHANNEL_H
#define AMBROSE_STUDIOCHANNEL_H

#include "StudioContent.h"

#include <string>

class StudioChannel
{
public:
    explicit StudioChannel(StudioContent const& content) : _content(content) {}

    std::string Answer(std::string const& message) const;

private:
    StudioContent const& _content;
};

#endif

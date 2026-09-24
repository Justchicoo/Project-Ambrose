/*
 * Project Ambrose by Imjustchico
 * Writes and reads the XML form of the LatestFileList manifest with the patch table list and package records the client expects.
 */

#ifndef AMBROSE_LATESTFILELISTXML_H
#define AMBROSE_LATESTFILELISTXML_H

#include "LatestFileList.h"

#include <string>
#include <string_view>

class LatestFileListXml
{
public:
    static std::string Write(LatestFileList const& list);
    static LatestFileList Read(std::string_view xml);
};

#endif

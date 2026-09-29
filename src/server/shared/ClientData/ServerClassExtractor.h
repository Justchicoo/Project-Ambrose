/*
 * Project Ambrose by Imjustchico
 * Finds the classes an install's data holds and its type dump does not describe, in its BINd files and headerless objects and in its plain-XML object files alike, and keeps only those every object of which then reads cleanly: each round opens and sweeps the archives one at a time, since an install holds more than a process may keep open, with the classes kept so far, proposes one for every class still unknown, a class the XML files hold only when no BINd file does, tries a property that did not decode as a list before refusing its class, and rebuilds the registry's supplement, until a round changes nothing, which is the sweep that proves the classes kept.
 */

#ifndef AMBROSE_SERVERCLASSEXTRACTOR_H
#define AMBROSE_SERVERCLASSEXTRACTOR_H

#include "BindSweep.h"
#include "ServerClassProposer.h"
#include "XmlSweep.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct ServerClassExtraction
{
    std::vector<ServerClassProposal> Classes;
    std::vector<ServerClassRefusal> Refused;
    std::size_t UnknownBefore = 0;
    std::size_t UnknownAfter = 0;
    uint64 ObjectsBefore = 0;
    uint64 ObjectsAfter = 0;
    uint64 FailuresBefore = 0;
    uint64 FailuresAfter = 0;
    uint64 XmlDocuments = 0;
    std::size_t XmlUnknownBefore = 0;
    std::size_t XmlUnknownAfter = 0;
    uint64 XmlObjectsBefore = 0;
    uint64 XmlObjectsAfter = 0;
    uint64 XmlFailuresBefore = 0;
    uint64 XmlFailuresAfter = 0;
    uint32 Rounds = 0;
    std::vector<std::string> Errors;

    bool Ok() const noexcept { return Errors.empty(); }
};

struct ServerClassExtractorOptions
{
    unsigned Threads = 0;
    uint32 MaxRounds = 8;
    std::function<void(uint32 round, std::size_t kept)> Progress;
};

class ServerClassExtractor
{
public:
    ServerClassExtractor() = delete;

    static ServerClassExtraction Run(std::vector<std::filesystem::path> const& archives, TypeRegistry& registry, TypeDumpLoader::RawDump const& existing, PropertyOracle const& oracle,
        ServerClassNames const& names, ServerClassExtractorOptions const& options = {});
};

#endif

/*
 * Project Ambrose by Imjustchico
 * Sweeps every archive whole once, its BINd files and its XML object files, and afterwards only the files an unknown class appeared in, opening only the archives that hold one, since nothing a proposal changes can reach another, merging each round's sweeps, then judges the classes kept so far by the issues their own properties raise in either kind of file: a property read with a trial container that does not decode is tried as a list the next round, any other is its class's refusal, and when files that decoded before stop decoding, the classes added the round before are refused together, since nothing narrower can be said of them; a class the proposer turns down is asked again each round, because a class kept meanwhile may name the list it sits in, and the last round's refusals are the ones reported; a class any BINd file has held is never proposed from the XML files, whose values prove no type, and an XML document refused after a round is blamed on the classes added the round before as a BINd file is.
 */

#include "ServerClassExtractor.h"
#include "KiwadArchive.h"
#include "ObjectSerializer.h"
#include "StringHash.h"

#include <fmt/format.h>

#include <iterator>
#include <map>
#include <memory>
#include <set>

namespace
{
    struct Swept
    {
        std::map<uint32, BindSweepUnknownClass> Unknown;
        std::map<uint32, std::set<uint32>> Properties;
        std::vector<BindSweepIssue> Issues;
        std::set<std::string> Failed;
        std::vector<std::vector<std::string>> UnknownFiles;
        uint64 Objects = 0;
        XmlSweepReport Xml;
        std::vector<std::vector<std::string>> XmlUnknownFiles;
        std::set<std::string> XmlFailed;
    };

    Swept SweepAll(std::vector<std::filesystem::path> const& archives, TypeCatalogPtr const& catalog, unsigned threads, std::vector<std::vector<std::string>> const* only,
        std::vector<std::vector<std::string>> const* xmlOnly, std::string& error)
    {
        Swept swept;
        for (std::size_t at = 0; at < archives.size(); ++at)
        {
            if (only && (*only)[at].empty() && (*xmlOnly)[at].empty())
            {
                swept.UnknownFiles.emplace_back();
                swept.XmlUnknownFiles.emplace_back();
                continue;
            }
            std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(archives[at], error);
            if (!archive)
                return swept;
            XmlSweepReport xml = !only || !(*xmlOnly)[at].empty() ? XmlSweep::Run(*archive, catalog, only ? &(*xmlOnly)[at] : nullptr) : XmlSweepReport();
            swept.XmlUnknownFiles.push_back(xml.UnknownFiles);
            for (XmlSweepFailure const& failure : xml.Failures)
                swept.XmlFailed.insert(archives[at].filename().string() + ":" + failure.File);
            XmlSweep::Merge(swept.Xml, std::move(xml));
            if (only && (*only)[at].empty())
            {
                swept.UnknownFiles.emplace_back();
                continue;
            }
            BindSweepReport report = BindSweep::Run(*archive, catalog, threads, BindFile::GetDefaultLimits(), only ? &(*only)[at] : nullptr);
            swept.UnknownFiles.push_back(std::move(report.UnknownFiles));
            for (BindSweepUnknownClass& unknown : report.UnknownClasses)
            {
                auto [found, added] = swept.Unknown.try_emplace(unknown.Hash, unknown);
                if (!added)
                {
                    found->second.Count += unknown.Count;
                    found->second.Files += unknown.Files;
                }
                swept.Objects += unknown.Count;
            }
            for (BindSweepClassProperty const& property : report.ClassProperties)
                swept.Properties[property.Owner].insert(property.Hash);
            for (BindSweepIssue& issue : report.Issues)
                swept.Issues.push_back(std::move(issue));
            for (BindSweepFailure const& failure : report.Failures)
                swept.Failed.insert(failure.File);
        }
        return swept;
    }

    TypeDumpLoader::RawProperty const* FindProperty(ServerClassProposal const& proposal, uint32 hash)
    {
        for (TypeDumpLoader::RawProperty const& property : proposal.Class.Properties)
            if (property.Hash && *property.Hash == hash)
                return &property;
        return nullptr;
    }
}

ServerClassExtraction ServerClassExtractor::Run(std::vector<std::filesystem::path> const& archives, TypeRegistry& registry, TypeDumpLoader::RawDump const& existing, PropertyOracle const& oracle,
    ServerClassNames const& names, ServerClassExtractorOptions const& options)
{
    ServerClassExtraction result;
    std::map<uint32, ServerClassProposal> kept;
    std::map<uint32, ServerClassRefusal> refused;
    ServerClassContainers containers;
    std::set<uint32> lastAdded;
    std::set<std::string> failedBefore;
    std::set<std::string> xmlFailedBefore;
    std::set<uint32> inBinary;
    std::vector<std::vector<std::string>> unknownFiles;
    std::vector<std::vector<std::string>> xmlUnknownFiles;
    for (uint32 round = 0;; ++round)
    {
        if (round > options.MaxRounds)
        {
            result.Errors.push_back(fmt::format("the classes did not settle within {} rounds", options.MaxRounds));
            break;
        }
        result.Rounds = round + 1;
        std::string opening;
        Swept const swept = SweepAll(archives, registry.GetCatalog(), options.Threads, round == 0 ? nullptr : &unknownFiles, round == 0 ? nullptr : &xmlUnknownFiles, opening);
        if (!opening.empty())
        {
            result.Errors.push_back(std::move(opening));
            break;
        }
        if (round == 0)
        {
            result.UnknownBefore = swept.Unknown.size();
            result.ObjectsBefore = swept.Objects;
            result.FailuresBefore = swept.Failed.size();
            failedBefore = swept.Failed;
            unknownFiles = swept.UnknownFiles;
            result.XmlDocuments = swept.Xml.Documents;
            result.XmlUnknownBefore = swept.Xml.UnknownClasses.size();
            for (XmlSweepClass const& seen : swept.Xml.UnknownClasses)
                result.XmlObjectsBefore += seen.Count;
            result.XmlFailuresBefore = swept.XmlFailed.size();
            xmlFailedBefore = swept.XmlFailed;
            xmlUnknownFiles = swept.XmlUnknownFiles;
        }
        for (auto const& [hash, unknown] : swept.Unknown)
            inBinary.insert(hash);

        bool changed = false;
        auto const drop = [&](uint32 hash, std::string reason)
        {
            if (kept.erase(hash) > 0)
                changed = true;
            refused[hash] = ServerClassRefusal{ hash, std::move(reason) };
        };
        std::map<uint32, std::vector<BindSweepIssue const*>> byOwner;
        for (BindSweepIssue const& issue : swept.Issues)
            if (kept.contains(issue.Owner))
                byOwner[issue.Owner].push_back(&issue);
        for (BindSweepIssue const& issue : swept.Xml.Issues)
            if (kept.contains(issue.Owner))
                byOwner[issue.Owner].push_back(&issue);
        for (auto const& [owner, issues] : byOwner)
        {
            ServerClassProposal const& proposal = kept.at(owner);
            bool retry = false;
            std::string failure;
            for (BindSweepIssue const* issue : issues)
            {
                std::pair<uint32, uint32> const key{ owner, issue->Hash };
                if (FindProperty(proposal, issue->Hash) && proposal.Trials.contains(issue->Hash) && !containers.contains(key))
                {
                    containers[key] = std::string(ServerClassProposer::ListContainer);
                    retry = true;
                }
                else if (failure.empty())
                    failure = fmt::format("its objects do not decode: {} at {}, {}", ObjectSerializer::GetIssueName(issue->Kind), issue->FirstPath, issue->FirstDetail);
            }
            if (!failure.empty())
                drop(owner, std::move(failure));
            else if (retry)
            {
                kept.erase(owner);
                changed = true;
            }
        }
        std::vector<std::string> newlyFailed;
        for (std::string const& file : swept.Failed)
            if (!failedBefore.contains(file))
                newlyFailed.push_back(file);
        for (std::string const& file : swept.XmlFailed)
            if (!xmlFailedBefore.contains(file))
                newlyFailed.push_back(file);
        if (!newlyFailed.empty())
        {
            for (uint32 const hash : lastAdded)
                drop(hash, fmt::format("with the classes added beside it, {} file(s) that decoded before stop decoding, the first {}", newlyFailed.size(), newlyFailed.front()));
            if (lastAdded.empty())
            {
                result.Errors.push_back(fmt::format("{} file(s) stopped decoding and no class added last can be blamed, the first {}", newlyFailed.size(), newlyFailed.front()));
                break;
            }
        }

        std::vector<ServerClassObservation> observed;
        for (auto const& [hash, unknown] : swept.Unknown)
        {
            if (refused.contains(hash) || kept.contains(hash))
                continue;
            ServerClassObservation seen{ hash, unknown.Count, unknown.Files, unknown.FirstFile, unknown.FirstPath, {} };
            if (auto const properties = swept.Properties.find(hash); properties != swept.Properties.end())
                seen.Properties.assign(properties->second.begin(), properties->second.end());
            observed.push_back(std::move(seen));
        }
        ServerClassProposals proposals = ServerClassProposer::Propose(*registry.GetCatalog(), oracle, names, observed, containers);
        std::vector<XmlSweepClass> xmlObserved;
        for (XmlSweepClass const& seen : swept.Xml.UnknownClasses)
        {
            uint32 const hash = StringHash::KiStringHash(seen.Name);
            if (!refused.contains(hash) && !kept.contains(hash))
                xmlObserved.push_back(seen);
        }
        ServerClassProposals xmlProposals = ServerClassProposer::ProposeXml(*registry.GetCatalog(), xmlObserved, inBinary);
        std::move(xmlProposals.Classes.begin(), xmlProposals.Classes.end(), std::back_inserter(proposals.Classes));
        std::move(xmlProposals.Refused.begin(), xmlProposals.Refused.end(), std::back_inserter(proposals.Refused));
        lastAdded.clear();
        for (ServerClassProposal& proposal : proposals.Classes)
        {
            uint32 const hash = static_cast<uint32>(*proposal.Class.Hash);
            lastAdded.insert(hash);
            kept.emplace(hash, std::move(proposal));
            changed = true;
        }

        if (!changed)
        {
            result.UnknownAfter = swept.Unknown.size();
            result.ObjectsAfter = swept.Objects;
            std::set<std::string> resweptFiles;
            for (std::vector<std::string> const& files : unknownFiles)
                resweptFiles.insert(files.begin(), files.end());
            std::size_t outside = 0;
            for (std::string const& file : failedBefore)
                if (!resweptFiles.contains(file))
                    ++outside;
            result.FailuresAfter = outside + swept.Failed.size();
            result.XmlUnknownAfter = swept.Xml.UnknownClasses.size();
            for (XmlSweepClass const& seen : swept.Xml.UnknownClasses)
                result.XmlObjectsAfter += seen.Count;
            std::set<std::string> xmlResweptFiles;
            for (std::size_t at = 0; at < xmlUnknownFiles.size(); ++at)
                for (std::string const& file : xmlUnknownFiles[at])
                    xmlResweptFiles.insert(archives[at].filename().string() + ":" + file);
            std::size_t xmlOutside = 0;
            for (std::string const& file : xmlFailedBefore)
                if (!xmlResweptFiles.contains(file))
                    ++xmlOutside;
            result.XmlFailuresAfter = xmlOutside + swept.XmlFailed.size();
            for (ServerClassRefusal& refusal : proposals.Refused)
                refused.try_emplace(refusal.Hash, std::move(refusal));
            break;
        }

        TypeDumpLoader::RawDump supplement = existing;
        supplement.Version = TypeDumpLoader::SupportedVersion;
        supplement.HasClasses = true;
        for (auto const& [hash, proposal] : kept)
            supplement.Classes.push_back(proposal.Class);
        std::vector<std::string> errors;
        if (!registry.SetSupplement(std::move(supplement), "the classes this install's data holds", errors))
        {
            result.Errors.insert(result.Errors.end(), errors.begin(), errors.end());
            break;
        }
        if (options.Progress)
            options.Progress(round + 1, kept.size());
    }
    for (auto& [hash, proposal] : kept)
        result.Classes.push_back(std::move(proposal));
    for (auto& [hash, refusal] : refused)
        result.Refused.push_back(std::move(refusal));
    return result;
}

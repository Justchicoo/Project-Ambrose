/*
 * Project Ambrose by Imjustchico
 * Sweeps every plain-XML ObjectProperty document in a KIWAD archive, or only the entries it is given by name, through the XML object reader and reports how many it reads, every document it refuses, every other issue grouped by kind, hash and the class that owns it with how often and in how many files it appears and where it first does, and every class the catalog does not describe with what its objects hold: each element by name in the order first seen, whether an object repeats it or writes it with a key, the classes it holds and the distinct text it holds, and the classes and properties that hold the class; it names every file an unknown class appears in, so a later sweep can be limited to them, and Merge joins the reports of several archives.
 */

#ifndef AMBROSE_XMLSWEEP_H
#define AMBROSE_XMLSWEEP_H

#include "BindSweep.h"
#include "XmlObjectReader.h"

#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class KiwadArchive;

struct XmlSweepProperty
{
    std::string Name;
    uint64 Count = 0;
    uint64 Empty = 0;
    bool Repeats = false;
    bool Keyed = false;
    bool Mixed = false;
    bool Overflow = false;
    std::set<std::string> Held;
    std::set<std::string> Values;
};

struct XmlSweepClass
{
    std::string Name;
    uint64 Count = 0;
    uint64 Files = 0;
    std::string FirstFile;
    std::string FirstPath;
    bool AtRoot = false;
    std::set<std::pair<std::string, std::string>> Holders;
    std::vector<XmlSweepProperty> Properties;
};

struct XmlSweepFailure
{
    std::string File;
    XmlReadStatus Status = XmlReadStatus::Ok;
    std::string Detail;
};

struct XmlSweepReport
{
    uint64 Entries = 0;
    uint64 Documents = 0;
    uint64 Read = 0;
    uint64 ReadErrors = 0;
    std::vector<XmlSweepFailure> Failures;
    std::vector<XmlSweepClass> UnknownClasses;
    std::vector<BindSweepIssue> Issues;
    std::vector<std::string> UnknownFiles;
};

class XmlSweep
{
public:
    static constexpr std::size_t MaxValues = 65536;

    XmlSweep() = delete;

    static bool IsObjectsDocument(std::string_view text);
    static XmlSweepReport Run(KiwadArchive const& archive, TypeCatalogPtr const& catalog, std::vector<std::string> const* only = nullptr);
    static void Merge(XmlSweepReport& into, XmlSweepReport&& from);
};

#endif

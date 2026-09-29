/*
 * Project Ambrose by Imjustchico
 * Takes an entry as an Objects document when its name ends in .xml, it is not a BINd file and its first element, after a byte order mark, blank space, declarations and comments, is Objects; reads each through the XML object reader, and only where it reports a class the catalog does not describe parses the document again to walk every Class element, recording each unknown class's elements, the classes they hold and their text as the reader would read it, an element holding only blank space counted as empty, and for a class inside another the class and property holding it; values beyond the limit are not kept and the property is marked, and merging adds counts, joins sets and keeps the first file and path.
 */

#include "XmlSweep.h"
#include "BindFile.h"
#include "KiwadArchive.h"
#include "StringUtil.h"

#include <pugixml.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <map>
#include <new>
#include <tuple>

namespace
{
    constexpr std::string_view Utf8Bom = "\xEF\xBB\xBF";
    constexpr std::string_view ObjectsOpen = "<Objects";
    constexpr std::string_view ClassElement = "Class";
    constexpr char const* NameAttribute = "Name";
    constexpr char const* KeyAttribute = "key";

    using IssueKey = std::tuple<DecodeIssueKind, uint32, uint32>;

    bool IsXmlName(std::string_view name)
    {
        return name.size() >= 4 && Ambrose::EqualsIgnoreCase(name.substr(name.size() - 4), ".xml");
    }

    std::string_view SkipSpace(std::string_view text)
    {
        std::size_t at = 0;
        while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at])))
            ++at;
        return text.substr(at);
    }

    std::string Text(pugi::xml_node element)
    {
        std::string text;
        for (pugi::xml_node child = element.first_child(); child; child = child.next_sibling())
            if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
                text += child.value();
        return text;
    }

    void AddValue(XmlSweepProperty& property, std::string value)
    {
        if (property.Values.size() < XmlSweep::MaxValues || property.Values.contains(value))
            property.Values.insert(std::move(value));
        else
            property.Overflow = true;
    }

    XmlSweepProperty& PropertyNamed(XmlSweepClass& seen, std::string_view name)
    {
        auto const found = std::find_if(seen.Properties.begin(), seen.Properties.end(), [name](XmlSweepProperty const& property) { return property.Name == name; });
        if (found != seen.Properties.end())
            return *found;
        seen.Properties.emplace_back().Name = std::string(name);
        return seen.Properties.back();
    }

    class Observer
    {
    public:
        Observer(TypeCatalog const& catalog, std::string const& file, std::map<std::string, XmlSweepClass>& classes, std::vector<std::string>& order)
            : _catalog(catalog), _file(file), _classes(classes), _order(order)
        {
        }

        void Walk(pugi::xml_node node, std::string const& path, std::string const* holderClass, std::string const* holderProperty)
        {
            std::string const name = node.attribute(NameAttribute).value();
            std::string const objectPath = path.empty() ? name : path;
            XmlSweepClass* const seen = _catalog.FindClass(name) ? nullptr : &Record(name, objectPath, holderClass, holderProperty);

            std::map<std::string, uint64> counts;
            for (pugi::xml_node element = node.first_child(); element; element = element.next_sibling())
                if (element.type() == pugi::node_element)
                    ++counts[element.name()];
            std::map<std::string, std::size_t> positions;
            for (pugi::xml_node element = node.first_child(); element; element = element.next_sibling())
            {
                if (element.type() != pugi::node_element)
                    continue;
                std::string const propertyName = element.name();
                bool const repeated = counts[propertyName] > 1;
                std::string const propertyPath = repeated ? fmt::format("{}.{}[{}]", objectPath, propertyName, positions[propertyName]++) : fmt::format("{}.{}", objectPath, propertyName);
                std::vector<pugi::xml_node> nested;
                bool other = false;
                for (pugi::xml_node child = element.first_child(); child; child = child.next_sibling())
                {
                    if (child.type() != pugi::node_element)
                        continue;
                    if (std::string_view(child.name()) == ClassElement)
                        nested.push_back(child);
                    else
                        other = true;
                }
                if (seen)
                    Note(PropertyNamed(*seen, propertyName), element, nested, other, repeated);
                for (pugi::xml_node const child : nested)
                    Walk(child, propertyPath, &name, &propertyName);
            }
        }

    private:
        XmlSweepClass& Record(std::string const& name, std::string const& path, std::string const* holderClass, std::string const* holderProperty)
        {
            auto [found, added] = _classes.try_emplace(name);
            XmlSweepClass& seen = found->second;
            if (added)
            {
                seen.Name = name;
                seen.FirstFile = _file;
                seen.FirstPath = path;
                _order.push_back(name);
            }
            ++seen.Count;
            if (_inFile.insert(name).second)
                ++seen.Files;
            if (holderClass && holderProperty)
                seen.Holders.emplace(*holderClass, *holderProperty);
            else
                seen.AtRoot = true;
            return seen;
        }

        static void Note(XmlSweepProperty& property, pugi::xml_node element, std::vector<pugi::xml_node> const& nested, bool other, bool repeated)
        {
            ++property.Count;
            if (repeated)
                property.Repeats = true;
            if (element.attribute(KeyAttribute))
                property.Keyed = true;
            std::string text = Text(element);
            if (other || nested.size() > 1 || (!nested.empty() && !Ambrose::Trim(text).empty()))
                property.Mixed = true;
            for (pugi::xml_node const child : nested)
                property.Held.insert(child.attribute(NameAttribute).value());
            if (!nested.empty() || other)
                return;
            if (Ambrose::Trim(text).empty())
                ++property.Empty;
            else
                AddValue(property, std::move(text));
        }

        TypeCatalog const& _catalog;
        std::string const& _file;
        std::map<std::string, XmlSweepClass>& _classes;
        std::vector<std::string>& _order;
        std::set<std::string> _inFile;
    };

    void Observe(TypeCatalog const& catalog, std::string const& file, std::string_view text, std::map<std::string, XmlSweepClass>& classes, std::vector<std::string>& order)
    {
        pugi::xml_document document;
        if (!document.load_buffer(text.data(), text.size(), pugi::parse_default | pugi::parse_ws_pcdata_single, pugi::encoding_utf8))
            return;
        pugi::xml_node const root = document.child("Objects");
        Observer observer(catalog, file, classes, order);
        for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling())
            if (child.type() == pugi::node_element && std::string_view(child.name()) == ClassElement)
                observer.Walk(child, {}, nullptr, nullptr);
    }
}

bool XmlSweep::IsObjectsDocument(std::string_view text)
{
    std::string_view rest = text.starts_with(Utf8Bom) ? text.substr(Utf8Bom.size()) : text;
    for (;;)
    {
        rest = SkipSpace(rest);
        std::string_view close;
        if (rest.starts_with("<?"))
            close = "?>";
        else if (rest.starts_with("<!--"))
            close = "-->";
        else
            break;
        std::size_t const end = rest.find(close);
        if (end == std::string_view::npos)
            return false;
        rest.remove_prefix(end + close.size());
    }
    if (!rest.starts_with(ObjectsOpen) || rest.size() == ObjectsOpen.size())
        return false;
    char const next = rest[ObjectsOpen.size()];
    return next == '>' || next == '/' || std::isspace(static_cast<unsigned char>(next));
}

XmlSweepReport XmlSweep::Run(KiwadArchive const& archive, TypeCatalogPtr const& catalog, std::vector<std::string> const* only)
{
    XmlSweepReport report;
    std::set<std::string> const wanted = only ? std::set<std::string>(only->begin(), only->end()) : std::set<std::string>();
    std::map<std::string, XmlSweepClass> classes;
    std::vector<std::string> order;
    std::map<IssueKey, BindSweepIssue> issues;
    for (KiwadEntry const& entry : archive.GetEntries())
    {
        if (!IsXmlName(entry.Name) || (only && !wanted.contains(entry.Name)))
            continue;
        ++report.Entries;
        try
        {
            KiwadReadResult const read = archive.Read(entry);
            if (!read.Succeeded())
            {
                ++report.ReadErrors;
                continue;
            }
            std::string_view const text(reinterpret_cast<char const*>(read.Data.data()), read.Data.size());
            if (BindFile::IsBind(read.Data) || !IsObjectsDocument(text))
                continue;
            ++report.Documents;
            XmlReadResult const result = XmlObjectReader::Read(catalog, text);
            if (!result.Ok())
            {
                report.Failures.push_back({ entry.Name, result.Status, result.Detail });
                continue;
            }
            ++report.Read;
            bool unknown = false;
            std::set<IssueKey> inFile;
            for (DecodeIssue const& issue : result.Issues)
            {
                if (issue.Kind == DecodeIssueKind::UnknownClass)
                {
                    unknown = true;
                    continue;
                }
                IssueKey const key{ issue.Kind, issue.Hash, issue.Owner };
                BindSweepIssue& grouped = issues[key];
                if (grouped.Count == 0)
                {
                    grouped.Kind = issue.Kind;
                    grouped.Hash = issue.Hash;
                    grouped.Owner = issue.Owner;
                    grouped.FirstFile = entry.Name;
                    grouped.FirstPath = issue.Path;
                    grouped.FirstDetail = issue.Detail;
                }
                ++grouped.Count;
                if (inFile.insert(key).second)
                    ++grouped.Files;
            }
            if (!unknown)
                continue;
            report.UnknownFiles.push_back(entry.Name);
            Observe(*catalog, entry.Name, text, classes, order);
        }
        catch (std::bad_alloc const&)
        {
            report.Failures.push_back({ entry.Name, XmlReadStatus::OutOfMemory, "memory ran out while sweeping it" });
        }
    }
    for (std::string const& name : order)
        report.UnknownClasses.push_back(std::move(classes.at(name)));
    for (auto& [key, issue] : issues)
        report.Issues.push_back(std::move(issue));
    return report;
}

void XmlSweep::Merge(XmlSweepReport& into, XmlSweepReport&& from)
{
    into.Entries += from.Entries;
    into.Documents += from.Documents;
    into.Read += from.Read;
    into.ReadErrors += from.ReadErrors;
    std::move(from.Failures.begin(), from.Failures.end(), std::back_inserter(into.Failures));
    std::move(from.UnknownFiles.begin(), from.UnknownFiles.end(), std::back_inserter(into.UnknownFiles));
    for (BindSweepIssue& issue : from.Issues)
    {
        auto const found = std::find_if(into.Issues.begin(), into.Issues.end(),
            [&issue](BindSweepIssue const& existing) { return existing.Kind == issue.Kind && existing.Hash == issue.Hash && existing.Owner == issue.Owner; });
        if (found == into.Issues.end())
        {
            into.Issues.push_back(std::move(issue));
            continue;
        }
        found->Count += issue.Count;
        found->Files += issue.Files;
    }
    for (XmlSweepClass& seen : from.UnknownClasses)
    {
        auto const found = std::find_if(into.UnknownClasses.begin(), into.UnknownClasses.end(), [&seen](XmlSweepClass const& existing) { return existing.Name == seen.Name; });
        if (found == into.UnknownClasses.end())
        {
            into.UnknownClasses.push_back(std::move(seen));
            continue;
        }
        found->Count += seen.Count;
        found->Files += seen.Files;
        found->AtRoot = found->AtRoot || seen.AtRoot;
        found->Holders.insert(seen.Holders.begin(), seen.Holders.end());
        for (XmlSweepProperty& property : seen.Properties)
        {
            XmlSweepProperty& merged = PropertyNamed(*found, property.Name);
            merged.Count += property.Count;
            merged.Empty += property.Empty;
            merged.Repeats = merged.Repeats || property.Repeats;
            merged.Keyed = merged.Keyed || property.Keyed;
            merged.Mixed = merged.Mixed || property.Mixed;
            merged.Overflow = merged.Overflow || property.Overflow;
            merged.Held.insert(property.Held.begin(), property.Held.end());
            for (std::string const& value : property.Values)
                AddValue(merged, value);
        }
    }
}

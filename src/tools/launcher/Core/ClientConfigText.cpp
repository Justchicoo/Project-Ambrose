/*
 * Project Ambrose by Imjustchico
 * Implements the text edits: a scanner walks the file's tags, skipping comments, declarations and character data, and finds an element by name inside a range together with its content, so a value is replaced in place and nothing else is rewritten; a value already as asked for is left alone, an empty element becomes a pair only when its value has to change, and an inserted line takes the line ending the file uses, the indentation its neighbours have, and the indentation step the file's first indented line shows.
 */

#include "ClientConfigText.h"

#include <fmt/format.h>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace
{
    constexpr std::string_view RecordName = "RECORD";
    constexpr std::string_view TableListName = "_TableList";
    constexpr std::string_view TableNameKey = "Name";
    constexpr std::string_view DefaultIndent = "  ";

    enum class TagKind
    {
        Open,
        Close,
        SelfClosing,
        Other
    };

    struct Tag
    {
        std::size_t Start = 0;
        std::size_t End = 0;
        std::size_t NameStart = 0;
        std::size_t NameEnd = 0;
        TagKind Kind = TagKind::Other;
    };

    struct Element
    {
        std::size_t Start = 0;
        std::size_t NameStart = 0;
        std::size_t NameEnd = 0;
        std::size_t ContentStart = 0;
        std::size_t ContentEnd = 0;
        std::size_t End = 0;
        bool SelfClosing = false;
    };

    bool IsSpace(char character)
    {
        return character == ' ' || character == '\t' || character == '\r' || character == '\n';
    }

    std::size_t PastMark(std::string_view text, std::string_view mark, std::size_t from)
    {
        std::size_t const found = text.find(mark, from);
        return found == std::string_view::npos ? text.size() : found + mark.size();
    }

    std::optional<Tag> NextTag(std::string_view text, std::size_t from)
    {
        std::size_t const open = text.find('<', from);
        if (open == std::string_view::npos)
            return std::nullopt;
        Tag tag;
        tag.Start = open;
        std::string_view const rest = text.substr(open);
        if (rest.starts_with("<!--"))
        {
            tag.End = PastMark(text, "-->", open);
            return tag;
        }
        if (rest.starts_with("<![CDATA["))
        {
            tag.End = PastMark(text, "]]>", open);
            return tag;
        }
        if (rest.starts_with("<?"))
        {
            tag.End = PastMark(text, "?>", open);
            return tag;
        }
        if (rest.starts_with("<!"))
        {
            tag.End = PastMark(text, ">", open);
            return tag;
        }
        bool const closing = rest.starts_with("</");
        tag.NameStart = open + (closing ? 2 : 1);
        tag.NameEnd = tag.NameStart;
        while (tag.NameEnd < text.size() && !IsSpace(text[tag.NameEnd]) && text[tag.NameEnd] != '>' && text[tag.NameEnd] != '/')
            ++tag.NameEnd;
        std::size_t at = tag.NameEnd;
        char quote = 0;
        while (at < text.size())
        {
            char const character = text[at];
            if (quote != 0)
            {
                if (character == quote)
                    quote = 0;
            }
            else if (character == '"' || character == '\'')
                quote = character;
            else if (character == '>')
                break;
            ++at;
        }
        if (at >= text.size())
        {
            tag.End = text.size();
            return tag;
        }
        tag.End = at + 1;
        tag.Kind = closing ? TagKind::Close : (at > tag.NameEnd && text[at - 1] == '/' ? TagKind::SelfClosing : TagKind::Open);
        return tag;
    }

    std::string_view TagName(std::string_view text, Tag const& tag)
    {
        return text.substr(tag.NameStart, tag.NameEnd - tag.NameStart);
    }

    std::optional<Element> FindElement(std::string_view text, std::size_t from, std::size_t to, std::string_view name)
    {
        std::size_t at = from;
        while (std::optional<Tag> const tag = NextTag(text, at))
        {
            if (tag->Start >= to)
                return std::nullopt;
            at = tag->End;
            if (tag->Kind == TagKind::Other || tag->Kind == TagKind::Close || TagName(text, *tag) != name)
                continue;
            Element element;
            element.Start = tag->Start;
            element.NameStart = tag->NameStart;
            element.NameEnd = tag->NameEnd;
            element.End = tag->End;
            if (tag->Kind == TagKind::SelfClosing)
            {
                element.SelfClosing = true;
                element.ContentStart = tag->End - 2;
                element.ContentEnd = element.ContentStart;
                return element;
            }
            element.ContentStart = tag->End;
            std::size_t depth = 1;
            std::size_t scan = tag->End;
            while (std::optional<Tag> const inner = NextTag(text, scan))
            {
                scan = inner->End;
                if (TagName(text, *inner) != name)
                    continue;
                if (inner->Kind == TagKind::Open)
                    ++depth;
                else if (inner->Kind == TagKind::Close && --depth == 0)
                {
                    element.ContentEnd = inner->Start;
                    element.End = inner->End;
                    return element;
                }
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    std::optional<Element> RootElement(std::string_view text)
    {
        std::size_t at = 0;
        while (std::optional<Tag> const tag = NextTag(text, at))
        {
            at = tag->End;
            if (tag->Kind != TagKind::Open && tag->Kind != TagKind::SelfClosing)
                continue;
            return FindElement(text, tag->Start, text.size(), TagName(text, *tag));
        }
        return std::nullopt;
    }

    std::string_view Content(std::string_view text, Element const& element)
    {
        return text.substr(element.ContentStart, element.ContentEnd - element.ContentStart);
    }

    std::optional<Element> FindKeyToSet(std::string_view text, std::size_t from, std::size_t to, std::string_view key, std::string_view value)
    {
        std::size_t at = from;
        while (std::optional<Element> const element = FindElement(text, at, to, key))
        {
            if (Content(text, *element) != value)
                return element;
            at = element->End;
        }
        return std::nullopt;
    }

    std::string_view LineEnding(std::string_view text)
    {
        std::size_t const line = text.find('\n');
        return line != std::string_view::npos && line > 0 && text[line - 1] == '\r' ? "\r\n" : "\n";
    }

    std::optional<std::string_view> Indent(std::string_view text, std::size_t position)
    {
        std::size_t start = position;
        while (start > 0 && text[start - 1] != '\n')
            --start;
        std::string_view const run = text.substr(start, position - start);
        for (char const character : run)
            if (character != ' ' && character != '\t')
                return std::nullopt;
        return run;
    }

    std::string IndentStep(std::string_view text)
    {
        std::size_t at = 0;
        while (at <= text.size())
        {
            std::size_t const found = text.find('\n', at);
            std::size_t const line = found == std::string_view::npos ? text.size() : found;
            std::size_t run = at;
            while (run < line && (text[run] == ' ' || text[run] == '\t'))
                ++run;
            if (run > at && run < line && text[run] != '\r')
                return std::string(text.substr(at, run - at));
            if (found == std::string_view::npos)
                break;
            at = found + 1;
        }
        return std::string(DefaultIndent);
    }

    std::string ChildIndent(std::string_view text, Element const& element, std::string_view step)
    {
        std::size_t at = element.ContentStart;
        while (std::optional<Tag> const tag = NextTag(text, at))
        {
            if (tag->Start >= element.ContentEnd)
                break;
            at = tag->End;
            if (tag->Kind != TagKind::Open && tag->Kind != TagKind::SelfClosing)
                continue;
            if (std::optional<std::string_view> const indent = Indent(text, tag->Start))
                return std::string(*indent);
            break;
        }
        std::optional<std::string_view> const own = Indent(text, element.Start);
        return std::string(own.value_or(std::string_view())).append(step);
    }

    void InsertLines(std::string& text, Element const& element, std::vector<std::string> const& lines, std::string_view indent, std::string_view lineEnd)
    {
        std::size_t at = element.ContentEnd;
        std::string block;
        if (std::optional<std::string_view> const run = Indent(text, at))
            at -= run->size();
        else
            block.append(lineEnd);
        for (std::string const& line : lines)
        {
            block.append(indent);
            block.append(line);
            block.append(lineEnd);
        }
        text.insert(at, block);
    }

    void SetContent(std::string& text, Element const& element, std::string_view name, std::string_view value)
    {
        if (element.SelfClosing)
            text.replace(element.ContentStart, element.End - element.ContentStart, fmt::format(">{}</{}>", value, name));
        else
            text.replace(element.ContentStart, element.ContentEnd - element.ContentStart, value);
    }
}

ClientConfigText::ClientConfigText(std::string text) : _text(std::move(text))
{
}

std::string const& ClientConfigText::Text() const
{
    return _text;
}

void ClientConfigText::RenameRoot(std::string_view name)
{
    std::optional<Element> const root = RootElement(_text);
    if (!root)
        return;
    if (!root->SelfClosing)
        _text.replace(root->ContentEnd, root->End - root->ContentEnd, fmt::format("</{}>", name));
    _text.replace(root->NameStart, root->NameEnd - root->NameStart, name);
}

void ClientConfigText::SetKey(std::string_view table, std::string_view key, std::string_view type, std::string_view value)
{
    std::string const step = IndentStep(_text);
    std::string_view const lineEnd = LineEnding(_text);
    std::optional<Element> held = FindElement(_text, 0, _text.size(), table);
    if (!held)
    {
        InsertTable(table, step, lineEnd);
        held = FindElement(_text, 0, _text.size(), table);
        if (!held)
            return;
    }
    if (!FindElement(_text, held->ContentStart, held->ContentEnd, key))
    {
        std::optional<Element> record = FindElement(_text, held->ContentStart, held->ContentEnd, RecordName);
        if (!record)
        {
            InsertLines(_text, *held, { fmt::format("<{}>", RecordName), fmt::format("</{}>", RecordName) }, ChildIndent(_text, *held, step), lineEnd);
            held = FindElement(_text, 0, _text.size(), table);
            if (!held)
                return;
            record = FindElement(_text, held->ContentStart, held->ContentEnd, RecordName);
            if (!record)
                return;
        }
        InsertLines(_text, *record, { fmt::format("<{} TYPE=\"{}\">{}</{}>", key, type, value, key) }, ChildIndent(_text, *record, step), lineEnd);
        return;
    }
    for (;;)
    {
        std::optional<Element> const found = FindElement(_text, 0, _text.size(), table);
        if (!found)
            return;
        std::optional<Element> const element = FindKeyToSet(_text, found->ContentStart, found->ContentEnd, key, value);
        if (!element)
            return;
        SetContent(_text, *element, key, value);
    }
}

void ClientConfigText::EmptyKey(std::string_view key)
{
    for (;;)
    {
        std::optional<Element> const element = FindKeyToSet(_text, 0, _text.size(), key, std::string_view());
        if (!element)
            return;
        SetContent(_text, *element, key, std::string_view());
    }
}

void ClientConfigText::InsertTable(std::string_view table, std::string_view step, std::string_view lineEnd)
{
    std::optional<Element> const root = RootElement(_text);
    if (!root)
        return;
    InsertLines(_text, *root, { fmt::format("<{}>", table), fmt::format("{}<{}>", step, RecordName), fmt::format("{}</{}>", step, RecordName), fmt::format("</{}>", table) },
        ChildIndent(_text, *root, step), lineEnd);
    ListTable(table, step, lineEnd);
}

void ClientConfigText::ListTable(std::string_view table, std::string_view step, std::string_view lineEnd)
{
    std::optional<Element> const list = FindElement(_text, 0, _text.size(), TableListName);
    if (!list)
        return;
    std::size_t at = list->ContentStart;
    while (std::optional<Element> const name = FindElement(_text, at, list->ContentEnd, TableNameKey))
    {
        if (Content(_text, *name) == table)
            return;
        at = name->End;
    }
    InsertLines(_text, *list, { fmt::format("<{}>", RecordName), fmt::format("{}<{} TYPE=\"STR\">{}</{}>", step, TableNameKey, table, TableNameKey), fmt::format("</{}>", RecordName) },
        ChildIndent(_text, *list, step), lineEnd);
}

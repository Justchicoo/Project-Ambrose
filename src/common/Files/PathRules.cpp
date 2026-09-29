/*
 * Project Ambrose by Imjustchico
 * Compiles a gitignore pattern into its segments, refusing one that is empty, too long, holds a control character, a doubled slash, a dot component, an unclosed class or a lone trailing backslash, and matches names with *, ?, classes with ranges and negation, and escapes, folding ASCII case where the file system does; ** matches any number of folders, one at the end at least one, worked out in one pass over the segments so no pattern costs more than its segments times the path's depth however many ** it holds; a layer excludes a path when the last rule matching one of its folders, or the path itself, is not a negation, and stops at the first folder excluded because nothing beneath an excluded folder comes back.
 */

#include "PathRules.h"

#include <algorithm>

namespace
{
    char Lower(char c) noexcept
    {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }

    char Upper(char c) noexcept
    {
        return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
    }

    bool Same(char pattern, char name, bool fold) noexcept
    {
        return fold ? Lower(pattern) == Lower(name) : pattern == name;
    }

    std::size_t CharacterLength(std::string_view text, std::size_t at) noexcept
    {
        unsigned char const lead = static_cast<unsigned char>(text[at]);
        std::size_t length = 1;
        if (lead >= 0xF0)
            length = 4;
        else if (lead >= 0xE0)
            length = 3;
        else if (lead >= 0xC0)
            length = 2;
        return std::min(length, text.size() - at);
    }

    struct ClassResult
    {
        bool Valid = false;
        bool Matched = false;
        std::size_t Length = 0;
    };

    ClassResult MatchClass(std::string_view pattern, std::size_t start, char wanted, bool fold)
    {
        ClassResult result;
        std::size_t index = start + 1;
        bool negate = false;
        if (index < pattern.size() && (pattern[index] == '!' || pattern[index] == '^'))
        {
            negate = true;
            ++index;
        }
        bool first = true;
        bool matched = false;
        while (index < pattern.size())
        {
            char low = pattern[index];
            if (low == ']' && !first)
            {
                result.Valid = true;
                result.Matched = matched != negate;
                result.Length = index + 1 - start;
                return result;
            }
            first = false;
            if (low == '\\')
            {
                if (++index >= pattern.size())
                    return result;
                low = pattern[index];
            }
            char high = low;
            if (index + 2 < pattern.size() && pattern[index + 1] == '-' && pattern[index + 2] != ']')
            {
                index += 2;
                high = pattern[index];
                if (high == '\\')
                {
                    if (++index >= pattern.size())
                        return result;
                    high = pattern[index];
                }
            }
            ++index;
            unsigned char const from = static_cast<unsigned char>(low);
            unsigned char const to = static_cast<unsigned char>(high);
            auto const within = [from, to](char c) { return static_cast<unsigned char>(c) >= from && static_cast<unsigned char>(c) <= to; };
            if (within(wanted) || (fold && (within(Lower(wanted)) || within(Upper(wanted)))))
                matched = true;
        }
        return result;
    }

    bool GlobMatch(std::string_view pattern, std::string_view name, bool fold)
    {
        constexpr std::size_t None = std::string_view::npos;
        std::size_t p = 0;
        std::size_t n = 0;
        std::size_t starPattern = None;
        std::size_t starName = 0;
        while (n < name.size())
        {
            if (p < pattern.size())
            {
                char const c = pattern[p];
                if (c == '*')
                {
                    while (p < pattern.size() && pattern[p] == '*')
                        ++p;
                    starPattern = p;
                    starName = n;
                    continue;
                }
                if (c == '?')
                {
                    ++p;
                    n += CharacterLength(name, n);
                    continue;
                }
                if (c == '[')
                {
                    ClassResult const match = MatchClass(pattern, p, name[n], fold);
                    if (match.Valid && match.Matched)
                    {
                        p += match.Length;
                        ++n;
                        continue;
                    }
                    if (!match.Valid && Same('[', name[n], fold))
                    {
                        ++p;
                        ++n;
                        continue;
                    }
                }
                else
                {
                    std::size_t const literal = c == '\\' && p + 1 < pattern.size() ? p + 1 : p;
                    if (Same(pattern[literal], name[n], fold))
                    {
                        p = literal + 1;
                        ++n;
                        continue;
                    }
                }
            }
            if (starPattern == None)
                return false;
            p = starPattern;
            n = ++starName;
        }
        while (p < pattern.size() && pattern[p] == '*')
            ++p;
        return p == pattern.size();
    }

    bool ValidSegment(std::string_view segment, std::string& error)
    {
        if (segment == "." || segment == "..")
        {
            error = "A pattern cannot name . or .., because it is matched inside its root";
            return false;
        }
        for (std::size_t index = 0; index < segment.size(); ++index)
        {
            if (segment[index] == '\\')
            {
                if (index + 1 >= segment.size())
                {
                    error = "A pattern ends in a lone backslash, which escapes nothing";
                    return false;
                }
                ++index;
                continue;
            }
            if (segment[index] == '[')
            {
                ClassResult const match = MatchClass(segment, index, '\0', false);
                if (!match.Valid)
                {
                    error = "A pattern opens a [ class it never closes";
                    return false;
                }
                index += match.Length - 1;
            }
        }
        return true;
    }

    std::vector<std::string> SplitPath(std::string_view text)
    {
        std::vector<std::string> parts;
        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t const slash = text.find('/', start);
            std::string_view const part = text.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start);
            if (!part.empty())
                parts.emplace_back(part);
            if (slash == std::string_view::npos)
                break;
            start = slash + 1;
        }
        return parts;
    }
}

Ambrose::PathRules::PathRules(CaseMode mode) : _case(mode)
{
}

Ambrose::CaseMode Ambrose::PathRules::PlatformCase() noexcept
{
#if defined(_WIN32) || defined(__APPLE__)
    return CaseMode::Insensitive;
#else
    return CaseMode::Sensitive;
#endif
}

bool Ambrose::PathRules::IsComment(std::string_view line) noexcept
{
    std::size_t const first = line.find_first_not_of(" \t");
    return first == std::string_view::npos || line[first] == '#';
}

std::string_view Ambrose::PathRules::EffectName(RuleEffect effect) noexcept
{
    switch (effect)
    {
        case RuleEffect::Hide: return "hide";
        case RuleEffect::ReadOnly: return "read_only";
        case RuleEffect::ClientDerived: return "client_derived";
        case RuleEffect::Elsewhere: break;
    }
    return "elsewhere";
}

std::string Ambrose::PathRules::Escape(std::string_view literal)
{
    std::string escaped;
    escaped.reserve(literal.size() * 2);
    for (std::size_t index = 0; index < literal.size(); ++index)
    {
        char const c = literal[index];
        bool const special = c == '*' || c == '?' || c == '[' || c == ']' || c == '\\' || (index == 0 && (c == '!' || c == '#')) || (index + 1 == literal.size() && c == ' ');
        if (special)
            escaped.push_back('\\');
        escaped.push_back(c);
    }
    return escaped;
}

std::optional<Ambrose::PathRule> Ambrose::PathRules::Compile(std::string_view pattern, RuleEffect effect, RuleOrigin origin, std::string why, std::string& error)
{
    PathRule rule;
    rule.Pattern = std::string(pattern);
    rule.Effect = effect;
    rule.Origin = origin;
    rule.Why = std::move(why);
    if (pattern.size() > MaxPatternBytes)
    {
        error = "A pattern may be at most 1024 bytes long";
        return std::nullopt;
    }
    if (std::any_of(pattern.begin(), pattern.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; }))
    {
        error = "A pattern may not hold a control character";
        return std::nullopt;
    }
    std::string_view text = pattern;
    if (!text.empty() && text.front() == '!')
    {
        rule.Negated = true;
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ' && !(text.size() >= 2 && text[text.size() - 2] == '\\'))
        text.remove_suffix(1);
    if (text.empty())
    {
        error = "A pattern needs something to match";
        return std::nullopt;
    }
    if (text.back() == '/')
    {
        rule.FolderOnly = true;
        text.remove_suffix(1);
    }
    if (!text.empty() && text.front() == '/')
    {
        rule.Anchored = true;
        text.remove_prefix(1);
    }
    if (text.empty())
    {
        error = "A pattern needs a name to match, not only a slash";
        return std::nullopt;
    }
    if (text.find('/') != std::string_view::npos)
        rule.Anchored = true;
    std::size_t start = 0;
    while (true)
    {
        std::size_t const slash = text.find('/', start);
        std::string_view const segment = text.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start);
        if (segment.empty())
        {
            error = "A pattern has no empty component; drop the doubled slash";
            return std::nullopt;
        }
        if (!ValidSegment(segment, error))
            return std::nullopt;
        rule.Segments.emplace_back(segment);
        if (slash == std::string_view::npos)
            break;
        start = slash + 1;
    }
    return rule;
}

bool Ambrose::PathRules::Add(std::string_view pattern, RuleEffect effect, RuleOrigin origin, std::string why, std::string& error)
{
    std::optional<PathRule> rule = Compile(pattern, effect, origin, std::move(why), error);
    if (!rule)
        return false;
    _rules.push_back(std::move(*rule));
    return true;
}

void Ambrose::PathRules::Add(PathRule rule)
{
    _rules.push_back(std::move(rule));
}

std::vector<std::string> Ambrose::PathRules::OperatorPatterns() const
{
    std::vector<std::string> patterns;
    for (PathRule const& rule : _rules)
        if (rule.Origin == RuleOrigin::Operator)
            patterns.push_back(rule.Pattern);
    return patterns;
}

bool Ambrose::PathRules::Glob(std::string_view pattern, std::string_view name) const
{
    return GlobMatch(pattern, name, _case == CaseMode::Insensitive);
}

bool Ambrose::PathRules::SegmentsMatch(std::span<std::string const> segments, std::span<std::string const> components) const
{
    std::size_t const width = components.size() + 1;
    std::vector<char> rest(width, 0);
    rest[components.size()] = 1;
    std::vector<char> here(width, 0);
    for (std::size_t segment = segments.size(); segment-- > 0;)
    {
        if (segments[segment] == "**")
        {
            bool const last = segment + 1 == segments.size();
            char any = 0;
            for (std::size_t at = width; at-- > 0;)
            {
                any = static_cast<char>(any | rest[at]);
                here[at] = last ? static_cast<char>(at < components.size()) : any;
            }
        }
        else
        {
            for (std::size_t at = 0; at < width; ++at)
                here[at] = static_cast<char>(at < components.size() && rest[at + 1] && Glob(segments[segment], components[at]));
        }
        rest.swap(here);
    }
    return rest[0] != 0;
}

bool Ambrose::PathRules::Matches(PathRule const& rule, std::span<std::string const> prefix, bool isFolder) const
{
    if (prefix.empty() || rule.Segments.empty())
        return false;
    if (rule.FolderOnly && !isFolder)
        return false;
    if (!rule.Anchored)
        return Glob(rule.Segments.front(), prefix.back());
    return SegmentsMatch(rule.Segments, prefix);
}

std::optional<Ambrose::RuleHit> Ambrose::PathRules::Layer(RuleEffect effect, RuleOrigin origin, std::span<std::string const> components, bool isFolder) const
{
    for (std::size_t depth = 1; depth <= components.size(); ++depth)
    {
        std::span<std::string const> const prefix = components.first(depth);
        bool const folder = depth < components.size() || isFolder;
        PathRule const* last = nullptr;
        for (PathRule const& rule : _rules)
            if (rule.Effect == effect && rule.Origin == origin && Matches(rule, prefix, folder))
                last = &rule;
        if (last && !last->Negated)
        {
            RuleHit hit;
            hit.Pattern = last->Pattern;
            hit.Effect = last->Effect;
            hit.Origin = last->Origin;
            hit.Why = last->Why;
            return hit;
        }
    }
    return std::nullopt;
}

std::optional<Ambrose::RuleHit> Ambrose::PathRules::Match(std::span<std::string const> components, bool isFolder) const
{
    struct Step
    {
        RuleEffect Effect;
        RuleOrigin Origin;
    };
    static constexpr Step Order[] = {
        { RuleEffect::Elsewhere, RuleOrigin::BuiltIn },
        { RuleEffect::Hide, RuleOrigin::BuiltIn },
        { RuleEffect::Hide, RuleOrigin::Operator },
        { RuleEffect::ClientDerived, RuleOrigin::BuiltIn },
        { RuleEffect::ReadOnly, RuleOrigin::BuiltIn },
    };
    for (Step const& step : Order)
        if (std::optional<RuleHit> hit = Layer(step.Effect, step.Origin, components, isFolder))
            return hit;
    return std::nullopt;
}

std::optional<Ambrose::RuleHit> Ambrose::PathRules::Match(std::string_view relative, bool isFolder) const
{
    std::vector<std::string> const components = SplitPath(relative);
    return Match(std::span<std::string const>(components), isFolder);
}

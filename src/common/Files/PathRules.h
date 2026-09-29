/*
 * Project Ambrose by Imjustchico
 * Protected path patterns in gitignore syntax, each with an effect, hide, read-only, client-derived or carved out as another root, an origin, built in or the operator's, and the sentence saying why it exists; a path is matched with each of its folders the way git matches, an excluded folder taking everything beneath it, in layers so a carve-out comes first, then the built-in protections, then the operator's own patterns, whose negations can only undo the operator's own, then client-derived data and read-only files, and a match names the rule responsible.
 */

#ifndef AMBROSE_PATHRULES_H
#define AMBROSE_PATHRULES_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Ambrose
{
    enum class RuleEffect : uint8
    {
        Hide,
        ReadOnly,
        ClientDerived,
        Elsewhere
    };

    enum class RuleOrigin : uint8
    {
        BuiltIn,
        Operator
    };

    enum class CaseMode : uint8
    {
        Sensitive,
        Insensitive
    };

    struct PathRule
    {
        std::string Pattern = {};
        RuleEffect Effect = RuleEffect::Hide;
        RuleOrigin Origin = RuleOrigin::BuiltIn;
        std::string Why = {};
        bool Negated = false;
        bool Anchored = false;
        bool FolderOnly = false;
        std::vector<std::string> Segments = {};
    };

    struct RuleHit
    {
        std::string Pattern = {};
        RuleEffect Effect = RuleEffect::Hide;
        RuleOrigin Origin = RuleOrigin::BuiltIn;
        std::string Why = {};
    };

    class PathRules
    {
    public:
        static constexpr std::size_t MaxPatternBytes = 1024;
        static constexpr std::size_t MaxOperatorPatterns = 256;

        explicit PathRules(CaseMode mode = PlatformCase());

        static CaseMode PlatformCase() noexcept;
        static std::optional<PathRule> Compile(std::string_view pattern, RuleEffect effect, RuleOrigin origin, std::string why, std::string& error);
        static std::string Escape(std::string_view literal);
        static std::string_view EffectName(RuleEffect effect) noexcept;
        static bool IsComment(std::string_view line) noexcept;

        void SetCaseMode(CaseMode mode) noexcept { _case = mode; }
        CaseMode GetCaseMode() const noexcept { return _case; }
        bool Add(std::string_view pattern, RuleEffect effect, RuleOrigin origin, std::string why, std::string& error);
        void Add(PathRule rule);

        std::optional<RuleHit> Match(std::span<std::string const> components, bool isFolder) const;
        std::optional<RuleHit> Match(std::string_view relative, bool isFolder) const;
        bool Glob(std::string_view pattern, std::string_view name) const;

        std::vector<PathRule> const& Rules() const noexcept { return _rules; }
        std::vector<std::string> OperatorPatterns() const;

    private:
        bool Matches(PathRule const& rule, std::span<std::string const> prefix, bool isFolder) const;
        bool SegmentsMatch(std::span<std::string const> segments, std::span<std::string const> components) const;
        std::optional<RuleHit> Layer(RuleEffect effect, RuleOrigin origin, std::span<std::string const> components, bool isFolder) const;

        CaseMode _case;
        std::vector<PathRule> _rules;
    };
}

#endif

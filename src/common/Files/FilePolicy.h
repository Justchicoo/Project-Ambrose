/*
 * Project Ambrose by Imjustchico
 * Every operation a file root can be asked for, from listing to remote pull, and a root's policy of which it allows, each refusal carrying the sentence a person reads and a code a page acts on, with whether the root is client-derived; and the effective policy at one path, the root's policy combined with the rule that matched it, so a protected path, a carved-out folder, client-derived data and a read-only file each refuse exactly what they should and name why.
 */

#ifndef AMBROSE_FILEPOLICY_H
#define AMBROSE_FILEPOLICY_H

#include "PathRules.h"
#include "Types.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Ambrose
{
    enum class FileOperation : uint8
    {
        List,
        Read,
        Preview,
        Download,
        Archive,
        Share,
        Write,
        Upload,
        Create,
        Rename,
        Move,
        Copy,
        Delete,
        Permissions,
        Extract,
        Truncate,
        Sftp,
        Pull
    };

    inline constexpr std::size_t FileOperationCount = 18;

    struct OperationVerdict
    {
        bool Allowed = true;
        std::string Code = {};
        std::string Reason = {};
        std::string Rule = {};
    };

    class FilePolicy
    {
    public:
        static std::span<FileOperation const> Operations() noexcept;
        static std::string_view NameOf(FileOperation operation) noexcept;
        static std::optional<FileOperation> Parse(std::string_view name) noexcept;
        static bool Changes(FileOperation operation) noexcept;
        static bool HandsOutBytes(FileOperation operation) noexcept;

        FilePolicy();

        FilePolicy& Refuse(FileOperation operation, std::string reason, std::string code = "refused_by_policy");
        FilePolicy& RefuseAllBut(std::span<FileOperation const> allowed, std::string const& reason, std::string const& code = "refused_by_policy");
        FilePolicy& SetSummary(std::string summary);
        FilePolicy& SetClientDerived(bool clientDerived) noexcept;

        bool IsClientDerived() const noexcept { return _clientDerived; }
        bool IsReadOnly() const noexcept;
        std::string const& Summary() const noexcept { return _summary; }
        OperationVerdict Decide(FileOperation operation) const;
        OperationVerdict DecideAt(std::optional<RuleHit> const& hit, FileOperation operation) const;

    private:
        struct Refusal
        {
            bool Refused = false;
            std::string Reason = {};
            std::string Code = {};
        };

        std::array<Refusal, FileOperationCount> _refusals;
        std::string _summary;
        bool _clientDerived = false;
    };
}

#endif

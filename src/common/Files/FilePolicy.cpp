/*
 * Project Ambrose by Imjustchico
 * Names each operation as the API spells it, says which change the disk and which hand a file's bytes out, keeps a root's refusals by operation, and decides one operation at one path: a protected path refuses everything, naming its pattern, a folder carved out as another root refuses everything and names that root, client-derived data allows only a listing, and a read-only file refuses every change, before the root's own policy has its say.
 */

#include "FilePolicy.h"

#include <fmt/format.h>

#include <algorithm>

namespace
{
    constexpr std::array<Ambrose::FileOperation, Ambrose::FileOperationCount> AllOperations{
        Ambrose::FileOperation::List,
        Ambrose::FileOperation::Read,
        Ambrose::FileOperation::Preview,
        Ambrose::FileOperation::Download,
        Ambrose::FileOperation::Archive,
        Ambrose::FileOperation::Share,
        Ambrose::FileOperation::Write,
        Ambrose::FileOperation::Upload,
        Ambrose::FileOperation::Create,
        Ambrose::FileOperation::Rename,
        Ambrose::FileOperation::Move,
        Ambrose::FileOperation::Copy,
        Ambrose::FileOperation::Delete,
        Ambrose::FileOperation::Permissions,
        Ambrose::FileOperation::Extract,
        Ambrose::FileOperation::Truncate,
        Ambrose::FileOperation::Sftp,
        Ambrose::FileOperation::Pull,
    };

    std::size_t IndexOf(Ambrose::FileOperation operation) noexcept
    {
        return static_cast<std::size_t>(operation);
    }
}

std::span<Ambrose::FileOperation const> Ambrose::FilePolicy::Operations() noexcept
{
    return AllOperations;
}

std::string_view Ambrose::FilePolicy::NameOf(FileOperation operation) noexcept
{
    switch (operation)
    {
        case FileOperation::List: return "list";
        case FileOperation::Read: return "read";
        case FileOperation::Preview: return "preview";
        case FileOperation::Download: return "download";
        case FileOperation::Archive: return "archive";
        case FileOperation::Share: return "share";
        case FileOperation::Write: return "write";
        case FileOperation::Upload: return "upload";
        case FileOperation::Create: return "create";
        case FileOperation::Rename: return "rename";
        case FileOperation::Move: return "move";
        case FileOperation::Copy: return "copy";
        case FileOperation::Delete: return "delete";
        case FileOperation::Permissions: return "permissions";
        case FileOperation::Extract: return "extract";
        case FileOperation::Truncate: return "truncate";
        case FileOperation::Sftp: return "sftp";
        case FileOperation::Pull: break;
    }
    return "pull";
}

std::optional<Ambrose::FileOperation> Ambrose::FilePolicy::Parse(std::string_view name) noexcept
{
    for (FileOperation const operation : AllOperations)
        if (NameOf(operation) == name)
            return operation;
    return std::nullopt;
}

bool Ambrose::FilePolicy::Changes(FileOperation operation) noexcept
{
    switch (operation)
    {
        case FileOperation::Write:
        case FileOperation::Upload:
        case FileOperation::Create:
        case FileOperation::Rename:
        case FileOperation::Move:
        case FileOperation::Copy:
        case FileOperation::Delete:
        case FileOperation::Permissions:
        case FileOperation::Extract:
        case FileOperation::Truncate:
        case FileOperation::Pull:
            return true;
        default:
            return false;
    }
}

bool Ambrose::FilePolicy::HandsOutBytes(FileOperation operation) noexcept
{
    switch (operation)
    {
        case FileOperation::Read:
        case FileOperation::Preview:
        case FileOperation::Download:
        case FileOperation::Archive:
        case FileOperation::Share:
        case FileOperation::Sftp:
            return true;
        default:
            return false;
    }
}

Ambrose::FilePolicy::FilePolicy() = default;

Ambrose::FilePolicy& Ambrose::FilePolicy::Refuse(FileOperation operation, std::string reason, std::string code)
{
    Refusal& refusal = _refusals[IndexOf(operation)];
    refusal.Refused = true;
    refusal.Reason = std::move(reason);
    refusal.Code = std::move(code);
    return *this;
}

Ambrose::FilePolicy& Ambrose::FilePolicy::RefuseAllBut(std::span<FileOperation const> allowed, std::string const& reason, std::string const& code)
{
    for (FileOperation const operation : AllOperations)
        if (std::find(allowed.begin(), allowed.end(), operation) == allowed.end())
            Refuse(operation, reason, code);
    return *this;
}

Ambrose::FilePolicy& Ambrose::FilePolicy::SetSummary(std::string summary)
{
    _summary = std::move(summary);
    return *this;
}

Ambrose::FilePolicy& Ambrose::FilePolicy::SetClientDerived(bool clientDerived) noexcept
{
    _clientDerived = clientDerived;
    return *this;
}

bool Ambrose::FilePolicy::IsReadOnly() const noexcept
{
    return std::all_of(AllOperations.begin(), AllOperations.end(), [this](FileOperation operation) { return !Changes(operation) || _refusals[IndexOf(operation)].Refused; });
}

Ambrose::OperationVerdict Ambrose::FilePolicy::Decide(FileOperation operation) const
{
    OperationVerdict verdict;
    Refusal const& refusal = _refusals[IndexOf(operation)];
    if (!refusal.Refused)
        return verdict;
    verdict.Allowed = false;
    verdict.Code = refusal.Code.empty() ? std::string("refused_by_policy") : refusal.Code;
    verdict.Reason = refusal.Reason;
    return verdict;
}

Ambrose::OperationVerdict Ambrose::FilePolicy::DecideAt(std::optional<RuleHit> const& hit, FileOperation operation) const
{
    if (hit)
    {
        OperationVerdict verdict;
        verdict.Allowed = false;
        verdict.Rule = hit->Pattern;
        switch (hit->Effect)
        {
            case RuleEffect::Hide:
                verdict.Code = "protected";
                verdict.Reason = fmt::format("This path is protected by {}: {}", hit->Pattern, hit->Why);
                return verdict;
            case RuleEffect::Elsewhere:
                verdict.Code = "elsewhere";
                verdict.Reason = hit->Why;
                return verdict;
            case RuleEffect::ClientDerived:
                if (operation == FileOperation::List)
                    break;
                verdict.Code = "client_derived";
                verdict.Reason = hit->Why;
                return verdict;
            case RuleEffect::ReadOnly:
                if (!Changes(operation))
                    break;
                verdict.Code = "read_only";
                verdict.Reason = hit->Why;
                return verdict;
        }
    }
    return Decide(operation);
}

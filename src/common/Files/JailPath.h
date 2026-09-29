/*
 * Project Ambrose by Imjustchico
 * A request's path inside a file root, parsed before anything touches a disk: one strict percent-decoding pass for a path carried in a query and none for one carried in a JSON body, then valid UTF-8 and every component checked, refusing traversal, absolute paths, drive letters, UNC and device prefixes, backslashes, empty components, control characters, names Windows cannot hold and reserved device names on every system, each refusal with a kind; and the host path a request would have reached, worked out for the audit row alone.
 */

#ifndef AMBROSE_JAILPATH_H
#define AMBROSE_JAILPATH_H

#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Ambrose
{
    enum class JailRefusal : uint8
    {
        None,
        Traversal,
        Absolute,
        Encoding,
        Name,
        DeviceName,
        TooLong,
        TooDeep
    };

    struct JailPath
    {
        std::vector<std::string> Components = {};

        bool IsRoot() const noexcept { return Components.empty(); }
        std::string Text() const;
        std::string Leaf() const;
        JailPath Parent() const;
        JailPath Child(std::string name) const;
    };

    struct JailPathResult
    {
        JailPath Path = {};
        JailRefusal Refusal = JailRefusal::None;
        std::string Reason = {};
        std::string Decoded = {};

        bool Ok() const noexcept { return Refusal == JailRefusal::None; }
    };

    namespace JailPaths
    {
        inline constexpr std::size_t MaxComponentBytes = 255;
        inline constexpr std::size_t MaxComponents = 64;
        inline constexpr std::size_t MaxPathBytes = 4096;

        std::optional<std::string> PercentDecode(std::string_view raw);
        JailPathResult ParseQuery(std::string_view raw);
        JailPathResult ParseText(std::string_view text);
        JailRefusal CheckName(std::string_view name, std::string& reason);
        bool IsReservedDeviceName(std::string_view name);
        std::string_view RefusalCode(JailRefusal refusal) noexcept;
        std::filesystem::path HostPathFor(std::filesystem::path const& root, std::string_view decoded);
    }
}

#endif

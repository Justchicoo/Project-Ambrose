/*
 * Project Ambrose by Imjustchico
 * The jail every file request goes through: a root held open as a handle, and each request resolved against it through handles rather than path strings, on Linux with openat2 beneath the root or a component walk that follows no link, on Windows with opens relative to each parent that never follow a reparse point and a final path compared with the root's, refusing a link, a junction, a mount point, a FIFO, a device or a socket before anything reads it; an entry carries its kind, size, modification time, link count and identity, a folder lists from its own handle with a row for every entry, special ones marked unopenable and failures as error rows, a read reopens the file and checks it is still the one resolved, a create reserves its space before it makes the file exclusively, and a rename or a removal reopens what it acts on relative to its parent and acts only if it is still the entry that was resolved, a rename never replacing unless asked.
 */

#ifndef AMBROSE_FILEJAIL_H
#define AMBROSE_FILEJAIL_H

#include "JailPath.h"
#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Ambrose
{
    class SpaceGuard;
    struct SpaceRefusal;

    enum class EntryKind : uint8
    {
        File,
        Folder,
        Link,
        Fifo,
        Device,
        Socket,
        Other
    };

    struct FileIdentity
    {
        uint64 Volume = 0;
        uint64 High = 0;
        uint64 Low = 0;
        bool Known = false;

        bool Same(FileIdentity const& other) const noexcept { return Known && other.Known && Volume == other.Volume && High == other.High && Low == other.Low; }
    };

    struct JailStat
    {
        EntryKind Kind = EntryKind::Other;
        uint64 Size = 0;
        int64 ModifiedEpochMs = 0;
        uint64 Links = 1;
        FileIdentity Identity = {};
    };

    enum class JailFailure : uint8
    {
        None,
        Refused,
        Missing,
        Conflict,
        NoSpace,
        Failed
    };

    struct JailError
    {
        JailFailure Failure = JailFailure::None;
        std::string Code = {};
        std::string Message = {};
        std::string Resolved = {};

        void Set(JailFailure failure, std::string code, std::string message, std::string resolved = {});
    };

    enum class LinkPolicy : uint8
    {
        Refuse,
        FollowInside
    };

    class JailHandle
    {
    public:
#ifdef _WIN32
        using Native = void*;
#else
        using Native = int;
#endif

        JailHandle() noexcept = default;
        explicit JailHandle(Native native) noexcept;
        ~JailHandle();
        JailHandle(JailHandle&& other) noexcept;
        JailHandle& operator=(JailHandle&& other) noexcept;
        JailHandle(JailHandle const&) = delete;
        JailHandle& operator=(JailHandle const&) = delete;

        bool Valid() const noexcept;
        Native Get() const noexcept { return _native; }
        void Reset() noexcept;
        JailHandle Duplicate() const;

        static Native Invalid() noexcept;

    private:
        Native _native = Invalid();
    };

    class JailRoot
    {
    public:
        static std::shared_ptr<JailRoot> Open(std::filesystem::path const& path, std::string& error);

        JailRoot(JailRoot const&) = delete;
        JailRoot& operator=(JailRoot const&) = delete;

        std::filesystem::path const& GetPath() const noexcept { return _path; }
        FileIdentity const& GetIdentity() const noexcept { return _identity; }
        JailHandle const& GetHandle() const noexcept { return _handle; }
#ifdef _WIN32
        std::wstring const& GetFinalPath() const noexcept { return _final; }
#endif

    private:
        JailRoot() = default;

        std::filesystem::path _path;
        JailHandle _handle;
        FileIdentity _identity;
#ifdef _WIN32
        std::wstring _final;
#endif
    };

    struct JailEntry
    {
        JailHandle Handle = {};
        JailHandle Parent = {};
        std::string Leaf = {};
        std::vector<std::string> Components = {};
        JailStat Stat = {};
        std::filesystem::path HostPath = {};

        std::string Relative() const;
    };

    struct JailListed
    {
        std::string Name = {};
        EntryKind Kind = EntryKind::Other;
        uint64 Size = 0;
        int64 ModifiedEpochMs = 0;
        FileIdentity Identity = {};
        bool Openable = true;
        std::string Problem = {};
    };

    class FileJail
    {
    public:
        static constexpr std::size_t MaxLinksFollowed = 40;

        FileJail() = delete;

        static std::optional<JailEntry> Resolve(JailRoot const& root, JailPath const& path, LinkPolicy links, JailError& error);
        static bool List(JailEntry const& folder, std::size_t mostEntries, std::vector<JailListed>& entries, bool& truncated, JailError& error);
        static bool Read(JailEntry const& entry, uint64 offset, std::size_t length, std::string& bytes, JailError& error);
        static bool Create(JailRoot const& root, JailPath const& path, std::string_view contents, SpaceGuard& guard, JailError& error, SpaceRefusal* refusal = nullptr);
        static bool Replace(JailEntry const& entry, std::string_view contents, std::string_view previous, SpaceGuard& guard, JailError& error, SpaceRefusal* refusal = nullptr);
        static bool Rename(JailEntry const& source, JailEntry const& targetParent, std::string_view name, bool replace, JailError& error);
        static bool Remove(JailEntry const& entry, JailError& error);
        static bool IdentityOf(std::filesystem::path const& path, FileIdentity& identity, std::string& error);
        static bool UsesOpenat2() noexcept;
        static std::string_view KindName(EntryKind kind) noexcept;
        static std::string HostText(std::filesystem::path const& path);
    };
}

#endif

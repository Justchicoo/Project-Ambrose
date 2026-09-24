/*
 * Project Ambrose by Imjustchico
 * Maps a request path onto the panel folder segment by segment, refuses anything that is not a plain name, a known type or a regular file inside the folder, reads the file whole within a size cap, and sets its type and caching.
 */

#include "AdminFiles.h"
#include "ConfigMgr.h"
#include "StringUtil.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>

namespace
{
    constexpr std::string_view AssetPrefix = "/assets/";

    bool PlainName(std::string_view segment)
    {
        if (segment.empty() || segment.size() > AdminFiles::MaxSegmentLength)
            return false;
        auto const plain = [](char character)
        {
            return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '.' || character == '_' || character == '-';
        };
        char const first = segment.front();
        bool const startsPlain = (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || (first >= '0' && first <= '9');
        return startsPlain && std::all_of(segment.begin(), segment.end(), plain);
    }

    std::optional<std::string> ReadWhole(std::filesystem::path const& file)
    {
        std::error_code code;
        std::uintmax_t const size = std::filesystem::file_size(file, code);
        if (code || size > AdminFiles::MaxFileBytes)
            return std::nullopt;
        std::ifstream stream(file, std::ios::binary);
        if (!stream)
            return std::nullopt;
        std::string contents(static_cast<std::size_t>(size), '\0');
        if (size != 0 && !stream.read(contents.data(), static_cast<std::streamsize>(size)))
            return std::nullopt;
        return contents;
    }
}

void AdminFiles::SetRoot(std::filesystem::path root)
{
    std::unique_lock const lock(_mutex);
    _root = std::move(root);
}

std::filesystem::path AdminFiles::GetRoot() const
{
    std::shared_lock const lock(_mutex);
    return _root;
}

std::optional<std::string_view> AdminFiles::ContentType(std::filesystem::path const& file)
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 10> Types{ {
        { ".html", "text/html; charset=utf-8" },
        { ".js", "text/javascript; charset=utf-8" },
        { ".css", "text/css; charset=utf-8" },
        { ".json", "application/json" },
        { ".svg", "image/svg+xml" },
        { ".png", "image/png" },
        { ".webp", "image/webp" },
        { ".ico", "image/x-icon" },
        { ".woff2", "font/woff2" },
        { ".txt", "text/plain; charset=utf-8" },
    } };
    std::string const extension = Ambrose::ToLower(file.extension().string());
    for (auto const& [known, type] : Types)
        if (extension == known)
            return type;
    return std::nullopt;
}

std::optional<std::filesystem::path> AdminFiles::Resolve(std::filesystem::path const& root, std::string_view path)
{
    if (root.empty() || path.empty() || path.front() != '/')
        return std::nullopt;
    std::string_view relative = path.substr(1);
    if (relative.empty())
        relative = "index.html";

    std::filesystem::path candidate = root;
    while (!relative.empty())
    {
        std::size_t const slash = relative.find('/');
        std::string_view const segment = relative.substr(0, slash);
        if (!PlainName(segment))
            return std::nullopt;
        candidate /= std::filesystem::path(std::string(segment));
        relative = slash == std::string_view::npos ? std::string_view() : relative.substr(slash + 1);
        if (slash != std::string_view::npos && relative.empty())
            return std::nullopt;
    }

    if (!ContentType(candidate))
        return std::nullopt;
    std::error_code code;
    if (!std::filesystem::is_regular_file(candidate, code) || code)
        return std::nullopt;
    std::filesystem::path const folder = std::filesystem::canonical(root, code);
    if (code)
        return std::nullopt;
    std::filesystem::path const file = std::filesystem::canonical(candidate, code);
    if (code)
        return std::nullopt;
    auto const [folderEnd, fileAt] = std::mismatch(folder.begin(), folder.end(), file.begin(), file.end());
    if (folderEnd != folder.end() || fileAt == file.end())
        return std::nullopt;
    return file;
}

AdminResponse AdminFiles::Serve(AdminRequest const& request) const
{
    std::string const method = Ambrose::ToUpper(request.Method);
    if (method != "GET" && method != "HEAD")
    {
        AdminResponse refused = AdminResponse::Problem(405, "method_not_allowed", request.Path + " answers GET, HEAD");
        refused.Headers.emplace_back("Allow", "GET, HEAD");
        return refused;
    }

    std::filesystem::path const root = GetRoot();
    std::optional<std::filesystem::path> const file = Resolve(root, request.Path);
    if (!file)
    {
        if (request.Path == "/")
            return AdminResponse::Problem(404, "panel_not_installed",
                "No built panel is in " + ConfigMgr::PathToUtf8(root) + "; build apps/dashboard and point Admin.DashboardDir at its dist folder");
        return AdminResponse::Problem(404, "not_found", "The admin API has no " + request.Path);
    }

    std::optional<std::string> contents = ReadWhole(*file);
    if (!contents)
        return AdminResponse::Problem(500, "file_unreadable", "The admin API could not read " + request.Path);

    AdminResponse response;
    response.Status = 200;
    response.ContentType = std::string(*ContentType(*file));
    response.Body = std::move(*contents);
    response.Headers.emplace_back("Cache-Control", request.Path.starts_with(AssetPrefix) ? "public, max-age=31536000, immutable" : "no-cache");
    return response;
}

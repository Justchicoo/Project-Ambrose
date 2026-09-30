/*
 * Project Ambrose by Imjustchico
 * The built panel's files as the admin API serves them without a token, from a folder or from a copy compiled into the program: GET and HEAD only, the index at the root, a path taken only when every segment is a plain file name and the file stays inside the folder once links are followed, only the file types the build writes, the fingerprinted assets cached for a year and everything else revalidated, and a note on what to set when the folder holds no panel.
 */

#ifndef AMBROSE_ADMINFILES_H
#define AMBROSE_ADMINFILES_H

#include "AdminRouter.h"

class EmbeddedPage;

#include <cstdint>
#include <filesystem>
#include <optional>
#include <shared_mutex>
#include <string_view>

class AdminFiles
{
public:
    static constexpr std::uintmax_t MaxFileBytes = 16 * 1024 * 1024;
    static constexpr std::size_t MaxSegmentLength = 255;

    void SetRoot(std::filesystem::path root);
    void SetEmbedded(EmbeddedPage const* page);
    std::filesystem::path GetRoot() const;
    bool ServesEmbedded() const;

    AdminResponse Serve(AdminRequest const& request) const;

    static std::optional<std::string_view> ContentType(std::filesystem::path const& file);
    static std::optional<std::filesystem::path> Resolve(std::filesystem::path const& root, std::string_view path);
    static std::optional<std::string> Relative(std::string_view path);

private:
    mutable std::shared_mutex _mutex;
    std::filesystem::path _root;
    EmbeddedPage const* _embedded = nullptr;
};

#endif

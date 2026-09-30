/*
 * Project Ambrose by Imjustchico
 * A built web page compiled into a program, as the list the ambrose_embed_page build step generates: each file's path, media type, entity tag and bytes, with the fingerprinted assets marked. It answers a path the way both desktop web views and the supervisor serve one: the query and fragment dropped, the root and any unknown path outside the API prefix given index.html so the page's own routes load, an unknown path under the API prefix given 404 so a mistyped endpoint never answers HTML, an immutable cache header only on hashed assets, and a content policy on every answer that lets the page reach nothing but its own origin, so a program's own page makes no request to the network.
 */

#ifndef AMBROSE_EMBEDDEDPAGE_H
#define AMBROSE_EMBEDDEDPAGE_H

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct EmbeddedFile
{
    std::string_view Path;
    std::string_view MediaType;
    std::string_view EntityTag;
    std::string_view Bytes;
    bool Hashed = false;
};

struct EmbeddedAnswer
{
    int Status = 200;
    EmbeddedFile const* File = nullptr;
    std::vector<std::pair<std::string, std::string>> Headers;
};

class EmbeddedPage
{
public:
    static constexpr std::string_view Index = "index.html";
    static constexpr std::string_view ApiPrefix = "/api/";
    static constexpr std::string_view ImmutableCache = "public, max-age=31536000, immutable";
    static constexpr std::string_view RevalidateCache = "no-cache";
    static constexpr std::string_view ContentPolicy = "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self'; connect-src 'self'; frame-src 'none'; object-src 'none'; base-uri 'none'; form-action 'self'";

    explicit EmbeddedPage(std::span<EmbeddedFile const> files, bool placeholder = false);

    std::span<EmbeddedFile const> Files() const { return _files; }
    bool IsPlaceholder() const { return _placeholder; }

    EmbeddedFile const* Find(std::string_view path) const;
    EmbeddedAnswer Serve(std::string_view target) const;

    static std::string_view PathOf(std::string_view target);

private:
    std::span<EmbeddedFile const> _files;
    bool _placeholder = false;
};

#endif

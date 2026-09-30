/*
 * Project Ambrose by Imjustchico
 * Looks a request up in a compiled-in page: a path matches a file only exactly and with no parent segment, the root and unknown paths outside the API prefix fall back to index.html, the API prefix never does, and the answer carries the content policy that keeps the page on its own origin and the file's entity tag, with an immutable cache header for hashed assets and a revalidating one for everything else.
 */

#include "EmbeddedPage.h"

#include <algorithm>
#include <string>

EmbeddedPage::EmbeddedPage(std::span<EmbeddedFile const> files, bool placeholder) : _files(files), _placeholder(placeholder)
{
}

std::string_view EmbeddedPage::PathOf(std::string_view target)
{
    std::size_t const end = target.find_first_of("?#");
    if (end != std::string_view::npos)
        target = target.substr(0, end);
    if (target.empty())
        return "/";
    return target;
}

EmbeddedFile const* EmbeddedPage::Find(std::string_view path) const
{
    if (path.starts_with('/'))
        path.remove_prefix(1);
    if (path.empty())
        path = Index;
    if (path.find("..") != std::string_view::npos || path.find('\\') != std::string_view::npos)
        return nullptr;
    auto const found = std::find_if(_files.begin(), _files.end(), [&](EmbeddedFile const& file) { return file.Path == path; });
    return found == _files.end() ? nullptr : &*found;
}

EmbeddedAnswer EmbeddedPage::Serve(std::string_view target) const
{
    std::string_view const path = PathOf(target);
    EmbeddedAnswer answer;
    answer.File = Find(path);
    if (answer.File == nullptr)
    {
        if (path.starts_with(ApiPrefix) || path == ApiPrefix.substr(0, ApiPrefix.size() - 1))
        {
            answer.Status = 404;
            return answer;
        }
        answer.File = Find(Index);
        if (answer.File == nullptr)
        {
            answer.Status = 404;
            return answer;
        }
    }
    answer.Headers.emplace_back("Content-Type", std::string(answer.File->MediaType));
    answer.Headers.emplace_back("Cache-Control", std::string(answer.File->Hashed ? ImmutableCache : RevalidateCache));
    if (!answer.File->EntityTag.empty())
        answer.Headers.emplace_back("ETag", std::string(answer.File->EntityTag));
    answer.Headers.emplace_back("Content-Security-Policy", std::string(ContentPolicy));
    return answer;
}

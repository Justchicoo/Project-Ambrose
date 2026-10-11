/*
 * Project Ambrose by Imjustchico
 * The patch service id and the PATCH messages the patch server decodes or sends, declared by tag with only the fields it reads or sets.
 */

#ifndef AMBROSE_PATCHMESSAGES_H
#define AMBROSE_PATCHMESSAGES_H

#include "MessageDeclaration.h"

#include <string>
#include <string_view>
#include <tuple>

namespace PatchMessages
{
    inline constexpr uint8 PatchService = 8;

    struct LatestFileList
    {
        static constexpr uint8 ServiceId = PatchService;
        static constexpr std::string_view Tag = "MSG_LATEST_FILE_LIST";

        uint32 LatestVersion = 0;
        std::string ListFileName;
        uint32 ListFileType = 0;
        uint32 ListFileTime = 0;
        uint32 ListFileSize = 0;
        uint32 ListFileCrc = 0;
        std::string ListFileUrl;
        std::string UrlPrefix;
        std::string UrlSuffix;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("LatestVersion", &LatestFileList::LatestVersion), DmlField("ListFileName", &LatestFileList::ListFileName),
                DmlField("ListFileType", &LatestFileList::ListFileType), DmlField("ListFileTime", &LatestFileList::ListFileTime),
                DmlField("ListFileSize", &LatestFileList::ListFileSize), DmlField("ListFileCRC", &LatestFileList::ListFileCrc), DmlField("ListFileURL", &LatestFileList::ListFileUrl),
                DmlField("URLPrefix", &LatestFileList::UrlPrefix), DmlField("URLSuffix", &LatestFileList::UrlSuffix) };
        }
    };

    struct LatestFileListV2
    {
        static constexpr uint8 ServiceId = PatchService;
        static constexpr std::string_view Tag = "MSG_LATEST_FILE_LIST_V2";

        uint32 LatestVersion = 0;
        std::string ListFileName;
        uint32 ListFileType = 0;
        uint32 ListFileTime = 0;
        uint32 ListFileSize = 0;
        uint32 ListFileCrc = 0;
        std::string ListFileUrl;
        std::string UrlPrefix;
        std::string UrlSuffix;
        std::string Locale;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("LatestVersion", &LatestFileListV2::LatestVersion), DmlField("ListFileName", &LatestFileListV2::ListFileName),
                DmlField("ListFileType", &LatestFileListV2::ListFileType), DmlField("ListFileTime", &LatestFileListV2::ListFileTime),
                DmlField("ListFileSize", &LatestFileListV2::ListFileSize), DmlField("ListFileCRC", &LatestFileListV2::ListFileCrc),
                DmlField("ListFileURL", &LatestFileListV2::ListFileUrl), DmlField("URLPrefix", &LatestFileListV2::UrlPrefix), DmlField("URLSuffix", &LatestFileListV2::UrlSuffix),
                DmlField("Locale", &LatestFileListV2::Locale) };
        }
    };
}

#endif

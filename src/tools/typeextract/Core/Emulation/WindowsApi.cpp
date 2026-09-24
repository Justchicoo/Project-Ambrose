/*
 * Project Ambrose by Imjustchico
 * Handlers for the Windows kernel functions the guest runtime uses, with deterministic time, process values and module paths under C:\Wizard101\Bin, code page 1252, 437, ASCII and UTF-8 conversion that replaces broken sequences as Windows does, character types and case mapping from Windows' own tables for every character code page 1252 reaches, one-time initialization through a redirected callback, and file and exit requests refused as a Windows process without those resources would see them.
 */

#include "WindowsApi.h"
#include "GuestProcess.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr uint64 ProcessHeapHandle = 0x1000;
    constexpr uint64 FirstHandle = 0x5000;
    constexpr uint64 ProcessId = 0x1234;
    constexpr uint64 ThreadId = 0x1238;
    constexpr uint64 FileTime = 133500000000000000ull;
    constexpr uint64 PerformanceCounter = 123456789;
    constexpr uint64 PerformanceFrequency = 10000000;
    constexpr uint64 TickCount = 100000;
    constexpr uint64 InvalidHandle = 0xFFFFFFFFFFFFFFFFull;
    constexpr uint32 AnsiCodePage = 1252;
    constexpr uint32 OemCodePage = 437;
    constexpr uint32 ErrorFileNotFound = 2;
    constexpr uint32 ErrorModNotFound = 126;
    constexpr uint32 ErrorProcNotFound = 127;
    constexpr uint32 ErrorInsufficientBuffer = 122;
    constexpr uint32 ErrorEnvVarNotFound = 203;
    constexpr uint32 ErrorNoUnicodeTranslation = 1113;
    constexpr uint32 ErrorInvalidParameter = 87;
    constexpr uint32 MbErrInvalidChars = 8;
    constexpr uint32 LcmapLowercase = 0x100;
    constexpr uint32 LcmapUppercase = 0x200;
    constexpr uint32 LocaleUserDefault = 0x409;
    constexpr uint16 DefinedAlphaType = 0x300;
    constexpr uint64 TicksPerMillisecond = 10000;
    constexpr uint64 MillisecondsPerDay = 86400000;
    constexpr uint64 DaysFromMarchYearZeroTo1601 = 584694;
    constexpr uint64 DaysPerEra = 146097;
    constexpr std::size_t TimeZoneInformationSize = 172;
    constexpr std::size_t DynamicTimeZoneInformationSize = 432;
    constexpr std::u16string_view GuestProgramFolder = u"C:\\Wizard101\\Bin\\";
    constexpr std::u16string_view KernelModulePath = u"C:\\WINDOWS\\System32\\KERNEL32.DLL";

    struct CharacterInfo
    {
        uint16 Type = 0;
        char16_t Lower = 0;
        char16_t Upper = 0;
    };

    struct WideCharacterInfo
    {
        char16_t Code = 0;
        CharacterInfo Info;
    };

    constexpr std::array<CharacterInfo, 256> Latin1Characters = { {
        { 0x220, 0x00, 0x00 }, { 0x220, 0x01, 0x01 }, { 0x220, 0x02, 0x02 }, { 0x220, 0x03, 0x03 }, { 0x220, 0x04, 0x04 }, { 0x220, 0x05, 0x05 }, { 0x220, 0x06, 0x06 }, { 0x220, 0x07, 0x07 },
        { 0x220, 0x08, 0x08 }, { 0x268, 0x09, 0x09 }, { 0x228, 0x0A, 0x0A }, { 0x228, 0x0B, 0x0B }, { 0x228, 0x0C, 0x0C }, { 0x228, 0x0D, 0x0D }, { 0x220, 0x0E, 0x0E }, { 0x220, 0x0F, 0x0F },
        { 0x220, 0x10, 0x10 }, { 0x220, 0x11, 0x11 }, { 0x220, 0x12, 0x12 }, { 0x220, 0x13, 0x13 }, { 0x220, 0x14, 0x14 }, { 0x220, 0x15, 0x15 }, { 0x220, 0x16, 0x16 }, { 0x220, 0x17, 0x17 },
        { 0x220, 0x18, 0x18 }, { 0x220, 0x19, 0x19 }, { 0x220, 0x1A, 0x1A }, { 0x220, 0x1B, 0x1B }, { 0x220, 0x1C, 0x1C }, { 0x220, 0x1D, 0x1D }, { 0x220, 0x1E, 0x1E }, { 0x220, 0x1F, 0x1F },
        { 0x248, 0x20, 0x20 }, { 0x210, 0x21, 0x21 }, { 0x210, 0x22, 0x22 }, { 0x210, 0x23, 0x23 }, { 0x210, 0x24, 0x24 }, { 0x210, 0x25, 0x25 }, { 0x210, 0x26, 0x26 }, { 0x210, 0x27, 0x27 },
        { 0x210, 0x28, 0x28 }, { 0x210, 0x29, 0x29 }, { 0x210, 0x2A, 0x2A }, { 0x210, 0x2B, 0x2B }, { 0x210, 0x2C, 0x2C }, { 0x210, 0x2D, 0x2D }, { 0x210, 0x2E, 0x2E }, { 0x210, 0x2F, 0x2F },
        { 0x284, 0x30, 0x30 }, { 0x284, 0x31, 0x31 }, { 0x284, 0x32, 0x32 }, { 0x284, 0x33, 0x33 }, { 0x284, 0x34, 0x34 }, { 0x284, 0x35, 0x35 }, { 0x284, 0x36, 0x36 }, { 0x284, 0x37, 0x37 },
        { 0x284, 0x38, 0x38 }, { 0x284, 0x39, 0x39 }, { 0x210, 0x3A, 0x3A }, { 0x210, 0x3B, 0x3B }, { 0x210, 0x3C, 0x3C }, { 0x210, 0x3D, 0x3D }, { 0x210, 0x3E, 0x3E }, { 0x210, 0x3F, 0x3F },
        { 0x210, 0x40, 0x40 }, { 0x381, 0x61, 0x41 }, { 0x381, 0x62, 0x42 }, { 0x381, 0x63, 0x43 }, { 0x381, 0x64, 0x44 }, { 0x381, 0x65, 0x45 }, { 0x381, 0x66, 0x46 }, { 0x301, 0x67, 0x47 },
        { 0x301, 0x68, 0x48 }, { 0x301, 0x69, 0x49 }, { 0x301, 0x6A, 0x4A }, { 0x301, 0x6B, 0x4B }, { 0x301, 0x6C, 0x4C }, { 0x301, 0x6D, 0x4D }, { 0x301, 0x6E, 0x4E }, { 0x301, 0x6F, 0x4F },
        { 0x301, 0x70, 0x50 }, { 0x301, 0x71, 0x51 }, { 0x301, 0x72, 0x52 }, { 0x301, 0x73, 0x53 }, { 0x301, 0x74, 0x54 }, { 0x301, 0x75, 0x55 }, { 0x301, 0x76, 0x56 }, { 0x301, 0x77, 0x57 },
        { 0x301, 0x78, 0x58 }, { 0x301, 0x79, 0x59 }, { 0x301, 0x7A, 0x5A }, { 0x210, 0x5B, 0x5B }, { 0x210, 0x5C, 0x5C }, { 0x210, 0x5D, 0x5D }, { 0x210, 0x5E, 0x5E }, { 0x210, 0x5F, 0x5F },
        { 0x210, 0x60, 0x60 }, { 0x382, 0x61, 0x41 }, { 0x382, 0x62, 0x42 }, { 0x382, 0x63, 0x43 }, { 0x382, 0x64, 0x44 }, { 0x382, 0x65, 0x45 }, { 0x382, 0x66, 0x46 }, { 0x302, 0x67, 0x47 },
        { 0x302, 0x68, 0x48 }, { 0x302, 0x69, 0x49 }, { 0x302, 0x6A, 0x4A }, { 0x302, 0x6B, 0x4B }, { 0x302, 0x6C, 0x4C }, { 0x302, 0x6D, 0x4D }, { 0x302, 0x6E, 0x4E }, { 0x302, 0x6F, 0x4F },
        { 0x302, 0x70, 0x50 }, { 0x302, 0x71, 0x51 }, { 0x302, 0x72, 0x52 }, { 0x302, 0x73, 0x53 }, { 0x302, 0x74, 0x54 }, { 0x302, 0x75, 0x55 }, { 0x302, 0x76, 0x56 }, { 0x302, 0x77, 0x57 },
        { 0x302, 0x78, 0x58 }, { 0x302, 0x79, 0x59 }, { 0x302, 0x7A, 0x5A }, { 0x210, 0x7B, 0x7B }, { 0x210, 0x7C, 0x7C }, { 0x210, 0x7D, 0x7D }, { 0x210, 0x7E, 0x7E }, { 0x220, 0x7F, 0x7F },
        { 0x220, 0x80, 0x80 }, { 0x220, 0x81, 0x81 }, { 0x220, 0x82, 0x82 }, { 0x220, 0x83, 0x83 }, { 0x220, 0x84, 0x84 }, { 0x228, 0x85, 0x85 }, { 0x220, 0x86, 0x86 }, { 0x220, 0x87, 0x87 },
        { 0x220, 0x88, 0x88 }, { 0x220, 0x89, 0x89 }, { 0x220, 0x8A, 0x8A }, { 0x220, 0x8B, 0x8B }, { 0x220, 0x8C, 0x8C }, { 0x220, 0x8D, 0x8D }, { 0x220, 0x8E, 0x8E }, { 0x220, 0x8F, 0x8F },
        { 0x220, 0x90, 0x90 }, { 0x220, 0x91, 0x91 }, { 0x220, 0x92, 0x92 }, { 0x220, 0x93, 0x93 }, { 0x220, 0x94, 0x94 }, { 0x220, 0x95, 0x95 }, { 0x220, 0x96, 0x96 }, { 0x220, 0x97, 0x97 },
        { 0x220, 0x98, 0x98 }, { 0x220, 0x99, 0x99 }, { 0x220, 0x9A, 0x9A }, { 0x220, 0x9B, 0x9B }, { 0x220, 0x9C, 0x9C }, { 0x220, 0x9D, 0x9D }, { 0x220, 0x9E, 0x9E }, { 0x220, 0x9F, 0x9F },
        { 0x248, 0xA0, 0xA0 }, { 0x210, 0xA1, 0xA1 }, { 0x210, 0xA2, 0xA2 }, { 0x210, 0xA3, 0xA3 }, { 0x210, 0xA4, 0xA4 }, { 0x210, 0xA5, 0xA5 }, { 0x210, 0xA6, 0xA6 }, { 0x210, 0xA7, 0xA7 },
        { 0x210, 0xA8, 0xA8 }, { 0x210, 0xA9, 0xA9 }, { 0x312, 0xAA, 0xAA }, { 0x210, 0xAB, 0xAB }, { 0x210, 0xAC, 0xAC }, { 0x230, 0xAD, 0xAD }, { 0x210, 0xAE, 0xAE }, { 0x210, 0xAF, 0xAF },
        { 0x210, 0xB0, 0xB0 }, { 0x210, 0xB1, 0xB1 }, { 0x214, 0xB2, 0xB2 }, { 0x214, 0xB3, 0xB3 }, { 0x210, 0xB4, 0xB4 }, { 0x312, 0xB5, 0xB5 }, { 0x210, 0xB6, 0xB6 }, { 0x210, 0xB7, 0xB7 },
        { 0x210, 0xB8, 0xB8 }, { 0x214, 0xB9, 0xB9 }, { 0x312, 0xBA, 0xBA }, { 0x210, 0xBB, 0xBB }, { 0x210, 0xBC, 0xBC }, { 0x210, 0xBD, 0xBD }, { 0x210, 0xBE, 0xBE }, { 0x210, 0xBF, 0xBF },
        { 0x301, 0xE0, 0xC0 }, { 0x301, 0xE1, 0xC1 }, { 0x301, 0xE2, 0xC2 }, { 0x301, 0xE3, 0xC3 }, { 0x301, 0xE4, 0xC4 }, { 0x301, 0xE5, 0xC5 }, { 0x301, 0xE6, 0xC6 }, { 0x301, 0xE7, 0xC7 },
        { 0x301, 0xE8, 0xC8 }, { 0x301, 0xE9, 0xC9 }, { 0x301, 0xEA, 0xCA }, { 0x301, 0xEB, 0xCB }, { 0x301, 0xEC, 0xCC }, { 0x301, 0xED, 0xCD }, { 0x301, 0xEE, 0xCE }, { 0x301, 0xEF, 0xCF },
        { 0x301, 0xF0, 0xD0 }, { 0x301, 0xF1, 0xD1 }, { 0x301, 0xF2, 0xD2 }, { 0x301, 0xF3, 0xD3 }, { 0x301, 0xF4, 0xD4 }, { 0x301, 0xF5, 0xD5 }, { 0x301, 0xF6, 0xD6 }, { 0x210, 0xD7, 0xD7 },
        { 0x301, 0xF8, 0xD8 }, { 0x301, 0xF9, 0xD9 }, { 0x301, 0xFA, 0xDA }, { 0x301, 0xFB, 0xDB }, { 0x301, 0xFC, 0xDC }, { 0x301, 0xFD, 0xDD }, { 0x301, 0xFE, 0xDE }, { 0x302, 0xDF, 0xDF },
        { 0x302, 0xE0, 0xC0 }, { 0x302, 0xE1, 0xC1 }, { 0x302, 0xE2, 0xC2 }, { 0x302, 0xE3, 0xC3 }, { 0x302, 0xE4, 0xC4 }, { 0x302, 0xE5, 0xC5 }, { 0x302, 0xE6, 0xC6 }, { 0x302, 0xE7, 0xC7 },
        { 0x302, 0xE8, 0xC8 }, { 0x302, 0xE9, 0xC9 }, { 0x302, 0xEA, 0xCA }, { 0x302, 0xEB, 0xCB }, { 0x302, 0xEC, 0xCC }, { 0x302, 0xED, 0xCD }, { 0x302, 0xEE, 0xCE }, { 0x302, 0xEF, 0xCF },
        { 0x302, 0xF0, 0xD0 }, { 0x302, 0xF1, 0xD1 }, { 0x302, 0xF2, 0xD2 }, { 0x302, 0xF3, 0xD3 }, { 0x302, 0xF4, 0xD4 }, { 0x302, 0xF5, 0xD5 }, { 0x302, 0xF6, 0xD6 }, { 0x210, 0xF7, 0xF7 },
        { 0x302, 0xF8, 0xD8 }, { 0x302, 0xF9, 0xD9 }, { 0x302, 0xFA, 0xDA }, { 0x302, 0xFB, 0xDB }, { 0x302, 0xFC, 0xDC }, { 0x302, 0xFD, 0xDD }, { 0x302, 0xFE, 0xDE }, { 0x302, 0xFF, 0x0178 }
    } };

    constexpr std::array<WideCharacterInfo, 27> Cp1252WideCharacters = { {
        { 0x0152, { 0x301, 0x0153, 0x0152 } }, { 0x0153, { 0x302, 0x0153, 0x0152 } }, { 0x0160, { 0x301, 0x0161, 0x0160 } }, { 0x0161, { 0x302, 0x0161, 0x0160 } },
        { 0x0178, { 0x301, 0x00FF, 0x0178 } }, { 0x017D, { 0x301, 0x017E, 0x017D } }, { 0x017E, { 0x302, 0x017E, 0x017D } }, { 0x0192, { 0x302, 0x0192, 0x0191 } },
        { 0x02C6, { 0x200, 0x02C6, 0x02C6 } }, { 0x02DC, { 0x200, 0x02DC, 0x02DC } }, { 0x2013, { 0x210, 0x2013, 0x2013 } }, { 0x2014, { 0x210, 0x2014, 0x2014 } },
        { 0x2018, { 0x210, 0x2018, 0x2018 } }, { 0x2019, { 0x210, 0x2019, 0x2019 } }, { 0x201A, { 0x210, 0x201A, 0x201A } }, { 0x201C, { 0x210, 0x201C, 0x201C } },
        { 0x201D, { 0x210, 0x201D, 0x201D } }, { 0x201E, { 0x210, 0x201E, 0x201E } }, { 0x2020, { 0x210, 0x2020, 0x2020 } }, { 0x2021, { 0x210, 0x2021, 0x2021 } },
        { 0x2022, { 0x210, 0x2022, 0x2022 } }, { 0x2026, { 0x210, 0x2026, 0x2026 } }, { 0x2030, { 0x210, 0x2030, 0x2030 } }, { 0x2039, { 0x210, 0x2039, 0x2039 } },
        { 0x203A, { 0x210, 0x203A, 0x203A } }, { 0x20AC, { 0x200, 0x20AC, 0x20AC } }, { 0x2122, { 0x200, 0x2122, 0x2122 } }
    } };

    constexpr std::array<char16_t, 32> Cp1252High = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
    };

    constexpr std::array<char16_t, 128> Cp437High = {
        0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
        0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
        0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
        0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
        0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
        0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
        0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
        0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
    };

    struct ApiState
    {
        uint32 lastError = 0;
        uint32 nextTls = 1;
        std::unordered_map<uint32, uint64> tls;
        uint64 nextHandle = FirstHandle;
        uint64 nextHeap = ProcessHeapHandle + 0x10;
        std::unordered_map<std::string, uint64> statics;
    };

    uint32 ResolveCodePage(uint32 codePage)
    {
        switch (codePage)
        {
            case 0:
            case 3:
                return AnsiCodePage;
            case 1:
            case 2:
                return OemCodePage;
            default:
                return codePage;
        }
    }

    bool IsSupportedCodePage(uint32 codePage)
    {
        uint32 const resolved = ResolveCodePage(codePage);
        return resolved == 1252 || resolved == 437 || resolved == 20127 || resolved == 65001 || resolved == 28591;
    }

    std::u16string DecodeBytes(uint32 codePage, std::vector<uint8> const& bytes, bool& invalid)
    {
        std::u16string out;
        uint32 const resolved = ResolveCodePage(codePage);
        if (resolved != 65001)
        {
            for (uint8 const b : bytes)
            {
                if (b < 0x80)
                    out.push_back(static_cast<char16_t>(b));
                else if (resolved == 1252 && b < 0xA0)
                    out.push_back(Cp1252High[b - 0x80]);
                else if (resolved == 437)
                    out.push_back(Cp437High[b - 0x80]);
                else if (resolved == 20127)
                {
                    out.push_back(u'?');
                    invalid = true;
                }
                else
                    out.push_back(static_cast<char16_t>(b));
            }
            return out;
        }
        std::size_t i = 0;
        while (i < bytes.size())
        {
            uint8 const lead = bytes[i];
            if (lead < 0x80)
            {
                out.push_back(static_cast<char16_t>(lead));
                ++i;
                continue;
            }
            std::size_t extra = 0;
            uint32 codePoint = 0;
            uint8 low = 0x80;
            uint8 high = 0xBF;
            if (lead >= 0xC2 && lead <= 0xDF)
            {
                extra = 1;
                codePoint = lead & 0x1F;
            }
            else if (lead >= 0xE0 && lead <= 0xEF)
            {
                extra = 2;
                codePoint = lead & 0x0F;
                low = static_cast<uint8>(lead == 0xE0 ? 0xA0 : 0x80);
                high = static_cast<uint8>(lead == 0xED ? 0x9F : 0xBF);
            }
            else if (lead >= 0xF0 && lead <= 0xF4)
            {
                extra = 3;
                codePoint = lead & 0x07;
                low = static_cast<uint8>(lead == 0xF0 ? 0x90 : 0x80);
                high = static_cast<uint8>(lead == 0xF4 ? 0x8F : 0xBF);
            }
            else
            {
                out.push_back(0xFFFD);
                invalid = true;
                ++i;
                continue;
            }
            std::size_t used = 1;
            bool outOfRange = false;
            while (used <= extra && i + used < bytes.size() && (bytes[i + used] & 0xC0) == 0x80)
            {
                uint8 const next = bytes[i + used];
                ++used;
                if (used == 2 && (next < low || next > high))
                {
                    outOfRange = true;
                    break;
                }
                codePoint = (codePoint << 6) | (next & 0x3F);
            }
            i += used;
            if (outOfRange || used <= extra)
            {
                out.push_back(0xFFFD);
                invalid = true;
                continue;
            }
            if (codePoint >= 0x10000)
            {
                codePoint -= 0x10000;
                out.push_back(static_cast<char16_t>(0xD800 + (codePoint >> 10)));
                out.push_back(static_cast<char16_t>(0xDC00 + (codePoint & 0x3FF)));
            }
            else
                out.push_back(static_cast<char16_t>(codePoint));
        }
        return out;
    }

    std::vector<uint8> EncodeText(uint32 codePage, std::u16string const& text, bool& usedDefault)
    {
        std::vector<uint8> out;
        uint32 const resolved = ResolveCodePage(codePage);
        if (resolved == 65001)
        {
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                uint32 codePoint = text[i];
                if (codePoint >= 0xD800 && codePoint <= 0xDBFF && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
                {
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (text[i + 1] - 0xDC00);
                    ++i;
                }
                else if (codePoint >= 0xD800 && codePoint <= 0xDFFF)
                {
                    codePoint = 0xFFFD;
                    usedDefault = true;
                }
                if (codePoint < 0x80)
                    out.push_back(static_cast<uint8>(codePoint));
                else if (codePoint < 0x800)
                {
                    out.push_back(static_cast<uint8>(0xC0 | (codePoint >> 6)));
                    out.push_back(static_cast<uint8>(0x80 | (codePoint & 0x3F)));
                }
                else if (codePoint < 0x10000)
                {
                    out.push_back(static_cast<uint8>(0xE0 | (codePoint >> 12)));
                    out.push_back(static_cast<uint8>(0x80 | ((codePoint >> 6) & 0x3F)));
                    out.push_back(static_cast<uint8>(0x80 | (codePoint & 0x3F)));
                }
                else
                {
                    out.push_back(static_cast<uint8>(0xF0 | (codePoint >> 18)));
                    out.push_back(static_cast<uint8>(0x80 | ((codePoint >> 12) & 0x3F)));
                    out.push_back(static_cast<uint8>(0x80 | ((codePoint >> 6) & 0x3F)));
                    out.push_back(static_cast<uint8>(0x80 | (codePoint & 0x3F)));
                }
            }
            return out;
        }
        for (char16_t const c : text)
        {
            if (c < 0x80)
            {
                out.push_back(static_cast<uint8>(c));
                continue;
            }
            int found = -1;
            if (resolved == 1252)
            {
                for (std::size_t k = 0; k < Cp1252High.size(); ++k)
                    if (Cp1252High[k] == c)
                        found = static_cast<int>(0x80 + k);
                if (found < 0 && c >= 0xA0 && c <= 0xFF)
                    found = c;
            }
            else if (resolved == 437)
            {
                for (std::size_t k = 0; k < Cp437High.size(); ++k)
                    if (Cp437High[k] == c)
                        found = static_cast<int>(0x80 + k);
            }
            else if (resolved == 28591 && c <= 0xFF)
                found = c;
            if (found < 0)
            {
                out.push_back('?');
                usedDefault = true;
            }
            else
                out.push_back(static_cast<uint8>(found));
        }
        return out;
    }

    CharacterInfo DescribeCharacter(char16_t c)
    {
        if (c < Latin1Characters.size())
            return Latin1Characters[c];
        auto const found = std::lower_bound(Cp1252WideCharacters.begin(), Cp1252WideCharacters.end(), c, [](WideCharacterInfo const& entry, char16_t code) { return entry.Code < code; });
        if (found != Cp1252WideCharacters.end() && found->Code == c)
            return found->Info;
        return CharacterInfo{ DefinedAlphaType, c, c };
    }

    uint16 CharType1(char16_t c)
    {
        return DescribeCharacter(c).Type;
    }

    char16_t Lower16(char16_t c)
    {
        return DescribeCharacter(c).Lower;
    }

    char16_t Upper16(char16_t c)
    {
        return DescribeCharacter(c).Upper;
    }

    std::array<uint16, 8> SystemTimeOf(uint64 fileTime)
    {
        uint64 const milliseconds = fileTime / TicksPerMillisecond;
        uint64 const days = milliseconds / MillisecondsPerDay;
        uint64 const clock = milliseconds % MillisecondsPerDay;
        uint64 const shifted = days + DaysFromMarchYearZeroTo1601;
        uint64 const era = shifted / DaysPerEra;
        uint64 const dayOfEra = shifted - era * DaysPerEra;
        uint64 const yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
        uint64 const dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
        uint64 const monthFromMarch = (5 * dayOfYear + 2) / 153;
        uint64 const day = dayOfYear - (153 * monthFromMarch + 2) / 5 + 1;
        uint64 const month = monthFromMarch < 10 ? monthFromMarch + 3 : monthFromMarch - 9;
        uint64 const year = era * 400 + yearOfEra + (month <= 2 ? 1 : 0);
        return {
            static_cast<uint16>(year), static_cast<uint16>(month), static_cast<uint16>((days + 1) % 7), static_cast<uint16>(day),
            static_cast<uint16>(clock / 3600000), static_cast<uint16>(clock / 60000 % 60), static_cast<uint16>(clock / 1000 % 60), static_cast<uint16>(clock % 1000)
        };
    }

    std::optional<std::u16string> GuestModulePath(GuestProcess& process, uint64 handle)
    {
        if (handle == GuestProcess::KernelModuleHandle)
            return std::u16string(KernelModulePath);
        GuestModule const* module = handle ? nullptr : &process.GetMain();
        for (GuestModule const* const candidate : process.GetModules())
            if (candidate->Base == handle)
                module = candidate;
        if (!module)
            return std::nullopt;
        return std::u16string(GuestProgramFolder) + module->Path.filename().u16string();
    }

    uint64 Arg(GuestProcess& process, std::size_t index)
    {
        return process.GetMachine().GetArgument(index);
    }

    std::u16string ReadWide(GuestProcess& process, uint64 address, uint64 length)
    {
        Machine& machine = process.GetMachine();
        if (static_cast<int32>(static_cast<uint32>(length)) < 0)
        {
            std::optional<std::u16string> text = machine.ReadWideString(address, 1u << 20);
            if (!text)
                throw EmulationError(fmt::format("a wide string at {:#x} is unreadable", address));
            text->push_back(0);
            return *text;
        }
        std::u16string out;
        std::vector<uint8> const bytes = machine.ReadBytes(address, static_cast<std::size_t>(length) * 2);
        for (std::size_t i = 0; i + 1 < bytes.size(); i += 2)
            out.push_back(static_cast<char16_t>(bytes[i] | (bytes[i + 1] << 8)));
        return out;
    }

    std::vector<uint8> ReadNarrow(GuestProcess& process, uint64 address, uint64 length)
    {
        Machine& machine = process.GetMachine();
        if (static_cast<int32>(static_cast<uint32>(length)) < 0)
        {
            std::optional<std::string> text = machine.ReadCString(address, 1u << 20);
            if (!text)
                throw EmulationError(fmt::format("a string at {:#x} is unreadable", address));
            std::vector<uint8> bytes(text->begin(), text->end());
            bytes.push_back(0);
            return bytes;
        }
        return machine.ReadBytes(address, static_cast<std::size_t>(length));
    }

    void WriteWide(Machine& machine, uint64 address, std::u16string_view text)
    {
        std::vector<uint8> bytes;
        bytes.reserve(text.size() * 2);
        for (char16_t const c : text)
        {
            bytes.push_back(static_cast<uint8>(c));
            bytes.push_back(static_cast<uint8>(c >> 8));
        }
        if (!bytes.empty())
            machine.Write(address, bytes);
    }

    bool IsKernelModuleName(std::string_view name)
    {
        std::string key;
        for (char const c : name)
            key.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
        std::size_t const slash = key.find_last_of("/\\");
        if (slash != std::string::npos)
            key.erase(0, slash + 1);
        if (key.find('.') == std::string::npos)
            key += ".dll";
        return key == "kernel32.dll" || key == "kernelbase.dll" || key == "ntdll.dll" || key.starts_with("api-ms-win-core-");
    }

    std::string ModuleName(GuestProcess& process, uint64 address, bool wide)
    {
        Machine& machine = process.GetMachine();
        if (wide)
        {
            std::optional<std::u16string> const text = machine.ReadWideString(address, 1024);
            if (!text)
                return {};
            std::string out;
            for (char16_t const c : *text)
                out.push_back(static_cast<char>(c < 0x80 ? c : '?'));
            return out;
        }
        return machine.ReadCString(address, 1024).value_or(std::string{});
    }
}

void WindowsApi::Register(GuestProcess& process)
{
    auto state = std::make_shared<ApiState>();
    auto constant = [&process](std::string_view name, uint64 value)
    {
        process.RegisterApi(name, [value](GuestProcess&) { return value; });
    };

    for (std::string_view const name : { "HeapValidate", "HeapSetInformation", "HeapLock", "HeapUnlock", "HeapDestroy", "VirtualFree", "FreeLibrary", "DisableThreadLibraryCalls",
             "InitializeCriticalSectionAndSpinCount", "InitializeCriticalSectionEx", "TryEnterCriticalSection", "TryAcquireSRWLockExclusive", "TryAcquireSRWLockShared",
             "SleepConditionVariableSRW", "SleepConditionVariableCS", "SetEvent", "ResetEvent", "CloseHandle", "SwitchToThread", "FlushFileBuffers", "FreeEnvironmentStringsW",
             "FreeEnvironmentStringsA", "SetEnvironmentVariableW", "SetEnvironmentVariableA", "IsValidLocale", "IsValidLocaleName", "EnumSystemLocalesW", "EnumSystemLocalesEx",
             "SetConsoleCtrlHandler", "SetThreadStackGuarantee", "AreFileApisANSI", "QueryDepthSList" })
        constant(name, 1);
    for (std::string_view const name : { "HeapQueryInformation", "EnterCriticalSection", "LeaveCriticalSection", "DeleteCriticalSection", "SetCriticalSectionSpinCount",
             "AcquireSRWLockExclusive", "AcquireSRWLockShared", "ReleaseSRWLockExclusive", "ReleaseSRWLockShared", "WakeAllConditionVariable", "WakeConditionVariable",
             "WaitForSingleObject", "WaitForSingleObjectEx", "WaitForMultipleObjects", "Sleep", "SleepEx", "IsDebuggerPresent", "SetUnhandledExceptionFilter",
             "UnhandledExceptionFilter", "OutputDebugStringA", "OutputDebugStringW", "GetConsoleMode", "SetErrorMode", "GetErrorMode", "RtlLookupFunctionEntry",
             "GetProcessHeaps", "VirtualQuery", "GetLocaleInfoW", "GetLocaleInfoEx", "GetStdHandle", "GetFileType" })
        constant(name, 0);
    constant("GetProcessHeap", ProcessHeapHandle);
    constant("GetCurrentProcess", InvalidHandle);
    constant("GetCurrentThread", InvalidHandle - 1);
    constant("GetCurrentProcessId", ProcessId);
    constant("GetCurrentThreadId", ThreadId);
    constant("GetProcessId", ProcessId);
    constant("GetACP", AnsiCodePage);
    constant("GetOEMCP", OemCodePage);
    constant("GetConsoleCP", AnsiCodePage);
    constant("GetConsoleOutputCP", AnsiCodePage);
    constant("GetUserDefaultLCID", LocaleUserDefault);
    constant("GetSystemDefaultLCID", LocaleUserDefault);
    constant("GetThreadLocale", LocaleUserDefault);
    constant("LocaleNameToLCID", LocaleUserDefault);
    constant("GetTickCount", TickCount);
    constant("GetTickCount64", TickCount);
    constant("timeGetTime", TickCount);

    process.RegisterApi("GetLastError", [state](GuestProcess&) { return uint64{ state->lastError }; });
    process.RegisterApi("SetLastError", [state](GuestProcess& p) { state->lastError = static_cast<uint32>(Arg(p, 0)); return uint64{ 0 }; });
    process.RegisterApi("RestoreLastError", [state](GuestProcess& p) { state->lastError = static_cast<uint32>(Arg(p, 0)); return uint64{ 0 }; });

    process.RegisterApi("HeapCreate", [state](GuestProcess&) { uint64 const handle = state->nextHeap; state->nextHeap += 0x10; return handle; });
    process.RegisterApi("HeapAlloc", [](GuestProcess& p) { return p.GetHeap().Allocate(Arg(p, 2), (Arg(p, 1) & 8) != 0); });
    process.RegisterApi("HeapFree", [](GuestProcess& p) { uint64 const address = Arg(p, 2); if (address) p.GetHeap().Free(address); return uint64{ 1 }; });
    process.RegisterApi("HeapReAlloc", [](GuestProcess& p) { return p.GetHeap().Reallocate(Arg(p, 2), Arg(p, 3), (Arg(p, 1) & 8) != 0); });
    process.RegisterApi("HeapSize", [](GuestProcess& p) { return p.GetHeap().SizeOf(Arg(p, 2)).value_or(InvalidHandle); });
    process.RegisterApi("VirtualAlloc", [](GuestProcess& p)
    {
        uint64 const requested = Arg(p, 0);
        if (requested && p.GetHeap().Contains(requested))
            return requested;
        uint64 const size = (Arg(p, 1) + 0xFFF) & ~uint64{ 0xFFF };
        return p.GetHeap().Allocate(size ? size : 0x1000, true);
    });
    process.RegisterApi("VirtualProtect", [](GuestProcess& p)
    {
        if (uint64 const old = Arg(p, 3))
            p.GetMachine().WriteU32(old, 0x04);
        return uint64{ 1 };
    });

    auto tlsAlloc = [state](GuestProcess&) { uint32 const index = state->nextTls++; state->tls[index] = 0; return uint64{ index }; };
    auto tlsGet = [state](GuestProcess& p) { state->lastError = 0; auto const found = state->tls.find(static_cast<uint32>(Arg(p, 0))); return found == state->tls.end() ? uint64{ 0 } : found->second; };
    auto tlsSet = [state](GuestProcess& p) { state->tls[static_cast<uint32>(Arg(p, 0))] = Arg(p, 1); return uint64{ 1 }; };
    auto tlsFree = [state](GuestProcess& p) { return uint64{ state->tls.erase(static_cast<uint32>(Arg(p, 0))) ? 1u : 0u }; };
    process.RegisterApi("TlsAlloc", tlsAlloc);
    process.RegisterApi("FlsAlloc", tlsAlloc);
    process.RegisterApi("TlsGetValue", tlsGet);
    process.RegisterApi("FlsGetValue", tlsGet);
    process.RegisterApi("TlsSetValue", tlsSet);
    process.RegisterApi("FlsSetValue", tlsSet);
    process.RegisterApi("TlsFree", tlsFree);
    process.RegisterApi("FlsFree", tlsFree);

    auto zeroFirst = [](std::size_t bytes)
    {
        return [bytes](GuestProcess& p)
        {
            if (uint64 const address = Arg(p, 0))
                p.GetMachine().Write(address, std::vector<uint8>(bytes, 0));
            return uint64{ 0 };
        };
    };
    process.RegisterApi("InitializeCriticalSection", [](GuestProcess& p)
    {
        if (uint64 const address = Arg(p, 0))
        {
            std::vector<uint8> section(40, 0);
            section[8] = 0xFF;
            section[9] = 0xFF;
            section[10] = 0xFF;
            section[11] = 0xFF;
            p.GetMachine().Write(address, section);
        }
        return uint64{ 0 };
    });
    process.RegisterApi("InitializeSRWLock", zeroFirst(8));
    process.RegisterApi("InitializeConditionVariable", zeroFirst(8));
    process.RegisterApi("InitOnceInitialize", zeroFirst(8));
    process.RegisterApi("InitializeSListHead", zeroFirst(16));
    process.RegisterApi("GetTimeZoneInformation", zeroFirst(TimeZoneInformationSize));
    process.RegisterApi("GetDynamicTimeZoneInformation", zeroFirst(DynamicTimeZoneInformationSize));

    auto newHandle = [state](GuestProcess&) { uint64 const handle = state->nextHandle; state->nextHandle += 4; return handle; };
    for (std::string_view const name : { "CreateEventW", "CreateEventA", "CreateEventExW", "CreateSemaphoreW", "CreateSemaphoreExW", "CreateMutexW", "CreateMutexA", "CreateMutexExW" })
        process.RegisterApi(name, newHandle);

    process.RegisterApi("InitOnceExecuteOnce", [](GuestProcess& p)
    {
        Machine& machine = p.GetMachine();
        uint64 const once = Arg(p, 0);
        uint64 const function = Arg(p, 1);
        uint64 const parameter = Arg(p, 2);
        uint64 const context = Arg(p, 3);
        if (machine.ReadU64(once) & 2)
            return uint64{ 1 };
        machine.WriteU64(once, 2);
        machine.SetRegister(GuestRegister::Rcx, once);
        machine.SetRegister(GuestRegister::Rdx, parameter);
        machine.SetRegister(GuestRegister::R8, context);
        machine.Redirect(function);
        return uint64{ 1 };
    });
    process.RegisterApi("InitOnceBeginInitialize", [](GuestProcess& p)
    {
        Machine& machine = p.GetMachine();
        uint64 const once = Arg(p, 0);
        uint64 const value = machine.ReadU64(once);
        bool const done = (value & 2) != 0;
        if (uint64 const pending = Arg(p, 2))
            machine.WriteU32(pending, done ? 0 : 1);
        if (uint64 const context = Arg(p, 3); context && done)
            machine.WriteU64(context, value & ~uint64{ 3 });
        return uint64{ 1 };
    });
    process.RegisterApi("InitOnceComplete", [](GuestProcess& p)
    {
        p.GetMachine().WriteU64(Arg(p, 0), (Arg(p, 2) & ~uint64{ 3 }) | 2);
        return uint64{ 1 };
    });

    process.RegisterApi("InterlockedPushEntrySList", [](GuestProcess& p)
    {
        Machine& machine = p.GetMachine();
        uint64 const head = Arg(p, 0);
        uint64 const item = Arg(p, 1);
        uint64 const first = machine.ReadU64(head);
        machine.WriteU64(item, first);
        machine.WriteU64(head, item);
        machine.WriteU16(head + 8, static_cast<uint16>(machine.ReadU16(head + 8) + 1));
        return first;
    });
    process.RegisterApi("InterlockedPopEntrySList", [](GuestProcess& p)
    {
        Machine& machine = p.GetMachine();
        uint64 const head = Arg(p, 0);
        uint64 const first = machine.ReadU64(head);
        if (first)
        {
            machine.WriteU64(head, machine.ReadU64(first));
            machine.WriteU16(head + 8, static_cast<uint16>(machine.ReadU16(head + 8) - 1));
        }
        return first;
    });
    process.RegisterApi("InterlockedFlushSList", [](GuestProcess& p)
    {
        Machine& machine = p.GetMachine();
        uint64 const head = Arg(p, 0);
        uint64 const first = machine.ReadU64(head);
        machine.WriteU64(head, 0);
        machine.WriteU16(head + 8, 0);
        return first;
    });
    process.RegisterApi("QueryDepthSList", [](GuestProcess& p) { return uint64{ p.GetMachine().ReadU16(Arg(p, 0) + 8) }; });

    process.RegisterApi("EncodePointer", [](GuestProcess& p) { return Arg(p, 0); });
    process.RegisterApi("DecodePointer", [](GuestProcess& p) { return Arg(p, 0); });
    process.RegisterApi("EncodeSystemPointer", [](GuestProcess& p) { return Arg(p, 0); });
    process.RegisterApi("DecodeSystemPointer", [](GuestProcess& p) { return Arg(p, 0); });
    process.RegisterApi("IsProcessorFeaturePresent", [](GuestProcess& p)
    {
        uint32 const feature = static_cast<uint32>(Arg(p, 0));
        return uint64{ feature == 6 || feature == 10 || feature == 13 || feature == 17 ? 1u : 0u };
    });

    process.RegisterApi("QueryPerformanceCounter", [](GuestProcess& p) { p.GetMachine().WriteU64(Arg(p, 0), PerformanceCounter); return uint64{ 1 }; });
    process.RegisterApi("QueryPerformanceFrequency", [](GuestProcess& p) { p.GetMachine().WriteU64(Arg(p, 0), PerformanceFrequency); return uint64{ 1 }; });
    auto fileTime = [](GuestProcess& p) { p.GetMachine().WriteU64(Arg(p, 0), FileTime); return uint64{ 0 }; };
    process.RegisterApi("GetSystemTimeAsFileTime", fileTime);
    process.RegisterApi("GetSystemTimePreciseAsFileTime", fileTime);
    auto systemTime = [](GuestProcess& p)
    {
        std::vector<uint8> time(16, 0);
        std::array<uint16, 8> const fields = SystemTimeOf(FileTime);
        for (std::size_t i = 0; i < fields.size(); ++i)
        {
            time[i * 2] = static_cast<uint8>(fields[i]);
            time[i * 2 + 1] = static_cast<uint8>(fields[i] >> 8);
        }
        p.GetMachine().Write(Arg(p, 0), time);
        return uint64{ 0 };
    };
    process.RegisterApi("GetSystemTime", systemTime);
    process.RegisterApi("GetLocalTime", systemTime);

    process.RegisterApi("GetStartupInfoW", [](GuestProcess& p)
    {
        std::vector<uint8> info(104, 0);
        info[0] = 104;
        p.GetMachine().Write(Arg(p, 0), info);
        return uint64{ 0 };
    });
    process.RegisterApi("GetStartupInfoA", [](GuestProcess& p)
    {
        std::vector<uint8> info(104, 0);
        info[0] = 104;
        p.GetMachine().Write(Arg(p, 0), info);
        return uint64{ 0 };
    });
    auto staticNarrow = [state](std::string key, std::string text)
    {
        return [state, key, text](GuestProcess& p)
        {
            auto const found = state->statics.find(key);
            if (found != state->statics.end())
                return found->second;
            uint64 const address = p.StoreBytes(std::vector<uint8>(text.begin(), text.end()));
            state->statics.emplace(key, address);
            return address;
        };
    };
    process.RegisterApi("GetCommandLineA", staticNarrow("cmdA", std::string("WizardGraphicalClient.exe\0", 26)));
    process.RegisterApi("GetCommandLineW", [state](GuestProcess& p)
    {
        auto const found = state->statics.find("cmdW");
        if (found != state->statics.end())
            return found->second;
        uint64 const address = p.StoreWideString(u"WizardGraphicalClient.exe");
        state->statics.emplace("cmdW", address);
        return address;
    });
    process.RegisterApi("GetEnvironmentStringsW", staticNarrow("envW", std::string(4, '\0')));
    process.RegisterApi("GetEnvironmentStringsA", staticNarrow("envA", std::string(2, '\0')));
    process.RegisterApi("GetEnvironmentStrings", staticNarrow("envA", std::string(2, '\0')));
    auto noVariable = [state](GuestProcess&) { state->lastError = ErrorEnvVarNotFound; return uint64{ 0 }; };
    process.RegisterApi("GetEnvironmentVariableW", noVariable);
    process.RegisterApi("GetEnvironmentVariableA", noVariable);

    auto moduleHandle = [](bool wide)
    {
        return [wide](GuestProcess& p)
        {
            uint64 const name = Arg(p, 0);
            if (!name)
                return p.GetMain().Base;
            std::string const moduleName = ModuleName(p, name, wide);
            if (IsKernelModuleName(moduleName))
                return GuestProcess::KernelModuleHandle;
            GuestModule const* const module = p.FindModule(moduleName);
            return module ? module->Base : uint64{ 0 };
        };
    };
    process.RegisterApi("GetModuleHandleW", moduleHandle(true));
    process.RegisterApi("GetModuleHandleA", moduleHandle(false));
    process.RegisterApi("GetModuleHandleExW", [](GuestProcess& p)
    {
        uint32 const flags = static_cast<uint32>(Arg(p, 0));
        uint64 const name = Arg(p, 1);
        uint64 const out = Arg(p, 2);
        uint64 base = 0;
        if (flags & 4)
        {
            if (GuestModule const* const module = p.ModuleAt(name))
                base = module->Base;
        }
        else if (!name)
            base = p.GetMain().Base;
        else if (std::string const moduleName = ModuleName(p, name, true); IsKernelModuleName(moduleName))
            base = GuestProcess::KernelModuleHandle;
        else if (GuestModule const* const module = p.FindModule(moduleName))
            base = module->Base;
        if (out)
            p.GetMachine().WriteU64(out, base);
        return uint64{ base ? 1u : 0u };
    });
    auto loadLibrary = [state](bool wide)
    {
        return [state, wide](GuestProcess& p)
        {
            std::string const moduleName = ModuleName(p, Arg(p, 0), wide);
            if (IsKernelModuleName(moduleName))
                return GuestProcess::KernelModuleHandle;
            GuestModule const* const module = p.FindModule(moduleName);
            if (!module)
                state->lastError = ErrorModNotFound;
            return module ? module->Base : uint64{ 0 };
        };
    };
    process.RegisterApi("LoadLibraryExW", loadLibrary(true));
    process.RegisterApi("LoadLibraryW", loadLibrary(true));
    process.RegisterApi("LoadLibraryExA", loadLibrary(false));
    process.RegisterApi("LoadLibraryA", loadLibrary(false));
    process.RegisterApi("GetProcAddress", [state](GuestProcess& p)
    {
        uint64 const base = Arg(p, 0);
        uint64 const name = Arg(p, 1);
        if (base == GuestProcess::KernelModuleHandle)
        {
            std::string const function = name >> 16 ? p.GetMachine().ReadCString(name, 1024).value_or(std::string{}) : std::string{};
            if (function.empty() || !p.HasApi(function))
            {
                state->lastError = ErrorProcNotFound;
                return uint64{ 0 };
            }
            return p.ResolveImport("kernel32.dll", function, std::nullopt);
        }
        for (GuestModule const* const module : p.GetModules())
        {
            if (module->Base != base)
                continue;
            std::optional<uint64> address;
            if (name >> 16)
                address = p.ResolveExport(*module, p.GetMachine().ReadCString(name, 1024).value_or(std::string{}));
            else
                address = p.ResolveExportByOrdinal(*module, static_cast<uint16>(name));
            if (!address)
                state->lastError = ErrorProcNotFound;
            return address.value_or(0);
        }
        state->lastError = ErrorModNotFound;
        return uint64{ 0 };
    });
    auto moduleFileName = [state](bool wide)
    {
        return [state, wide](GuestProcess& p)
        {
            uint64 const buffer = Arg(p, 1);
            uint64 const size = static_cast<uint32>(Arg(p, 2));
            std::optional<std::u16string> const path = GuestModulePath(p, Arg(p, 0));
            if (!path)
            {
                state->lastError = ErrorModNotFound;
                return uint64{ 0 };
            }
            if (!size)
            {
                state->lastError = ErrorInsufficientBuffer;
                return uint64{ 0 };
            }
            bool unmapped = false;
            std::vector<uint8> narrow;
            if (!wide)
                narrow = EncodeText(AnsiCodePage, *path, unmapped);
            uint64 const length = wide ? path->size() : narrow.size();
            bool const fits = length < size;
            std::size_t const count = static_cast<std::size_t>(fits ? length : size - 1);
            if (wide)
            {
                std::u16string text = path->substr(0, count);
                text.push_back(0);
                WriteWide(p.GetMachine(), buffer, text);
            }
            else
            {
                narrow.resize(count);
                narrow.push_back(0);
                p.GetMachine().Write(buffer, narrow);
            }
            state->lastError = fits ? 0 : ErrorInsufficientBuffer;
            return fits ? uint64{ count } : size;
        };
    };
    process.RegisterApi("GetModuleFileNameW", moduleFileName(true));
    process.RegisterApi("GetModuleFileNameA", moduleFileName(false));
    process.RegisterApi("RtlPcToFileHeader", [](GuestProcess& p)
    {
        GuestModule const* const module = p.ModuleAt(Arg(p, 0));
        uint64 const base = module ? module->Base : 0;
        if (uint64 const out = Arg(p, 1))
            p.GetMachine().WriteU64(out, base);
        return base;
    });
    process.RegisterApi("RtlCaptureContext", [](GuestProcess& p)
    {
        if (uint64 const context = Arg(p, 0))
            p.GetMachine().Write(context, std::vector<uint8>(0x4D0, 0));
        return uint64{ 0 };
    });

    auto refuse = [](std::string what)
    {
        return [what](GuestProcess& p) -> uint64
        {
            throw EmulationError(fmt::format("the client called {} ({}) during extraction, which extraction cannot follow", what, p.GetCurrentApiName()));
        };
    };
    for (std::string_view const name : { "ExitProcess", "TerminateProcess", "RaiseException", "RaiseFailFastException", "RtlUnwindEx", "RtlVirtualUnwind", "CreateThread", "_beginthreadex" })
        process.RegisterApi(name, refuse(std::string(name)));

    auto fileNotFound = [state](GuestProcess&) { state->lastError = ErrorFileNotFound; return InvalidHandle; };
    process.RegisterApi("CreateFileW", fileNotFound);
    process.RegisterApi("CreateFileA", fileNotFound);
    process.RegisterApi("FindFirstFileExW", fileNotFound);
    process.RegisterApi("FindFirstFileW", fileNotFound);
    process.RegisterApi("GetFileAttributesW", [state](GuestProcess&) { state->lastError = ErrorFileNotFound; return uint64{ 0xFFFFFFFF }; });
    process.RegisterApi("GetFileAttributesExW", [state](GuestProcess&) { state->lastError = ErrorFileNotFound; return uint64{ 0 }; });
    process.RegisterApi("WriteFile", [](GuestProcess& p)
    {
        if (uint64 const written = Arg(p, 3))
            p.GetMachine().WriteU32(written, static_cast<uint32>(Arg(p, 2)));
        return uint64{ 1 };
    });
    process.RegisterApi("WriteConsoleW", [](GuestProcess& p)
    {
        if (uint64 const written = Arg(p, 3))
            p.GetMachine().WriteU32(written, static_cast<uint32>(Arg(p, 2)));
        return uint64{ 1 };
    });

    process.RegisterApi("IsValidCodePage", [](GuestProcess& p) { return uint64{ IsSupportedCodePage(static_cast<uint32>(Arg(p, 0))) ? 1u : 0u }; });
    process.RegisterApi("GetCPInfo", [state](GuestProcess& p)
    {
        uint32 const codePage = static_cast<uint32>(Arg(p, 0));
        if (!IsSupportedCodePage(codePage))
        {
            state->lastError = ErrorInvalidParameter;
            return uint64{ 0 };
        }
        std::vector<uint8> info(20, 0);
        info[0] = ResolveCodePage(codePage) == 65001 ? 4 : 1;
        info[4] = '?';
        p.GetMachine().Write(Arg(p, 1), info);
        return uint64{ 1 };
    });
    process.RegisterApi("MultiByteToWideChar", [state](GuestProcess& p)
    {
        uint32 const codePage = static_cast<uint32>(Arg(p, 0));
        uint32 const flags = static_cast<uint32>(Arg(p, 1));
        uint64 const source = Arg(p, 2);
        uint64 const sourceLength = Arg(p, 3);
        uint64 const destination = Arg(p, 4);
        uint64 const destinationLength = static_cast<uint32>(Arg(p, 5));
        if (!IsSupportedCodePage(codePage) || static_cast<int32>(static_cast<uint32>(sourceLength)) == 0)
        {
            state->lastError = ErrorInvalidParameter;
            return uint64{ 0 };
        }
        bool invalid = false;
        std::u16string const text = DecodeBytes(codePage, ReadNarrow(p, source, sourceLength), invalid);
        if (invalid && (flags & MbErrInvalidChars))
        {
            state->lastError = ErrorNoUnicodeTranslation;
            return uint64{ 0 };
        }
        if (!destinationLength)
            return uint64{ text.size() };
        if (text.size() > destinationLength)
        {
            WriteWide(p.GetMachine(), destination, std::u16string_view(text).substr(0, static_cast<std::size_t>(destinationLength)));
            state->lastError = ErrorInsufficientBuffer;
            return uint64{ 0 };
        }
        WriteWide(p.GetMachine(), destination, text);
        return uint64{ text.size() };
    });
    process.RegisterApi("WideCharToMultiByte", [state](GuestProcess& p)
    {
        uint32 const codePage = static_cast<uint32>(Arg(p, 0));
        uint64 const source = Arg(p, 2);
        uint64 const sourceLength = Arg(p, 3);
        uint64 const destination = Arg(p, 4);
        uint64 const destinationLength = static_cast<uint32>(Arg(p, 5));
        uint64 const usedDefault = Arg(p, 7);
        if (!IsSupportedCodePage(codePage) || static_cast<int32>(static_cast<uint32>(sourceLength)) == 0)
        {
            state->lastError = ErrorInvalidParameter;
            return uint64{ 0 };
        }
        bool defaulted = false;
        std::vector<uint8> const bytes = EncodeText(codePage, ReadWide(p, source, sourceLength), defaulted);
        if (usedDefault && ResolveCodePage(codePage) != 65001)
            p.GetMachine().WriteU32(usedDefault, defaulted ? 1 : 0);
        if (!destinationLength)
            return uint64{ bytes.size() };
        if (bytes.size() > destinationLength)
        {
            p.GetMachine().Write(destination, std::span<uint8 const>(bytes.data(), static_cast<std::size_t>(destinationLength)));
            state->lastError = ErrorInsufficientBuffer;
            return uint64{ 0 };
        }
        if (!bytes.empty())
            p.GetMachine().Write(destination, bytes);
        return uint64{ bytes.size() };
    });
    process.RegisterApi("GetStringTypeW", [](GuestProcess& p)
    {
        uint32 const kind = static_cast<uint32>(Arg(p, 0));
        std::u16string text = ReadWide(p, Arg(p, 1), Arg(p, 2));
        if (static_cast<int32>(static_cast<uint32>(Arg(p, 2))) < 0 && !text.empty())
            text.pop_back();
        std::vector<uint8> out(text.size() * 2, 0);
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            uint16 const type = kind == 1 ? CharType1(text[i]) : 0;
            out[i * 2] = static_cast<uint8>(type);
            out[i * 2 + 1] = static_cast<uint8>(type >> 8);
        }
        if (!out.empty())
            p.GetMachine().Write(Arg(p, 3), out);
        return uint64{ 1 };
    });
    process.RegisterApi("GetStringTypeExW", [](GuestProcess& p)
    {
        uint32 const kind = static_cast<uint32>(Arg(p, 1));
        std::u16string text = ReadWide(p, Arg(p, 2), Arg(p, 3));
        if (static_cast<int32>(static_cast<uint32>(Arg(p, 3))) < 0 && !text.empty())
            text.pop_back();
        std::vector<uint8> out(text.size() * 2, 0);
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            uint16 const type = kind == 1 ? CharType1(text[i]) : 0;
            out[i * 2] = static_cast<uint8>(type);
            out[i * 2 + 1] = static_cast<uint8>(type >> 8);
        }
        if (!out.empty())
            p.GetMachine().Write(Arg(p, 4), out);
        return uint64{ 1 };
    });
    auto mapString = [state](GuestProcess& p)
    {
        uint32 const flags = static_cast<uint32>(Arg(p, 1));
        std::u16string text = ReadWide(p, Arg(p, 2), Arg(p, 3));
        for (char16_t& c : text)
            c = (flags & LcmapLowercase) ? Lower16(c) : (flags & LcmapUppercase) ? Upper16(c) : c;
        uint64 const destination = Arg(p, 4);
        uint64 const destinationLength = static_cast<uint32>(Arg(p, 5));
        if (!destinationLength)
            return uint64{ text.size() };
        if (text.size() > destinationLength)
        {
            state->lastError = ErrorInsufficientBuffer;
            return uint64{ 0 };
        }
        WriteWide(p.GetMachine(), destination, text);
        return uint64{ text.size() };
    };
    process.RegisterApi("LCMapStringW", mapString);
    process.RegisterApi("LCMapStringEx", mapString);
    auto compareString = [](GuestProcess& p)
    {
        std::u16string a = ReadWide(p, Arg(p, 2), Arg(p, 3));
        std::u16string b = ReadWide(p, Arg(p, 4), Arg(p, 5));
        if (static_cast<int32>(static_cast<uint32>(Arg(p, 3))) < 0 && !a.empty())
            a.pop_back();
        if (static_cast<int32>(static_cast<uint32>(Arg(p, 5))) < 0 && !b.empty())
            b.pop_back();
        if (Arg(p, 1) & 1)
        {
            for (char16_t& c : a)
                c = Lower16(c);
            for (char16_t& c : b)
                c = Lower16(c);
        }
        int const order = a.compare(b);
        return uint64{ order < 0 ? 1u : order == 0 ? 2u : 3u };
    };
    process.RegisterApi("CompareStringW", compareString);
    process.RegisterApi("CompareStringEx", compareString);
    auto localeName = [](GuestProcess& p)
    {
        uint64 const buffer = Arg(p, 0);
        uint64 const size = static_cast<uint32>(Arg(p, 1));
        if (buffer && size >= 6)
            WriteWide(p.GetMachine(), buffer, std::u16string(u"en-US", 6));
        return uint64{ 6 };
    };
    process.RegisterApi("GetUserDefaultLocaleName", localeName);
    process.RegisterApi("GetSystemDefaultLocaleName", localeName);
    process.RegisterApi("LCIDToLocaleName", [](GuestProcess& p)
    {
        uint64 const buffer = Arg(p, 1);
        uint64 const size = static_cast<uint32>(Arg(p, 2));
        if (buffer && size >= 6)
            WriteWide(p.GetMachine(), buffer, std::u16string(u"en-US", 6));
        return uint64{ 6 };
    });

    auto systemInfo = [](GuestProcess& p)
    {
        std::vector<uint8> info(48, 0);
        auto put16 = [&](std::size_t at, uint16 v) { info[at] = static_cast<uint8>(v); info[at + 1] = static_cast<uint8>(v >> 8); };
        auto put32 = [&](std::size_t at, uint32 v) { for (int i = 0; i < 4; ++i) info[at + i] = static_cast<uint8>(v >> (8 * i)); };
        auto put64 = [&](std::size_t at, uint64 v) { for (int i = 0; i < 8; ++i) info[at + i] = static_cast<uint8>(v >> (8 * i)); };
        put16(0, 9);
        put32(4, 0x1000);
        put64(8, 0x10000);
        put64(16, 0x7FFFFFFEFFFF);
        put64(24, 0xFF);
        put32(32, 8);
        put32(36, 8664);
        put32(40, 0x10000);
        put16(44, 6);
        put16(46, 0x5E03);
        p.GetMachine().Write(Arg(p, 0), info);
        return uint64{ 0 };
    };
    process.RegisterApi("GetSystemInfo", systemInfo);
    process.RegisterApi("GetNativeSystemInfo", systemInfo);
}

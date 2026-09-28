/*
 * Project Ambrose by Imjustchico
 * Decodes a query value exactly once, refusing a percent sign not followed by two hex digits and keeping a plus sign as itself, then checks the text in a fixed order so each refusal names the right kind: encoding, length, control characters, absolute forms, dot components, backslashes, empty components, depth, and each name's length, characters, trailing dot or space and reserved device name, the last matched with any extension, ignoring case and trailing spaces, and in the superscript COM and LPT forms Windows also reserves.
 */

#include "JailPath.h"
#include "Utf.h"

#include <fmt/format.h>

#include <algorithm>
#include <exception>

namespace
{
    int HexValue(char c) noexcept
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    char Lower(char c) noexcept
    {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }

    bool SameIgnoringCase(std::string_view left, std::string_view right) noexcept
    {
        return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) { return Lower(a) == Lower(b); });
    }

    bool IsControl(unsigned char byte) noexcept
    {
        return byte < 0x20 || byte == 0x7F;
    }

    bool HoldsC1Control(std::string_view text) noexcept
    {
        for (std::size_t index = 0; index + 1 < text.size(); ++index)
            if (static_cast<unsigned char>(text[index]) == 0xC2 && static_cast<unsigned char>(text[index + 1]) >= 0x80 && static_cast<unsigned char>(text[index + 1]) <= 0x9F)
                return true;
        return false;
    }

    bool StartsWithDrive(std::string_view text) noexcept
    {
        return text.size() >= 2 && ((text[0] >= 'A' && text[0] <= 'Z') || (text[0] >= 'a' && text[0] <= 'z')) && text[1] == ':';
    }

    std::vector<std::string_view> Split(std::string_view text, bool backslashToo)
    {
        std::vector<std::string_view> parts;
        std::size_t start = 0;
        for (std::size_t index = 0; index <= text.size(); ++index)
        {
            if (index == text.size() || text[index] == '/' || (backslashToo && text[index] == '\\'))
            {
                parts.push_back(text.substr(start, index - start));
                start = index + 1;
            }
        }
        return parts;
    }

    Ambrose::JailPathResult Refused(Ambrose::JailPathResult result, Ambrose::JailRefusal refusal, std::string reason)
    {
        result.Refusal = refusal;
        result.Reason = std::move(reason);
        result.Path.Components.clear();
        return result;
    }

    std::string Repaired(std::string_view text)
    {
        if (Utf::IsValidUtf8(text))
            return std::string(text);
        std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(text, Utf::InvalidPolicy::ReplaceWithU_FFFD);
        if (!wide)
            return {};
        return Utf::Utf16ToUtf8(*wide, Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string());
    }
}

std::string Ambrose::JailPath::Text() const
{
    std::string text;
    for (std::string const& component : Components)
    {
        if (!text.empty())
            text.push_back('/');
        text += component;
    }
    return text;
}

std::string Ambrose::JailPath::Leaf() const
{
    return Components.empty() ? std::string() : Components.back();
}

Ambrose::JailPath Ambrose::JailPath::Parent() const
{
    JailPath parent = *this;
    if (!parent.Components.empty())
        parent.Components.pop_back();
    return parent;
}

Ambrose::JailPath Ambrose::JailPath::Child(std::string name) const
{
    JailPath child = *this;
    child.Components.push_back(std::move(name));
    return child;
}

std::optional<std::string> Ambrose::JailPaths::PercentDecode(std::string_view raw)
{
    std::string decoded;
    decoded.reserve(raw.size());
    for (std::size_t index = 0; index < raw.size(); ++index)
    {
        char const c = raw[index];
        if (c != '%')
        {
            decoded.push_back(c);
            continue;
        }
        if (index + 2 >= raw.size())
            return std::nullopt;
        int const high = HexValue(raw[index + 1]);
        int const low = HexValue(raw[index + 2]);
        if (high < 0 || low < 0)
            return std::nullopt;
        decoded.push_back(static_cast<char>(high * 16 + low));
        index += 2;
    }
    return decoded;
}

Ambrose::JailPathResult Ambrose::JailPaths::ParseQuery(std::string_view raw)
{
    std::optional<std::string> const decoded = PercentDecode(raw);
    if (!decoded)
    {
        JailPathResult result;
        result.Decoded = std::string(raw);
        return Refused(std::move(result), JailRefusal::Encoding, "The path holds a percent sign that is not followed by two hex digits");
    }
    return ParseText(*decoded);
}

bool Ambrose::JailPaths::IsReservedDeviceName(std::string_view name)
{
    std::string_view base = name.substr(0, name.find('.'));
    while (!base.empty() && base.back() == ' ')
        base.remove_suffix(1);
    for (std::string_view const reserved : { "CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$" })
        if (SameIgnoringCase(base, reserved))
            return true;
    if (base.size() < 4 || !(SameIgnoringCase(base.substr(0, 3), "COM") || SameIgnoringCase(base.substr(0, 3), "LPT")))
        return false;
    std::string_view const number = base.substr(3);
    if (number.size() == 1 && number[0] >= '0' && number[0] <= '9')
        return true;
    return number == "\xC2\xB9" || number == "\xC2\xB2" || number == "\xC2\xB3";
}

Ambrose::JailRefusal Ambrose::JailPaths::CheckName(std::string_view name, std::string& reason)
{
    if (name.empty())
    {
        reason = "A path has no empty component; drop the doubled or trailing slash";
        return JailRefusal::Name;
    }
    if (name == "." || name == "..")
    {
        reason = "A path may not name . or .., so it cannot step out of its root";
        return JailRefusal::Traversal;
    }
    if (name.size() > MaxComponentBytes)
    {
        reason = fmt::format("A name may be at most {} bytes long", MaxComponentBytes);
        return JailRefusal::TooLong;
    }
    constexpr std::string_view Forbidden = "<>:\"|?*\\/";
    for (char const c : name)
    {
        if (IsControl(static_cast<unsigned char>(c)))
        {
            reason = "A name may not hold a control character";
            return JailRefusal::Name;
        }
        if (Forbidden.find(c) != std::string_view::npos)
        {
            reason = fmt::format("A name may not hold {}, which Windows cannot store", c);
            return JailRefusal::Name;
        }
    }
    if (HoldsC1Control(name))
    {
        reason = "A name may not hold a control character";
        return JailRefusal::Name;
    }
    if (name.back() == '.' || name.back() == ' ')
    {
        reason = "A name may not end in a dot or a space, which Windows drops";
        return JailRefusal::Name;
    }
    if (IsReservedDeviceName(name))
    {
        reason = "A name may not be a reserved device name such as CON, NUL, COM1 or LPT1, with or without an extension";
        return JailRefusal::DeviceName;
    }
    return JailRefusal::None;
}

Ambrose::JailPathResult Ambrose::JailPaths::ParseText(std::string_view text)
{
    JailPathResult result;
    result.Decoded = std::string(text);
    if (text.empty())
        return result;
    if (!Utf::IsValidUtf8(text))
        return Refused(std::move(result), JailRefusal::Encoding, "The path is not valid UTF-8");
    if (text.size() > MaxPathBytes)
        return Refused(std::move(result), JailRefusal::TooLong, fmt::format("A path may be at most {} bytes long", MaxPathBytes));
    if (std::any_of(text.begin(), text.end(), [](char c) { return IsControl(static_cast<unsigned char>(c)); }) || HoldsC1Control(text))
        return Refused(std::move(result), JailRefusal::Name, "A path may not hold a control character");
    if (text.front() == '/' || text.front() == '\\')
        return Refused(std::move(result), JailRefusal::Absolute, "An absolute path, a UNC name or a device path is refused; give a path inside the root");
    if (StartsWithDrive(text))
        return Refused(std::move(result), JailRefusal::Absolute, "A drive letter is refused; give a path inside the root");
    for (std::string_view const part : Split(text, true))
        if (part == "." || part == "..")
            return Refused(std::move(result), JailRefusal::Traversal, "A path may not name . or .., so it cannot step out of its root");
    if (text.find('\\') != std::string_view::npos)
        return Refused(std::move(result), JailRefusal::Name, "A backslash is not a separator here and no name may hold one; separate folders with /");
    std::vector<std::string_view> const components = Split(text, false);
    if (std::any_of(components.begin(), components.end(), [](std::string_view component) { return component.empty(); }))
        return Refused(std::move(result), JailRefusal::Name, "A path has no empty component; drop the doubled or trailing slash");
    if (components.size() > MaxComponents)
        return Refused(std::move(result), JailRefusal::TooDeep, fmt::format("A path may be at most {} folders deep", MaxComponents));
    for (std::string_view const component : components)
    {
        std::string reason;
        JailRefusal const refusal = CheckName(component, reason);
        if (refusal != JailRefusal::None)
            return Refused(std::move(result), refusal, std::move(reason));
    }
    for (std::string_view const component : components)
        result.Path.Components.emplace_back(component);
    return result;
}

std::string_view Ambrose::JailPaths::RefusalCode(JailRefusal refusal) noexcept
{
    switch (refusal)
    {
        case JailRefusal::None: return "none";
        case JailRefusal::Traversal: return "traversal";
        case JailRefusal::Absolute: return "absolute";
        case JailRefusal::Encoding: return "encoding";
        case JailRefusal::Name: return "name";
        case JailRefusal::DeviceName: return "device_name";
        case JailRefusal::TooLong: return "too_long";
        case JailRefusal::TooDeep: break;
    }
    return "too_deep";
}

std::filesystem::path Ambrose::JailPaths::HostPathFor(std::filesystem::path const& root, std::string_view decoded)
{
    std::string text = Repaired(decoded);
    text.erase(std::remove(text.begin(), text.end(), '\0'), text.end());
    bool const absolute = !text.empty() && (text.front() == '/' || text.front() == '\\' || StartsWithDrive(text));
    std::replace(text.begin(), text.end(), '\\', '/');
    try
    {
        std::filesystem::path const asked(std::u8string(text.begin(), text.end()));
        return (absolute ? asked : root / asked).lexically_normal();
    }
    catch (std::exception const&)
    {
        return root;
    }
}

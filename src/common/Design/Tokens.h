/*
 * Project Ambrose by Imjustchico
 * Generated from design/tokens.json: every semantic colour in truecolor, 256 and 16 form, with the motion durations the terminal obeys.
 */
#ifndef AMBROSE_TOKENS_H
#define AMBROSE_TOKENS_H

#include <array>
#include <cstdint>
#include <string_view>

namespace Ambrose::Design
{
struct TerminalColor
{
    std::string_view Name;
    std::string_view Hex;
    std::uint8_t Red;
    std::uint8_t Green;
    std::uint8_t Blue;
    std::uint8_t Index256;
    std::uint8_t Index16;
};

inline constexpr std::array<TerminalColor, 21> DarkTokens = {{
    TerminalColor{"surface-page", "#0B1020", 11, 16, 32, 233, 0},
    TerminalColor{"surface-card", "#131B31", 19, 27, 49, 235, 0},
    TerminalColor{"surface-sunken", "#0E1527", 14, 21, 39, 234, 0},
    TerminalColor{"surface-chrome", "#070B16", 7, 11, 22, 233, 0},
    TerminalColor{"edge-quiet", "#1B2540", 27, 37, 64, 236, 8},
    TerminalColor{"edge-strong", "#22304F", 34, 48, 79, 237, 8},
    TerminalColor{"edge-control", "#546AA5", 84, 106, 165, 61, 4},
    TerminalColor{"value-number", "#72BCFA", 114, 188, 250, 75, 12},
    TerminalColor{"value-text", "#5BA2EC", 91, 162, 236, 74, 12},
    TerminalColor{"value-name", "#4887DC", 72, 135, 220, 68, 4},
    TerminalColor{"fg-body", "#F2E8D5", 242, 232, 213, 7, 15},
    TerminalColor{"fg-muted", "#A8B6D4", 168, 182, 212, 146, 7},
    TerminalColor{"fg-faint", "#8798BC", 135, 152, 188, 103, 8},
    TerminalColor{"action", "#E4B457", 228, 180, 87, 179, 11},
    TerminalColor{"action-pressed", "#B98A2D", 185, 138, 45, 136, 3},
    TerminalColor{"state-healthy", "#5FD3C4", 95, 211, 196, 79, 14},
    TerminalColor{"state-waiting", "#E4B457", 228, 180, 87, 179, 11},
    TerminalColor{"state-wrong", "#E2725B", 226, 114, 91, 167, 9},
    TerminalColor{"state-unknown", "#8798BC", 135, 152, 188, 103, 8},
    TerminalColor{"mine", "#C77DFF", 199, 125, 255, 177, 13},
    TerminalColor{"focus-ring", "#E4B457", 228, 180, 87, 179, 11},
}};

inline constexpr std::array<TerminalColor, 21> LightTokens = {{
    TerminalColor{"surface-page", "#F4EAD5", 244, 234, 213, 224, 15},
    TerminalColor{"surface-card", "#FFFDF7", 255, 253, 247, 15, 15},
    TerminalColor{"surface-sunken", "#EADFC4", 234, 223, 196, 253, 7},
    TerminalColor{"surface-chrome", "#FBF5E7", 251, 245, 231, 255, 15},
    TerminalColor{"edge-quiet", "#D9CBAB", 217, 203, 171, 187, 7},
    TerminalColor{"edge-strong", "#C3AE86", 195, 174, 134, 144, 7},
    TerminalColor{"edge-control", "#546AA5", 84, 106, 165, 61, 4},
    TerminalColor{"value-number", "#233E5A", 35, 62, 90, 17, 4},
    TerminalColor{"value-text", "#2D5075", 45, 80, 117, 23, 4},
    TerminalColor{"value-name", "#376390", 55, 99, 144, 24, 4},
    TerminalColor{"fg-body", "#1B1608", 27, 22, 8, 233, 0},
    TerminalColor{"fg-muted", "#4A3F28", 74, 63, 40, 237, 8},
    TerminalColor{"fg-faint", "#5F5238", 95, 82, 56, 58, 8},
    TerminalColor{"action", "#7A5A12", 122, 90, 18, 94, 3},
    TerminalColor{"action-pressed", "#6A4E0F", 106, 78, 15, 58, 3},
    TerminalColor{"state-healthy", "#0F6F63", 15, 111, 99, 23, 6},
    TerminalColor{"state-waiting", "#7A5A12", 122, 90, 18, 94, 3},
    TerminalColor{"state-wrong", "#A33A25", 163, 58, 37, 124, 1},
    TerminalColor{"state-unknown", "#5F5238", 95, 82, 56, 58, 8},
    TerminalColor{"mine", "#6B2FA0", 107, 47, 160, 55, 5},
    TerminalColor{"focus-ring", "#7A5A12", 122, 90, 18, 94, 3},
}};

inline constexpr std::array<TerminalColor, 7> DarkSeries = {{
    TerminalColor{"series-1", "#C9503F", 201, 80, 63, 1, 1},
    TerminalColor{"series-2", "#B8C24E", 184, 194, 78, 143, 3},
    TerminalColor{"series-3", "#79C98A", 121, 201, 138, 114, 10},
    TerminalColor{"series-4", "#2A8F7C", 42, 143, 124, 30, 6},
    TerminalColor{"series-5", "#7D6BD6", 125, 107, 214, 98, 12},
    TerminalColor{"series-6", "#A560A5", 165, 96, 165, 133, 5},
    TerminalColor{"series-7", "#E1A6C4", 225, 166, 196, 181, 13},
}};

inline constexpr std::array<TerminalColor, 7> LightSeries = {{
    TerminalColor{"series-1", "#B45A5A", 180, 90, 90, 131, 1},
    TerminalColor{"series-2", "#8A7A22", 138, 122, 34, 100, 3},
    TerminalColor{"series-3", "#2F5A22", 47, 90, 34, 22, 2},
    TerminalColor{"series-4", "#2D7E9C", 45, 126, 156, 30, 6},
    TerminalColor{"series-5", "#3B3AA8", 59, 58, 168, 19, 4},
    TerminalColor{"series-6", "#6A2A86", 106, 42, 134, 54, 5},
    TerminalColor{"series-7", "#8E2A5C", 142, 42, 92, 89, 13},
}};

inline constexpr std::uint32_t DurationFlipMs = 90;
inline constexpr std::uint32_t DurationHoverMs = 120;
inline constexpr std::uint32_t DurationPanelMs = 200;
inline constexpr std::uint32_t DurationScreenMs = 320;

constexpr TerminalColor const* Find(std::array<TerminalColor, 21> const& tokens, std::string_view name)
{
    for (TerminalColor const& token : tokens)
    {
        if (token.Name == name)
        {
            return &token;
        }
    }
    return nullptr;
}
}

#endif

/*
 * Project Ambrose by Imjustchico
 * Reads the marks a burning child process wrote, its steady clock beside the processor time it had used: waits for the first, so a test starts measuring only once the burn has begun however slowly a busy machine starts the process, and gives the share of one core it truly held between two steady-clock instants the test measured, interpolating between the marks either side of each.
 */

#ifndef AMBROSE_BURNREPORT_H
#define AMBROSE_BURNREPORT_H

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace BurnReport
{
    inline bool WaitForFirstMark(std::filesystem::path const& output, std::chrono::milliseconds timeout)
    {
        auto const until = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < until)
        {
            std::ifstream reading(output);
            std::string line;
            while (std::getline(reading, line))
                if (line.rfind("at ", 0) == 0)
                    return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    }

    inline std::optional<double> ProcessorAt(std::vector<std::pair<double, double>> const& marks, double steady)
    {
        if (marks.size() < 2 || steady < marks.front().first || steady > marks.back().first)
            return std::nullopt;
        for (std::size_t index = 1; index < marks.size(); ++index)
        {
            auto const& [endAt, endUsed] = marks[index];
            if (steady > endAt)
                continue;
            auto const& [startAt, startUsed] = marks[index - 1];
            double const span = endAt - startAt;
            return span <= 0.0 ? endUsed : startUsed + (endUsed - startUsed) * (steady - startAt) / span;
        }
        return std::nullopt;
    }

    inline std::optional<double> ShareBetween(std::filesystem::path const& output, std::uint64_t fromMicroseconds, std::uint64_t toMicroseconds)
    {
        std::ifstream reading(output);
        std::vector<std::pair<double, double>> marks;
        std::string line;
        while (std::getline(reading, line))
        {
            if (line.rfind("at ", 0) != 0)
                continue;
            std::istringstream fields(line.substr(3));
            long long steady = 0;
            long long used = 0;
            if (fields >> steady >> used && used >= 0)
                marks.emplace_back(static_cast<double>(steady), static_cast<double>(used));
        }
        std::optional<double> const from = ProcessorAt(marks, static_cast<double>(fromMicroseconds));
        std::optional<double> const to = ProcessorAt(marks, static_cast<double>(toMicroseconds));
        if (!from || !to || toMicroseconds <= fromMicroseconds)
            return std::nullopt;
        return (*to - *from) * 100.0 / static_cast<double>(toMicroseconds - fromMicroseconds);
    }
}

#endif

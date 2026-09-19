/*
 * Project Ambrose by Imjustchico
 * Watches an Ambrose log and reports refused or unhandled client messages.
 */

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <thread>
#include <vector>

struct MessageStatistics {
    std::string name;
    int count = 0;
};

static std::optional<std::string> ExtractMessageName(const std::string& line) {
    std::regex pattern(
        R"(Unknown message \(([^)]+)\)|sent ([A-Za-z_][A-Za-z0-9_:.-]*), which .*(?:does not handle yet|never accepts|only the server sends))",
        std::regex::icase);

    std::smatch match;
    if (std::regex_search(line, match, pattern)) {
        for (std::size_t i = 1; i < match.size(); ++i) {
            if (!match[i].str().empty()) {
                return match[i].str();
            }
        }
    }

    return std::nullopt;
}

static bool IsRejectedMessage(const std::string& line) {
    static const std::regex pattern(
        R"(Unknown message \(|sent [A-Za-z_][A-Za-z0-9_:.-]*, which .*(?:does not handle yet|never accepts|only the server sends))",
        std::regex::icase);
    return std::regex_search(line, pattern);
}

static void PrintSummary(const std::map<std::string, int>& counts) {
    std::vector<MessageStatistics> statistics;
    statistics.reserve(counts.size());
    for (const auto& entry : counts) {
        statistics.push_back({entry.first, entry.second});
    }

    std::sort(statistics.begin(), statistics.end(), [](const MessageStatistics& left, const MessageStatistics& right) {
        if (left.count != right.count) {
            return left.count > right.count;
        }
        return left.name < right.name;
    });

    std::cout << "Refused or unhandled messages:\n";
    for (const auto& entry : statistics) {
        std::cout << entry.name << " : " << entry.count << '\n';
    }
}

static bool ProcessLine(const std::string& line, std::map<std::string, int>& counts, bool printEvent) {
    if (!IsRejectedMessage(line)) {
        return false;
    }

    std::optional<std::string> const name = ExtractMessageName(line);
    if (!name) {
        return false;
    }

    ++counts[*name];
    if (printEvent) {
        std::cout << *name << '\n';
    }
    return true;
}

static int WatchFile(const char* path, bool follow) {
    std::ifstream input(path);
    if (!input) {
        std::cerr << "Could not open log file: " << path << '\n';
        return 1;
    }

    std::map<std::string, int> counts;
    std::string line;
    while (std::getline(input, line)) {
        ProcessLine(line, counts, follow);
    }

    if (!follow) {
        PrintSummary(counts);
        return 0;
    }

    input.clear();
    while (true) {
        while (std::getline(input, line)) {
            ProcessLine(line, counts, true);
        }
        input.clear();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ambrose-message-watcher <logfile> [--follow]\n";
        return 2;
    }

    bool follow = false;
    if (argc >= 3) {
        if (std::string(argv[2]) != "--follow") {
            std::cerr << "Unknown option: " << argv[2] << '\n';
            return 2;
        }
        follow = true;
    }

    return WatchFile(argv[1], follow);
}

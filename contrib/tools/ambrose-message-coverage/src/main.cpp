/*
 * Project Ambrose by Imjustchico
 * Parses Ambrose network.opcode logs and reports message coverage categories.
 */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

struct Coverage {
    std::set<std::string> received;
    std::set<std::string> sent;
    std::set<std::string> refused;
    std::set<std::string> unhandled;
};

static bool IsOpcodeLine(const std::string& line) {
    return line.find("network.opcode") != std::string::npos ||
        line.find("[network.opcode") != std::string::npos;
}

static std::string FirstMatch(const std::string& line, const std::regex& pattern) {
    std::smatch match;
    if (std::regex_search(line, match, pattern) && match.size() > 1) {
        return match[1].str();
    }
    return {};
}

static std::string MessageName(const std::string& line) {
    static const std::regex unknown(R"(Unknown message \(([^)]+)\))", std::regex::icase);
    static const std::regex named(
        R"((?:received|sent|sending)\s+([A-Za-z_][A-Za-z0-9_:.-]*))",
        std::regex::icase);
    const std::string unknownName = FirstMatch(line, unknown);
    return unknownName.empty() ? FirstMatch(line, named) : unknownName;
}

static void ProcessLine(const std::string& line, Coverage& coverage) {
    if (!IsOpcodeLine(line)) {
        return;
    }
    const std::string name = MessageName(line);
    if (name.empty()) {
        return;
    }
    const bool isRefused = line.find("never accepts") != std::string::npos ||
        line.find("only the server sends") != std::string::npos ||
        line.find("Unknown message") != std::string::npos;
    const bool isUnhandled = line.find("does not handle yet") != std::string::npos;
    const bool isSent = line.find("sending ") != std::string::npos;
    if (isSent) {
        coverage.sent.insert(name);
    } else {
        coverage.received.insert(name);
    }
    if (isRefused) {
        coverage.refused.insert(name);
    }
    if (isUnhandled) {
        coverage.unhandled.insert(name);
    }
}

static std::string JsonArray(const std::set<std::string>& values, bool pretty, int indent) {
    std::ostringstream output;
    const std::string pad(static_cast<std::size_t>(indent), ' ');
    const std::string itemPad(static_cast<std::size_t>(indent + 2), ' ');
    output << '[';
    if (pretty && !values.empty()) {
        output << '\n';
    }
    bool first = true;
    for (const std::string& value : values) {
        if (!first) {
            output << ',';
            if (pretty) {
                output << '\n';
            }
        }
        if (pretty) {
            output << itemPad;
        }
        output << '"' << value << '"';
        first = false;
    }
    if (pretty && !values.empty()) {
        output << '\n' << pad;
    }
    output << ']';
    return output.str();
}

static void PrintJson(const Coverage& coverage, bool pretty) {
    const std::string newline = pretty ? "\n" : "";
    const std::string pad = pretty ? "  " : "";
    std::cout << '{' << newline;
    std::cout << pad << "\"version\":1," << newline;
    std::cout << pad << "\"received\":" << JsonArray(coverage.received, pretty, 2) << ',' << newline;
    std::cout << pad << "\"sent\":" << JsonArray(coverage.sent, pretty, 2) << ',' << newline;
    std::cout << pad << "\"refused\":" << JsonArray(coverage.refused, pretty, 2) << ',' << newline;
    std::cout << pad << "\"unhandled\":" << JsonArray(coverage.unhandled, pretty, 2) << newline;
    std::cout << '}';
    if (pretty) {
        std::cout << '\n';
    }
}

static int SelfTest() {
    Coverage coverage;
    ProcessLine("12:00:00 INFO [network.opcode] Session 1 received MSG_LOGIN", coverage);
    ProcessLine("12:00:00 INFO [network.opcode] Session 1 sending MSG_WELCOME", coverage);
    ProcessLine("12:00:01 INFO [network.opcode] Session 1 sent MSG_CHARLIST, which the login server does not handle yet", coverage);
    ProcessLine("12:00:02 WARN [network.opcode] Session 1 sent MSG_SERVER_ONLY, which the client never accepts", coverage);
    ProcessLine("12:00:03 WARN [network.opcode] Unknown message (4:9)", coverage);
    ProcessLine("12:00:04 INFO [network.session] Session 1 closed", coverage);
    if (coverage.received != std::set<std::string>{"4:9", "MSG_CHARLIST", "MSG_LOGIN", "MSG_SERVER_ONLY"}) {
        return 1;
    }
    if (coverage.sent != std::set<std::string>{"MSG_WELCOME"}) {
        return 1;
    }
    if (coverage.refused != std::set<std::string>{"4:9", "MSG_SERVER_ONLY"}) {
        return 1;
    }
    if (coverage.unhandled != std::set<std::string>{"MSG_CHARLIST"}) {
        return 1;
    }
    PrintJson(coverage, false);
    return 0;
}

int main(int argc, char** argv) {
    bool pretty = false;
    bool selfTest = false;
    std::string path;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--pretty") {
            pretty = true;
        } else if (argument == "--self-test") {
            selfTest = true;
        } else if (!path.empty()) {
            std::cerr << "Unknown argument: " << argument << '\n';
            return 2;
        } else {
            path = argument;
        }
    }
    if (selfTest) {
        return SelfTest();
    }
    if (path.empty()) {
        std::cerr << "Usage: ambrose-message-coverage <logfile> [--pretty]\n";
        return 2;
    }
    std::ifstream input(path);
    if (!input) {
        std::cerr << "Could not open log file: " << path << '\n';
        return 1;
    }
    Coverage coverage;
    std::string line;
    while (std::getline(input, line)) {
        ProcessLine(line, coverage);
    }
    PrintJson(coverage, pretty);
    return 0;
}

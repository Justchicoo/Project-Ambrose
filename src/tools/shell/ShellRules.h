/*
 * Project Ambrose by Imjustchico
 * The decisions a desktop web view window makes, with no window in sight so tests can drive each one: the origin a URL belongs to, the program's own origin on each web view, where a navigation or a new window goes from a view bound to one origin, which pages the host channel answers, logging each other origin that tries once, the browser switches that keep the web view itself from calling anywhere the program did not ask, and whether a certificate the web view could not verify is accepted, which it is only when its SHA-256 fingerprint equals the pin held for that host and port.
 */

#ifndef AMBROSE_SHELLRULES_H
#define AMBROSE_SHELLRULES_H

#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>

struct ShellOrigin
{
    std::string Scheme;
    std::string Host;
    int Port = 0;

    std::string Describe() const;
    bool operator==(ShellOrigin const&) const = default;

    static std::optional<ShellOrigin> Of(std::string_view url);
    static ShellOrigin Own(std::string_view program);
    static constexpr char const* OwnDomain = "ambrose";
    static constexpr char const* OwnScheme = "ambrose";
};

enum class ShellNavigation
{
    Stay,
    SystemBrowser,
    Refuse,
};

struct ShellPinDecision
{
    bool Accept = false;
    std::string Reason;
};

class ShellRules
{
public:
    ShellRules() = delete;

    static ShellNavigation Navigate(ShellOrigin const& bound, std::string_view url, bool newWindow);
    static ShellPinDecision Certificate(ShellOrigin const& origin, std::string_view fingerprint, std::string_view pin);
    static std::string NormalFingerprint(std::string_view fingerprint);
    static std::string BrowserArguments();
};

class ShellGate
{
public:
    using Logger = std::function<void(std::string const& line)>;

    ShellGate(ShellOrigin own, bool remote, Logger log);

    bool Admit(std::string_view sourceUrl);

private:
    ShellOrigin _own;
    bool _remote = false;
    Logger _log;
    std::mutex _mutex;
    std::set<std::string> _logged;
};

#endif

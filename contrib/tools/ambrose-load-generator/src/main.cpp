/*
 * Project Ambrose by Imjustchico
 * Runs bounded TCP sessions and reports connection, request, timeout, and latency metadata.
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket InvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr Socket InvalidSocket = -1;
#endif

struct Options
{
    std::string host = "127.0.0.1";
    int port = 0;
    int clients = 1;
    int requests = 1;
    int timeout_ms = 1000;
    int response_bytes = 0;
    bool allow_private = false;
    bool self_test = false;
    std::vector<std::uint8_t> payload;
};

struct Stats
{
    std::atomic<int> attempts = 0;
    std::atomic<int> connections = 0;
    std::atomic<int> successes = 0;
    std::atomic<int> failures = 0;
    std::atomic<int> timeouts = 0;
    std::mutex latency_mutex;
    std::vector<double> latencies_ms;
};

void close_socket(Socket socket)
{
    if (socket == InvalidSocket)
        return;
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

void fail(const std::string& message)
{
    throw std::runtime_error(message);
}

int parse_int(const std::string& text, const char* name, int minimum, int maximum)
{
    std::size_t consumed = 0;
    long value = 0;
    try
    {
        value = std::stol(text, &consumed);
    }
    catch (const std::exception&)
    {
        fail(std::string(name) + " must be an integer");
    }
    if (consumed != text.size() || value < minimum || value > maximum)
        fail(std::string(name) + " is outside its allowed bounds");
    return static_cast<int>(value);
}

std::vector<std::uint8_t> parse_hex(const std::string& text)
{
    if (text.empty() || text.size() > 8192 || text.size() % 2 != 0)
        fail("payload-hex must contain 1 to 4096 bytes");
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2)
    {
        auto digit = [](char character) -> int
        {
            if (character >= '0' && character <= '9')
                return character - '0';
            if (character >= 'a' && character <= 'f')
                return character - 'a' + 10;
            if (character >= 'A' && character <= 'F')
                return character - 'A' + 10;
            return -1;
        };
        int high = digit(text[index]);
        int low = digit(text[index + 1]);
        if (high < 0 || low < 0)
            fail("payload-hex contains a non-hexadecimal character");
        bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return bytes;
}

bool is_loopback(const std::string& host)
{
    return host == "127.0.0.1" || host == "::1";
}

bool is_private_ipv4(const std::string& host)
{
    in_addr address{};
    if (inet_pton(AF_INET, host.c_str(), &address) != 1)
        return false;
    std::uint32_t value = ntohl(address.s_addr);
    return (value >= 0x0A000000 && value <= 0x0AFFFFFF) ||
           (value >= 0xAC100000 && value <= 0xAC1FFFFF) ||
           (value >= 0xC0A80000 && value <= 0xC0A8FFFF);
}

bool is_private_ipv6(const std::string& host)
{
    in6_addr address{};
    if (inet_pton(AF_INET6, host.c_str(), &address) != 1)
        return false;
    return (address.s6_addr[0] & 0xFE) == 0xFC;
}

Options parse_options(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index)
    {
        std::string argument = argv[index];
        auto value = [&](const char* name) -> std::string
        {
            if (index + 1 >= argc)
                fail(std::string(name) + " needs a value");
            return argv[++index];
        };
        if (argument == "--host")
            options.host = value("--host");
        else if (argument == "--port")
            options.port = parse_int(value("--port"), "port", 1, 65535);
        else if (argument == "--clients")
            options.clients = parse_int(value("--clients"), "clients", 1, 256);
        else if (argument == "--requests")
            options.requests = parse_int(value("--requests"), "requests", 1, 10000);
        else if (argument == "--timeout-ms")
            options.timeout_ms = parse_int(value("--timeout-ms"), "timeout-ms", 1, 60000);
        else if (argument == "--response-bytes")
            options.response_bytes = parse_int(value("--response-bytes"), "response-bytes", 0, 4096);
        else if (argument == "--payload-hex")
            options.payload = parse_hex(value("--payload-hex"));
        else if (argument == "--allow-private")
            options.allow_private = true;
        else if (argument == "--self-test")
            options.self_test = true;
        else
            fail("unknown argument: " + argument);
    }
    if (options.self_test)
        return options;
    if (options.port == 0)
        fail("--port is required");
    if (options.payload.empty())
        fail("--payload-hex is required");
    if (!is_loopback(options.host) && (!options.allow_private || (!is_private_ipv4(options.host) && !is_private_ipv6(options.host))))
        fail("host must be loopback, or a private address with --allow-private");
    return options;
}

Socket connect_socket(const Options& options)
{
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    addrinfo* results = nullptr;
    std::string service = std::to_string(options.port);
    if (getaddrinfo(options.host.c_str(), service.c_str(), &hints, &results) != 0)
        return InvalidSocket;
    Socket socket = InvalidSocket;
    for (addrinfo* current = results; current != nullptr; current = current->ai_next)
    {
        socket = static_cast<Socket>(::socket(current->ai_family, current->ai_socktype, current->ai_protocol));
        if (socket == InvalidSocket)
            continue;
#ifdef _WIN32
        DWORD timeout = static_cast<DWORD>(options.timeout_ms);
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        timeval timeout{};
        timeout.tv_sec = options.timeout_ms / 1000;
        timeout.tv_usec = (options.timeout_ms % 1000) * 1000;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#endif
        if (::connect(socket, current->ai_addr, static_cast<int>(current->ai_addrlen)) == 0)
            break;
        close_socket(socket);
        socket = InvalidSocket;
    }
    freeaddrinfo(results);
    return socket;
}

bool send_all(Socket socket, const std::vector<std::uint8_t>& payload)
{
    std::size_t sent = 0;
    while (sent < payload.size())
    {
        int count = ::send(socket, reinterpret_cast<const char*>(payload.data() + sent),
                            static_cast<int>(payload.size() - sent), 0);
        if (count <= 0)
            return false;
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

bool receive_exact(Socket socket, int expected, int timeout_ms)
{
    std::vector<char> buffer(static_cast<std::size_t>(expected));
    std::size_t received = 0;
    while (received < buffer.size())
    {
        int count = ::recv(socket, buffer.data() + received,
                           static_cast<int>(buffer.size() - received), 0);
        if (count <= 0)
            return false;
        received += static_cast<std::size_t>(count);
    }
    (void)timeout_ms;
    return true;
}

void run_client(const Options& options, Stats& stats)
{
    for (int request = 0; request < options.requests; ++request)
    {
        ++stats.attempts;
        auto started = std::chrono::steady_clock::now();
        Socket socket = connect_socket(options);
        if (socket == InvalidSocket)
        {
            ++stats.failures;
            continue;
        }
        ++stats.connections;
        bool success = send_all(socket, options.payload) &&
                       (options.response_bytes == 0 || receive_exact(socket, options.response_bytes, options.timeout_ms));
        close_socket(socket);
        double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        if (success)
        {
            ++stats.successes;
            std::lock_guard lock(stats.latency_mutex);
            stats.latencies_ms.push_back(elapsed);
        }
        else
        {
            ++stats.failures;
            ++stats.timeouts;
        }
    }
}

int percentile(std::vector<double> values, double fraction)
{
    if (values.empty())
        return 0;
    std::sort(values.begin(), values.end());
    std::size_t index = static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1));
    return static_cast<int>(values[index] + 0.5);
}

int run(const Options& options)
{
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        fail("Winsock initialization failed");
#endif
    Stats stats;
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(options.clients));
    for (int client = 0; client < options.clients; ++client)
        workers.emplace_back(run_client, std::cref(options), std::ref(stats));
    for (auto& worker : workers)
        worker.join();
#ifdef _WIN32
    WSACleanup();
#endif
    std::vector<double> latencies;
    {
        std::lock_guard lock(stats.latency_mutex);
        latencies = stats.latencies_ms;
    }
    std::cout << "attempts=" << stats.attempts << " connections=" << stats.connections
              << " successes=" << stats.successes << " failures=" << stats.failures
              << " timeouts=" << stats.timeouts << '\n';
    std::cout << "latency_ms_p50=" << percentile(latencies, 0.50)
              << " latency_ms_p95=" << percentile(latencies, 0.95)
              << " latency_ms_max=" << percentile(latencies, 1.0) << '\n';
    return stats.failures == 0 ? 0 : 1;
}

int main(int argc, char** argv)
{
    try
    {
        Options options = parse_options(argc, argv);
        if (options.self_test)
        {
            if (parse_hex("00aF").size() != 2)
                fail("hex parser self-test failed");
            try
            {
                parse_hex("0");
                fail("odd hex self-test failed");
            }
            catch (const std::runtime_error&)
            {
            }
            std::cout << "self-test: passed\n";
            return 0;
        }
        return run(options);
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}

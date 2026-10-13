/*
 * Project Ambrose by Imjustchico
 * Tests the reverse-proxy configs the panel writes: every product's template forwards only to the panel listener with the event socket's upgrade passed, timeouts longer than its 60-second silent close, buffering off for the socket, a body limit matching the listener's and HSTS set in one place only, none of the six forwards the apps' control listeners or /metrics, and the nginx, Caddy, HAProxy and Apache texts pass each product's own configuration check.
 */

#include "PanelProxy.h"

#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    ProxyConfigParams ParamsFor(ProxyProduct product)
    {
        ProxyConfigParams params;
        params.Product = product;
        params.Hostname = "panel.example.com";
        params.PanelPort = 12080;
        params.MaxBodyBytes = 262144;
        return params;
    }

    std::string Lowered(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    std::string WithoutComments(std::string const& config, ProxyProduct product)
    {
        std::string kept;
        std::string line;
        bool inComment = false;
        for (std::size_t i = 0; i <= config.size(); ++i)
        {
            char const c = i < config.size() ? config[i] : '\n';
            if (product == ProxyProduct::IIS)
            {
                if (!inComment && config.compare(i, 4, "<!--") == 0)
                    inComment = true;
                if (inComment)
                {
                    if (config.compare(i, 3, "-->") == 0)
                    {
                        inComment = false;
                        i += 2;
                    }
                    continue;
                }
            }
            if (c == '\n')
            {
                std::string_view trimmed = line;
                while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
                    trimmed.remove_prefix(1);
                if (!trimmed.starts_with('#'))
                    kept += line + "\n";
                line.clear();
            }
            else
                line += c;
        }
        return kept;
    }

    std::size_t CountOf(std::string const& haystack, std::string const& needle)
    {
        std::size_t count = 0;
        for (std::size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + needle.size()))
            ++count;
        return count;
    }

    std::string FindBinary(std::initializer_list<char const*> names)
    {
        for (char const* name : names)
        {
            std::string const direct(name);
            if (std::filesystem::exists(direct))
                return direct;
        }
        for (char const* name : names)
        {
            std::string const base = std::filesystem::path(name).filename().string();
            for (char const* dir : { "/usr/sbin", "/usr/bin", "/usr/local/bin", "/opt/proxy-bins" })
            {
                std::string const candidate = std::string(dir) + "/" + base;
                if (std::filesystem::exists(candidate))
                    return candidate;
            }
        }
        return {};
    }

    bool RunCheck(std::string const& command)
    {
        std::string const silenced = command + " >/dev/null 2>&1";
        return std::system(silenced.c_str()) == 0;
    }

    std::string WriteTemp(LogTestDirectory& directory, std::string const& name, std::string const& content)
    {
        std::filesystem::path const path = directory.Path() / name;
        std::ofstream out(path, std::ios::binary);
        out << content;
        out.close();
        return path.string();
    }

    void MakeSelfSigned(std::string const& cert, std::string const& key)
    {
        std::string const command = "openssl req -x509 -newkey rsa:2048 -keyout '" + key + "' -out '" + cert +
            "' -days 2 -nodes -subj '/CN=panel.example.com' >/dev/null 2>&1";
        ASSERT_EQ(std::system(command.c_str()), 0) << "openssl could not make a test certificate";
    }
}

TEST(PanelProxyConfigTest, EveryTemplateForwardsOnlyToThePanelListener)
{
    for (ProxyProduct product : AllProxyProducts())
    {
        std::string const config = RenderProxyConfig(ParamsFor(product));
        std::string const code = Lowered(WithoutComments(config, product));
        EXPECT_NE(config.find("12080"), std::string::npos) << NameOf(product) << " names no panel port";
        EXPECT_EQ(code.find("/metrics"), std::string::npos) << NameOf(product) << " forwards /metrics";
        EXPECT_EQ(code.find("admin"), std::string::npos) << NameOf(product) << " forwards an admin listener";
        EXPECT_NE(config.find("/api/panel/events"), std::string::npos) << NameOf(product) << " says nothing about the event socket";
        EXPECT_NE(code.find("90"), std::string::npos) << NameOf(product) << " sets no timeout past the socket's 60-second silent close";
        EXPECT_NE(config.find("262144"), std::string::npos) << NameOf(product) << " carries no body limit matching the listener's";
        if (product == ProxyProduct::Caddy)
        {
            EXPECT_EQ(CountOf(code, "strict-transport-security"), 0u) << "caddy sets HSTS itself, so the template must not set it again";
        }
        else if (product == ProxyProduct::Traefik)
        {
            EXPECT_EQ(CountOf(code, "stsseconds"), 1u) << "traefik must set HSTS in exactly one place";
        }
        else
        {
            EXPECT_EQ(CountOf(code, "strict-transport-security"), 1u) << NameOf(product) << " must set HSTS in exactly one place";
        }
    }
}

TEST(PanelProxyConfigTest, TemplatesLeaveTheControlListenersAndMetricsUnproxied)
{
    for (ProxyProduct product : AllProxyProducts())
    {
        ProxyConfigParams params = ParamsFor(product);
        params.IncludePatchserver = true;
        params.PatchserverPort = 12500;
        std::string const code = Lowered(WithoutComments(RenderProxyConfig(params), product));
        EXPECT_EQ(code.find("/metrics"), std::string::npos) << NameOf(product) << " forwards /metrics";
        EXPECT_EQ(code.find("12510"), std::string::npos) << NameOf(product) << " forwards a control listener port";
    }
}

TEST(PanelProxyConfigTest, NginxConfigPassesNginxOwnCheck)
{
    std::string const nginx = FindBinary({ "nginx", "/usr/sbin/nginx" });
    if (nginx.empty())
        GTEST_SKIP() << "nginx is not installed";
    LogTestDirectory directory;
    std::string const cert = (directory.Path() / "panel.crt").string();
    std::string const key = (directory.Path() / "panel.key").string();
    MakeSelfSigned(cert, key);
    ProxyConfigParams params = ParamsFor(ProxyProduct::Nginx);
    params.CertificateFile = cert;
    params.CertificateKeyFile = key;
    std::string const file = WriteTemp(directory, "ambrose-panel-nginx.conf", RenderProxyConfig(params));
    EXPECT_TRUE(RunCheck("'" + nginx + "' -t -c '" + file + "'")) << "nginx -t refused the generated config";
}

TEST(PanelProxyConfigTest, CaddyConfigPassesCaddyOwnCheck)
{
    std::string const caddy = FindBinary({ "caddy", "/opt/proxy-bins/caddy" });
    if (caddy.empty())
        GTEST_SKIP() << "caddy is not installed";
    LogTestDirectory directory;
    std::string const file = WriteTemp(directory, "Caddyfile", RenderProxyConfig(ParamsFor(ProxyProduct::Caddy)));
    EXPECT_TRUE(RunCheck("'" + caddy + "' validate --config --adapter caddyfile --file '" + file + "'"))
        << "caddy validate refused the generated Caddyfile";
}

TEST(PanelProxyConfigTest, HAProxyConfigPassesHAProxyOwnCheck)
{
    std::string const haproxy = FindBinary({ "haproxy", "/usr/sbin/haproxy" });
    if (haproxy.empty())
        GTEST_SKIP() << "haproxy is not installed";
    LogTestDirectory directory;
    std::string const cert = (directory.Path() / "panel.crt").string();
    std::string const key = (directory.Path() / "panel.key").string();
    MakeSelfSigned(cert, key);
    std::string const pem = (directory.Path() / "panel.pem").string();
    {
        std::ifstream certIn(cert, std::ios::binary);
        std::ifstream keyIn(key, std::ios::binary);
        std::ofstream pemOut(pem, std::ios::binary);
        pemOut << certIn.rdbuf() << keyIn.rdbuf();
    }
    ProxyConfigParams params = ParamsFor(ProxyProduct::HAProxy);
    params.CertificateFile = pem;
    std::string const file = WriteTemp(directory, "haproxy.cfg", RenderProxyConfig(params));
    EXPECT_TRUE(RunCheck("'" + haproxy + "' -c -f '" + file + "'")) << "haproxy -c refused the generated config";
}

TEST(PanelProxyConfigTest, ApacheConfigPassesApacheOwnCheck)
{
    std::string const apache = FindBinary({ "apache2", "/usr/sbin/apache2" });
    if (apache.empty())
        GTEST_SKIP() << "apache2 is not installed";
    LogTestDirectory directory;
    std::string const cert = (directory.Path() / "panel.crt").string();
    std::string const key = (directory.Path() / "panel.key").string();
    MakeSelfSigned(cert, key);
    ProxyConfigParams params = ParamsFor(ProxyProduct::Apache);
    params.CertificateFile = cert;
    params.CertificateKeyFile = key;
    std::string const file = WriteTemp(directory, "ambrose-panel-apache.conf", RenderProxyConfig(params));
    EXPECT_TRUE(RunCheck("'" + apache + "' -t -f '" + file + "'")) << "apache2 -t refused the generated config";
}

TEST(PanelProxyConfigTest, UnknownProductsAreRefusedAndNamesAreParsed)
{
    EXPECT_FALSE(ParseProxyProduct("squid").has_value());
    EXPECT_EQ(ParseProxyProduct("NGINX"), ProxyProduct::Nginx);
    EXPECT_EQ(ParseProxyProduct("httpd"), ProxyProduct::Apache);
    EXPECT_EQ(ParseProxyProduct("iis-arr"), ProxyProduct::IIS);
    EXPECT_EQ(AllProxyProducts().size(), 6u);
}

/*
 * Project Ambrose by Imjustchico
 * Tests the panel's forwarded-header connection check: the peer, every forwarded header it arrived with, whether the peer is trusted, the client address, scheme and host that result, each compared with the public URL, and every mismatch explained with its fix, including the untrusted-proxy case that also writes a problem record.
 */

#include "PanelProxy.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

namespace
{
    ProxyCheckInput UntrustedInput()
    {
        ProxyCheckInput input;
        input.Peer = "127.0.0.1";
        input.ForwardedFor = "203.0.113.7";
        input.ForwardedProto = "https";
        input.ForwardedHost = "panel.example.com";
        input.TrustedProxies = "";
        input.PublicUrl = "https://panel.example.com";
        input.HostHeader = "127.0.0.1:12080";
        return input;
    }

    bool HasProblem(nlohmann::json const& answer, std::string const& code)
    {
        for (auto const& problem : answer["problems"])
            if (problem["code"] == code)
                return true;
        return false;
    }
}

TEST(PanelProxyCheckTest, UntrustedPeerKeepsTheProxyAddressAndNamesTheFix)
{
    nlohmann::json const answer = CheckProxyRequest(UntrustedInput());
    EXPECT_EQ(answer["peer"], "127.0.0.1");
    EXPECT_FALSE(answer["peer_trusted"].get<bool>());
    EXPECT_EQ(answer["client_address"], "127.0.0.1");
    EXPECT_EQ(answer["forwarded_headers"]["x_forwarded_for"], "203.0.113.7");
    EXPECT_EQ(answer["forwarded_headers"]["x_forwarded_proto"], "https");
    EXPECT_EQ(answer["forwarded_headers"]["x_forwarded_host"], "panel.example.com");
    ASSERT_TRUE(HasProblem(answer, "untrusted_proxy")) << answer.dump();
    for (auto const& problem : answer["problems"])
        if (problem["code"] == "untrusted_proxy")
        {
            EXPECT_NE(std::string(problem["fix"]).find("127.0.0.1"), std::string::npos) << problem.dump();
            EXPECT_NE(std::string(problem["message"]).find("127.0.0.1"), std::string::npos) << problem.dump();
        }
}

TEST(PanelProxyCheckTest, TrustedPeerResolvesTheRealClientAddress)
{
    ProxyCheckInput input = UntrustedInput();
    input.TrustedProxies = "127.0.0.1";
    nlohmann::json const answer = CheckProxyRequest(input);
    EXPECT_TRUE(answer["peer_trusted"].get<bool>());
    EXPECT_EQ(answer["client_address"], "203.0.113.7");
    EXPECT_EQ(answer["scheme"], "https");
    EXPECT_EQ(answer["host"], "panel.example.com");
    EXPECT_FALSE(HasProblem(answer, "untrusted_proxy")) << answer.dump();
    EXPECT_TRUE(answer["problems"].empty()) << answer.dump();
}

TEST(PanelProxyCheckTest, HostAndSchemeMismatchesNameBothValues)
{
    ProxyCheckInput input = UntrustedInput();
    input.TrustedProxies = "127.0.0.1";
    input.ForwardedProto = "http";
    input.ForwardedHost = "other.example.net";
    nlohmann::json const answer = CheckProxyRequest(input);
    ASSERT_TRUE(HasProblem(answer, "scheme_mismatch")) << answer.dump();
    ASSERT_TRUE(HasProblem(answer, "host_mismatch")) << answer.dump();
    for (auto const& problem : answer["problems"])
    {
        if (problem["code"] == "scheme_mismatch")
        {
            EXPECT_NE(std::string(problem["message"]).find("http"), std::string::npos);
            EXPECT_NE(std::string(problem["message"]).find("https"), std::string::npos);
        }
        if (problem["code"] == "host_mismatch")
        {
            EXPECT_NE(std::string(problem["message"]).find("other.example.net"), std::string::npos);
            EXPECT_NE(std::string(problem["message"]).find("panel.example.com"), std::string::npos);
        }
    }
}

TEST(PanelProxyCheckTest, TrustedProxyWithoutAProtoIsFlagged)
{
    ProxyCheckInput input = UntrustedInput();
    input.TrustedProxies = "127.0.0.1";
    input.ForwardedProto.clear();
    nlohmann::json const answer = CheckProxyRequest(input);
    EXPECT_TRUE(HasProblem(answer, "missing_forwarded_proto")) << answer.dump();
}

TEST(PanelProxyCheckTest, DirectConnectionWithNoForwardedHeadersIsQuiet)
{
    ProxyCheckInput input = UntrustedInput();
    input.ForwardedFor.clear();
    input.ForwardedProto.clear();
    input.ForwardedHost.clear();
    nlohmann::json const answer = CheckProxyRequest(input);
    EXPECT_TRUE(answer["forwarded_headers"].empty());
    EXPECT_TRUE(answer["problems"].empty()) << answer.dump();
    EXPECT_EQ(answer["client_address"], "127.0.0.1");
}

TEST(PanelProxyCheckTest, TrustedNetworksMatchByPrefix)
{
    ProxyCheckInput input = UntrustedInput();
    input.Peer = "10.1.2.3";
    input.TrustedProxies = "10.0.0.0/8";
    nlohmann::json const answer = CheckProxyRequest(input);
    EXPECT_TRUE(answer["peer_trusted"].get<bool>());
    EXPECT_EQ(answer["client_address"], "203.0.113.7");
}

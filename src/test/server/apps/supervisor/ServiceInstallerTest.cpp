/*
 * Project Ambrose by Imjustchico
 * Tests shared service argument filtering, systemd unit generation, and configuration value rewriting used by service installation.
 */

#include "ServiceInstaller.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(ServiceInstallerTest, RemovesServiceFlagWithoutChangingOtherArguments)
{
    std::vector<std::string> const arguments{
        "supervisor", "--service", "--config", "settings/supervisor.conf", "--service=ignored"
    };

    EXPECT_EQ(SupervisorService::ArgumentsWithoutServiceFlag(arguments),
        (std::vector<std::string>{ "supervisor", "--config", "settings/supervisor.conf", "--service=ignored" }));
}

TEST(ServiceInstallerTest, ConsoleRunnerReceivesNormalizedArguments)
{
    std::vector<std::string> received;
    int const result = SupervisorService::Run(
        { "supervisor", "--service", "--check" },
        [&received](std::vector<std::string> const& arguments)
        {
            received = arguments;
            return 7;
        },
        [] {});

    EXPECT_EQ(result, 7);
    EXPECT_EQ(received, (std::vector<std::string>{ "supervisor", "--check" }));
}

TEST(ServiceInstallerTest, SystemdUnitQuotesExecutableAndConfigPaths)
{
    std::string const unit = SupervisorService::BuildSystemdUnitText(
        "/opt/Ambrose Server/supervisor",
        "/etc/ambrose/supervisor.conf");

    EXPECT_NE(unit.find("ExecStart=\"/opt/Ambrose Server/supervisor\" --config \"/etc/ambrose/supervisor.conf\""),
        std::string::npos);
    EXPECT_NE(unit.find("NoNewPrivileges=true"), std::string::npos);
}

TEST(ServiceInstallerTest, RewritesConfigValuesWithSpacingAndCarriageReturns)
{
    std::string config = "Panel.Enable\t= 0\r\nOther.Value = keep\r\nPanel.Enable = 0\r\n";

    EXPECT_TRUE(SupervisorService::SetConfigValue(config, "Panel.Enable", "1"));
    EXPECT_EQ(config, "Panel.Enable = 1\r\nOther.Value = keep\r\nPanel.Enable = 1\r\n");
}

TEST(ServiceInstallerTest, MissingConfigKeyIsReportedWithoutChangingContents)
{
    std::string config = "Other.Value = keep\n";

    EXPECT_FALSE(SupervisorService::SetConfigValue(config, "Panel.Enable", "1"));
    EXPECT_EQ(config, "Other.Value = keep\n");
}

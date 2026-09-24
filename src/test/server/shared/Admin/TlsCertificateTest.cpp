/*
 * Project Ambrose by Imjustchico
 * Tests the certificate a listener serves: one written to sign itself reads back with its name, dates, chain length and SHA-256 fingerprint, and says out loud that it signed itself; a key from another certificate, a file holding no PEM, an empty file, a missing file, a certificate that has expired and a chain whose second certificate did not issue the first are each refused with the file named; and a certificate close to its last day warns how long is left.
 */

#include "LogTestDirectory.h"
#include "TlsCertificate.h"

#include <gtest/gtest.h>

#include <ctime>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    struct Pair
    {
        std::filesystem::path Certificate;
        std::filesystem::path Key;
    };

    Pair Make(LogTestDirectory const& directory, std::string const& name, int days = TlsCertificate::SelfSignedDays)
    {
        Pair pair{ directory.Path() / (name + ".crt"), directory.Path() / (name + ".key") };
        std::string error;
        EXPECT_TRUE(TlsCertificate::CreateSelfSigned(pair.Certificate, pair.Key, "Ambrose " + name, { "localhost", "127.0.0.1" }, days, error)) << error;
        return pair;
    }

    std::string ReadWhole(std::filesystem::path const& file)
    {
        std::ifstream stream(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }
}

TEST(TlsCertificateTest, WritesOneThatSignsItselfAndReadsItBack)
{
    LogTestDirectory directory;
    Pair const pair = Make(directory, "panel");

    TlsCertificate certificate;
    std::string error;
    ASSERT_TRUE(certificate.Load(pair.Certificate, pair.Key, error)) << error;
    EXPECT_TRUE(certificate.IsLoaded());
    EXPECT_EQ(certificate.GetCertificateFile(), pair.Certificate);
    EXPECT_EQ(certificate.GetKeyFile(), pair.Key);

    TlsCertificateInfo const& info = certificate.GetInfo();
    EXPECT_NE(info.Subject.find("Ambrose panel"), std::string::npos) << info.Subject;
    EXPECT_EQ(info.Subject, info.Issuer);
    EXPECT_TRUE(info.SelfSigned);
    EXPECT_EQ(info.ChainLength, 1u);
    EXPECT_EQ(info.Fingerprint.size(), 95u) << info.Fingerprint;
    EXPECT_EQ(info.Fingerprint.find_first_not_of("0123456789ABCDEF:"), std::string::npos) << info.Fingerprint;
    EXPECT_LT(info.NotBefore, info.NotAfter);

    int64 const now = static_cast<int64>(std::time(nullptr));
    EXPECT_GT(certificate.SecondsLeft(now), int64{ 86400 } * 800);
    EXPECT_NE(certificate.Describe().find(info.Fingerprint), std::string::npos);

    std::vector<std::string> const warnings = certificate.Warnings(now);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings.front().find("signed itself"), std::string::npos) << warnings.front();
    EXPECT_NE(warnings.front().find(info.Fingerprint), std::string::npos) << warnings.front();
}

TEST(TlsCertificateTest, WarnsWhileTheLastDaysRunDown)
{
    LogTestDirectory directory;
    Pair const pair = Make(directory, "soon", 10);

    TlsCertificate certificate;
    std::string error;
    ASSERT_TRUE(certificate.Load(pair.Certificate, pair.Key, error)) << error;
    std::vector<std::string> const warnings = certificate.Warnings(static_cast<int64>(std::time(nullptr)));
    ASSERT_EQ(warnings.size(), 2u);
    EXPECT_NE(warnings.front().find("expires in 10 days"), std::string::npos) << warnings.front();
    EXPECT_NE(warnings.front().find("soon.crt"), std::string::npos) << warnings.front();
}

TEST(TlsCertificateTest, RefusesAKeyThatBelongsToAnotherCertificate)
{
    LogTestDirectory directory;
    Pair const first = Make(directory, "first");
    Pair const second = Make(directory, "second");

    TlsCertificate certificate;
    std::string error;
    EXPECT_FALSE(certificate.Load(first.Certificate, second.Key, error));
    EXPECT_FALSE(certificate.IsLoaded());
    EXPECT_NE(error.find("first.crt"), std::string::npos) << error;
    EXPECT_NE(error.find("second.key"), std::string::npos) << error;
}

TEST(TlsCertificateTest, RefusesAChainWhoseSecondCertificateDidNotIssueTheFirst)
{
    LogTestDirectory directory;
    Pair const first = Make(directory, "leaf");
    Pair const second = Make(directory, "stranger");

    std::filesystem::path const chain = directory.Path() / "chain.crt";
    {
        std::ofstream out(chain, std::ios::binary);
        out << ReadWhole(first.Certificate) << ReadWhole(second.Certificate);
    }

    TlsCertificate certificate;
    std::string error;
    EXPECT_FALSE(certificate.Load(chain, first.Key, error));
    EXPECT_NE(error.find("chain.crt"), std::string::npos) << error;
    EXPECT_NE(error.find("out of order"), std::string::npos) << error;
    EXPECT_NE(error.find("leaf first"), std::string::npos) << error;
}

TEST(TlsCertificateTest, RefusesOneThatHasExpired)
{
    LogTestDirectory directory;
    Pair const pair = Make(directory, "old", -2);

    TlsCertificate certificate;
    std::string error;
    EXPECT_FALSE(certificate.Load(pair.Certificate, pair.Key, error));
    EXPECT_NE(error.find("old.crt"), std::string::npos) << error;
    EXPECT_NE(error.find("expired on"), std::string::npos) << error;
    EXPECT_NE(error.find("UTC"), std::string::npos) << error;
}

TEST(TlsCertificateTest, RefusesFilesThatHoldNoCertificateOrKey)
{
    LogTestDirectory directory;
    Pair const pair = Make(directory, "real");
    std::filesystem::path const rubbish = directory.Write("rubbish.pem", "not a certificate\n");
    std::filesystem::path const empty = directory.Write("empty.pem", "");
    std::filesystem::path const missing = directory.Path() / "missing.pem";

    TlsCertificate certificate;
    std::string error;
    EXPECT_FALSE(certificate.Load(rubbish, pair.Key, error));
    EXPECT_NE(error.find("rubbish.pem"), std::string::npos) << error;
    EXPECT_NE(error.find("no certificate"), std::string::npos) << error;

    EXPECT_FALSE(certificate.Load(pair.Certificate, rubbish, error));
    EXPECT_NE(error.find("rubbish.pem"), std::string::npos) << error;
    EXPECT_NE(error.find("no private key"), std::string::npos) << error;

    EXPECT_FALSE(certificate.Load(empty, pair.Key, error));
    EXPECT_NE(error.find("empty.pem"), std::string::npos) << error;
    EXPECT_NE(error.find("empty"), std::string::npos) << error;

    EXPECT_FALSE(certificate.Load(missing, pair.Key, error));
    EXPECT_NE(error.find("missing.pem"), std::string::npos) << error;
    EXPECT_NE(error.find("not a file"), std::string::npos) << error;
}

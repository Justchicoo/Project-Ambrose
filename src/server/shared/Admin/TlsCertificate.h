/*
 * Project Ambrose by Imjustchico
 * A certificate and key in PEM read from disk and checked before anything serves them: the key belongs to the leaf, the chain runs leaf first with each certificate issued by the next, and the dates hold today; it keeps the text to hand to the listener, the leaf's subject, issuer, dates and SHA-256 fingerprint to print, and says how long is left so a listener warns before one expires.
 */

#ifndef AMBROSE_TLSCERTIFICATE_H
#define AMBROSE_TLSCERTIFICATE_H

#include "Types.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct TlsCertificateInfo
{
    std::string Subject;
    std::string Issuer;
    std::string Fingerprint;
    int64 NotBefore = 0;
    int64 NotAfter = 0;
    std::size_t ChainLength = 0;
    bool SelfSigned = false;
};

class TlsCertificate
{
public:
    static constexpr int64 ExpiryWarningDays = 30;
    static constexpr int SelfSignedDays = 825;

    static std::string Fingerprint(std::string_view certificatePem);

    static bool CreateSelfSigned(std::filesystem::path const& certificate, std::filesystem::path const& key, std::string const& commonName,
        std::vector<std::string> const& names, int days, std::string& error);

    bool Load(std::filesystem::path const& certificate, std::filesystem::path const& key, std::string& error);

    bool IsLoaded() const { return !_certificatePem.empty(); }
    TlsCertificateInfo const& GetInfo() const { return _info; }
    std::string const& GetCertificatePem() const { return _certificatePem; }
    std::string const& GetKeyPem() const { return _keyPem; }
    std::filesystem::path const& GetCertificateFile() const { return _certificateFile; }
    std::filesystem::path const& GetKeyFile() const { return _keyFile; }

    int64 SecondsLeft(int64 now) const;
    std::vector<std::string> Warnings(int64 now) const;
    std::string Describe() const;

private:
    std::filesystem::path _certificateFile;
    std::filesystem::path _keyFile;
    std::string _certificatePem;
    std::string _keyPem;
    TlsCertificateInfo _info;
};

#endif

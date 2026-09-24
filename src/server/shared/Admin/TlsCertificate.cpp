/*
 * Project Ambrose by Imjustchico
 * Writes a certificate that signs itself for a machine-local listener, with a generated P-256 key, the names it answers to and owner-only permissions on the key; reads both files as text, parses the certificate file as a PEM chain and the key file as one private key through OpenSSL, and refuses with a message naming the file when either will not parse, when the key does not belong to the leaf, when a certificate in the chain was not issued by the one after it, or when today falls outside the leaf's dates; on success it keeps the PEM text for the listener and reads the leaf's subject, issuer, SHA-256 fingerprint and dates out for the console and the log.
 */

#include "TlsCertificate.h"
#include "AdminToken.h"
#include "ConfigMgr.h"
#include "IpAddress.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <fmt/format.h>

#include <ctime>
#include <fstream>
#include <memory>
#include <vector>

namespace
{
    struct BioDeleter
    {
        void operator()(BIO* bio) const { BIO_free(bio); }
    };

    struct CertificateDeleter
    {
        void operator()(X509* certificate) const { X509_free(certificate); }
    };

    struct KeyDeleter
    {
        void operator()(EVP_PKEY* key) const { EVP_PKEY_free(key); }
    };

    using BioPtr = std::unique_ptr<BIO, BioDeleter>;
    using CertificatePtr = std::unique_ptr<X509, CertificateDeleter>;
    using KeyPtr = std::unique_ptr<EVP_PKEY, KeyDeleter>;

    std::string TakeOpenSslError()
    {
        std::string text;
        while (unsigned long const code = ERR_get_error())
        {
            char buffer[256] = {};
            ERR_error_string_n(code, buffer, sizeof(buffer));
            if (!text.empty())
                text += "; ";
            text += buffer;
        }
        return text.empty() ? std::string("the file is not valid PEM") : text;
    }

    bool ReadText(std::filesystem::path const& path, std::string& contents, std::string& error)
    {
        std::error_code code;
        if (!std::filesystem::is_regular_file(path, code))
        {
            error = fmt::format("{} is not a file", ConfigMgr::PathToUtf8(path));
            return false;
        }
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            error = fmt::format("{} could not be read", ConfigMgr::PathToUtf8(path));
            return false;
        }
        contents.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        if (contents.empty())
        {
            error = fmt::format("{} is empty", ConfigMgr::PathToUtf8(path));
            return false;
        }
        return true;
    }

    BioPtr MemoryBio(std::string const& text)
    {
        return BioPtr(BIO_new_mem_buf(text.data(), static_cast<int>(text.size())));
    }

    std::string NameOf(X509_NAME const* name)
    {
        if (name == nullptr)
            return {};
        BioPtr bio(BIO_new(BIO_s_mem()));
        if (!bio)
            return {};
        X509_NAME_print_ex(bio.get(), const_cast<X509_NAME*>(name), 0, XN_FLAG_RFC2253);
        char* text = nullptr;
        long const length = BIO_get_mem_data(bio.get(), &text);
        return length > 0 ? std::string(text, static_cast<std::size_t>(length)) : std::string();
    }

    std::string FingerprintOf(X509* certificate)
    {
        unsigned char digest[EVP_MAX_MD_SIZE] = {};
        unsigned int length = 0;
        if (X509_digest(certificate, EVP_sha256(), digest, &length) != 1)
            return {};
        std::string text;
        text.reserve(static_cast<std::size_t>(length) * 3);
        for (unsigned int index = 0; index < length; ++index)
        {
            if (index != 0)
                text += ':';
            text += fmt::format("{:02X}", digest[index]);
        }
        return text;
    }

    int64 EpochOf(ASN1_TIME const* time)
    {
        std::tm parts = {};
        if (time == nullptr || ASN1_TIME_to_tm(time, &parts) != 1)
            return 0;
#ifdef _WIN32
        return static_cast<int64>(_mkgmtime(&parts));
#else
        return static_cast<int64>(timegm(&parts));
#endif
    }

    std::string WhenText(int64 epoch)
    {
        std::time_t const value = static_cast<std::time_t>(epoch);
        std::tm parts = {};
#ifdef _WIN32
        gmtime_s(&parts, &value);
#else
        gmtime_r(&value, &parts);
#endif
        return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02} UTC", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
    }
}

std::string TlsCertificate::Fingerprint(std::string_view certificatePem)
{
    ERR_clear_error();
    BioPtr bio(BIO_new_mem_buf(certificatePem.data(), static_cast<int>(certificatePem.size())));
    CertificatePtr parsed(bio ? PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr) : nullptr);
    ERR_clear_error();
    return parsed ? FingerprintOf(parsed.get()) : std::string();
}

bool TlsCertificate::Load(std::filesystem::path const& certificate, std::filesystem::path const& key, std::string& error)
{
    error.clear();
    std::string certificatePem;
    std::string keyPem;
    if (!ReadText(certificate, certificatePem, error) || !ReadText(key, keyPem, error))
        return false;

    ERR_clear_error();
    BioPtr certificateBio = MemoryBio(certificatePem);
    if (!certificateBio)
    {
        error = fmt::format("{} could not be parsed: out of memory", ConfigMgr::PathToUtf8(certificate));
        return false;
    }

    std::vector<CertificatePtr> chain;
    while (X509* parsed = PEM_read_bio_X509(certificateBio.get(), nullptr, nullptr, nullptr))
        chain.emplace_back(parsed);
    if (chain.empty())
    {
        error = fmt::format("{} holds no certificate in PEM: {}", ConfigMgr::PathToUtf8(certificate), TakeOpenSslError());
        return false;
    }
    ERR_clear_error();

    BioPtr keyBio = MemoryBio(keyPem);
    KeyPtr parsedKey(keyBio ? PEM_read_bio_PrivateKey(keyBio.get(), nullptr, nullptr, nullptr) : nullptr);
    if (!parsedKey)
    {
        error = fmt::format("{} holds no private key in PEM: {}", ConfigMgr::PathToUtf8(key), TakeOpenSslError());
        return false;
    }
    ERR_clear_error();

    X509* const leaf = chain.front().get();
    if (X509_check_private_key(leaf, parsedKey.get()) != 1)
    {
        error = fmt::format("the key in {} does not belong to the certificate in {}: {}", ConfigMgr::PathToUtf8(key), ConfigMgr::PathToUtf8(certificate), TakeOpenSslError());
        return false;
    }
    ERR_clear_error();

    for (std::size_t index = 0; index + 1 < chain.size(); ++index)
    {
        if (X509_check_issued(chain[index + 1].get(), chain[index].get()) != X509_V_OK)
        {
            error = fmt::format("{} is out of order: certificate {} ({}) was not issued by certificate {} ({}); a chain runs leaf first, each certificate followed by the one that issued it",
                ConfigMgr::PathToUtf8(certificate), index + 1, NameOf(X509_get_subject_name(chain[index].get())), index + 2, NameOf(X509_get_subject_name(chain[index + 1].get())));
            return false;
        }
    }
    ERR_clear_error();

    TlsCertificateInfo info;
    info.Subject = NameOf(X509_get_subject_name(leaf));
    info.Issuer = NameOf(X509_get_issuer_name(leaf));
    info.Fingerprint = FingerprintOf(leaf);
    info.NotBefore = EpochOf(X509_get0_notBefore(leaf));
    info.NotAfter = EpochOf(X509_get0_notAfter(leaf));
    info.ChainLength = chain.size();
    info.SelfSigned = chain.size() == 1 && X509_NAME_cmp(X509_get_subject_name(leaf), X509_get_issuer_name(leaf)) == 0;
    ERR_clear_error();

    if (info.NotBefore == 0 || info.NotAfter == 0)
    {
        error = fmt::format("the certificate in {} has dates that could not be read", ConfigMgr::PathToUtf8(certificate));
        return false;
    }
    int64 const now = static_cast<int64>(std::time(nullptr));
    if (now < info.NotBefore)
    {
        error = fmt::format("the certificate in {} is not valid until {}", ConfigMgr::PathToUtf8(certificate), WhenText(info.NotBefore));
        return false;
    }
    if (now >= info.NotAfter)
    {
        error = fmt::format("the certificate in {} expired on {}", ConfigMgr::PathToUtf8(certificate), WhenText(info.NotAfter));
        return false;
    }

    _certificateFile = certificate;
    _keyFile = key;
    _certificatePem = std::move(certificatePem);
    _keyPem = std::move(keyPem);
    _info = std::move(info);
    return true;
}

int64 TlsCertificate::SecondsLeft(int64 now) const
{
    return _info.NotAfter - now;
}

std::vector<std::string> TlsCertificate::Warnings(int64 now) const
{
    std::vector<std::string> warnings;
    if (!IsLoaded())
        return warnings;
    int64 const left = SecondsLeft(now);
    if (left <= ExpiryWarningDays * 86400)
    {
        int64 const days = left / 86400;
        warnings.push_back(days > 0
            ? fmt::format("the certificate in {} expires in {} day{} on {}; replace it and reload", ConfigMgr::PathToUtf8(_certificateFile), days, days == 1 ? "" : "s", WhenText(_info.NotAfter))
            : fmt::format("the certificate in {} expires within a day on {}; replace it and reload", ConfigMgr::PathToUtf8(_certificateFile), WhenText(_info.NotAfter)));
    }
    if (_info.SelfSigned)
        warnings.push_back(fmt::format("the certificate in {} signed itself, so a browser trusts it only once someone tells it to; its SHA-256 fingerprint is {}", ConfigMgr::PathToUtf8(_certificateFile), _info.Fingerprint));
    return warnings;
}

std::string TlsCertificate::Describe() const
{
    if (!IsLoaded())
        return "no certificate";
    return fmt::format("{} issued by {}, valid until {}, SHA-256 {}{}", _info.Subject, _info.Issuer, WhenText(_info.NotAfter), _info.Fingerprint,
        _info.ChainLength > 1 ? fmt::format(", with {} certificates in the chain", _info.ChainLength) : std::string());
}

bool TlsCertificate::CreateSelfSigned(std::filesystem::path const& certificate, std::filesystem::path const& key, std::string const& commonName,
    std::vector<std::string> const& names, int days, std::string& error)
{
    error.clear();
    ERR_clear_error();
    KeyPtr generated(EVP_EC_gen("P-256"));
    if (!generated)
    {
        error = fmt::format("a key could not be generated: {}", TakeOpenSslError());
        return false;
    }

    CertificatePtr made(X509_new());
    if (!made)
    {
        error = "a certificate could not be held in memory";
        return false;
    }
    X509_set_version(made.get(), 2);
    unsigned char serial[16] = {};
    if (RAND_bytes(serial, static_cast<int>(sizeof(serial))) != 1)
    {
        error = fmt::format("a serial number could not be generated: {}", TakeOpenSslError());
        return false;
    }
    serial[0] &= 0x7F;
    std::unique_ptr<BIGNUM, decltype(&BN_free)> serialNumber(BN_bin2bn(serial, static_cast<int>(sizeof(serial)), nullptr), &BN_free);
    if (!serialNumber || BN_to_ASN1_INTEGER(serialNumber.get(), X509_get_serialNumber(made.get())) == nullptr)
    {
        error = fmt::format("a serial number could not be set: {}", TakeOpenSslError());
        return false;
    }

    X509_NAME* const subject = X509_get_subject_name(made.get());
    if (X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_UTF8, reinterpret_cast<unsigned char const*>(commonName.data()), static_cast<int>(commonName.size()), -1, 0) != 1
        || X509_set_issuer_name(made.get(), subject) != 1)
    {
        error = fmt::format("the certificate could not be named: {}", TakeOpenSslError());
        return false;
    }
    X509_gmtime_adj(X509_getm_notBefore(made.get()), -3600);
    X509_gmtime_adj(X509_getm_notAfter(made.get()), static_cast<long>(days) * 86400);

    if (X509_set_pubkey(made.get(), generated.get()) != 1)
    {
        error = fmt::format("the key could not be put in the certificate: {}", TakeOpenSslError());
        return false;
    }

    std::string alternatives;
    for (std::string const& name : names)
    {
        if (!alternatives.empty())
            alternatives += ',';
        alternatives += Ambrose::Asio::MakeAddress(name) ? fmt::format("IP:{}", name) : fmt::format("DNS:{}", name);
    }
    X509V3_CTX context;
    X509V3_set_ctx_nodb(&context);
    X509V3_set_ctx(&context, made.get(), made.get(), nullptr, nullptr, 0);
    auto const addExtension = [&](int identifier, char const* value)
    {
        std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> extension(X509V3_EXT_conf_nid(nullptr, &context, identifier, value), &X509_EXTENSION_free);
        return extension && X509_add_ext(made.get(), extension.get(), -1) == 1;
    };
    if (!addExtension(NID_basic_constraints, "critical,CA:FALSE")
        || !addExtension(NID_key_usage, "critical,digitalSignature,keyEncipherment")
        || !addExtension(NID_ext_key_usage, "serverAuth")
        || !addExtension(NID_subject_key_identifier, "hash")
        || (!alternatives.empty() && !addExtension(NID_subject_alt_name, alternatives.c_str())))
    {
        error = fmt::format("the certificate's extensions could not be set: {}", TakeOpenSslError());
        return false;
    }

    if (X509_sign(made.get(), generated.get(), EVP_sha256()) == 0)
    {
        error = fmt::format("the certificate could not be signed: {}", TakeOpenSslError());
        return false;
    }

    BioPtr certificateBio(BIO_new(BIO_s_mem()));
    BioPtr keyBio(BIO_new(BIO_s_mem()));
    if (!certificateBio || !keyBio || PEM_write_bio_X509(certificateBio.get(), made.get()) != 1
        || PEM_write_bio_PrivateKey(keyBio.get(), generated.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1)
    {
        error = fmt::format("the certificate could not be written out: {}", TakeOpenSslError());
        return false;
    }

    auto const textOf = [](BIO* bio)
    {
        char* text = nullptr;
        long const length = BIO_get_mem_data(bio, &text);
        return length > 0 ? std::string(text, static_cast<std::size_t>(length)) : std::string();
    };
    std::error_code code;
    if (std::filesystem::path const folder = certificate.parent_path(); !folder.empty())
        std::filesystem::create_directories(folder, code);
    if (std::filesystem::path const folder = key.parent_path(); !folder.empty())
        std::filesystem::create_directories(folder, code);
    std::ofstream out(certificate, std::ios::binary | std::ios::trunc);
    out << textOf(certificateBio.get());
    if (!out)
    {
        error = fmt::format("{} could not be written", ConfigMgr::PathToUtf8(certificate));
        return false;
    }
    out.close();
    return AdminToken::WriteSecretFile(key, textOf(keyBio.get()), error);
}

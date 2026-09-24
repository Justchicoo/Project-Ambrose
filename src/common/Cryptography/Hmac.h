/*
 * Project Ambrose by Imjustchico
 * HMAC over SHA-256 or SHA-512 with incremental updates, for signing tokens and verifying messages.
 */

#ifndef AMBROSE_HMAC_H
#define AMBROSE_HMAC_H

#include "CryptoHash.h"

#include <memory>
#include <span>
#include <string_view>
#include <vector>

class Hmac
{
public:
    Hmac(CryptoHash::Algorithm algorithm, std::span<uint8 const> key);
    ~Hmac();

    Hmac(Hmac&& other) noexcept;
    Hmac& operator=(Hmac&& other) noexcept;
    Hmac(Hmac const&) = delete;
    Hmac& operator=(Hmac const&) = delete;

    Hmac& Update(std::span<uint8 const> data);
    Hmac& Update(std::string_view data);
    std::vector<uint8> Finalize();
    std::size_t GetLength() const noexcept;

    static std::vector<uint8> Compute(CryptoHash::Algorithm algorithm, std::span<uint8 const> key, std::span<uint8 const> data);
    static bool Verify(CryptoHash::Algorithm algorithm, std::span<uint8 const> key, std::span<uint8 const> data, std::span<uint8 const> expected);

private:
    struct Impl;

    void RequireState() const;

    std::unique_ptr<Impl> _impl;
    CryptoHash::Algorithm _algorithm;
};

#endif

/*
 * Project Ambrose by Imjustchico
 * Generates the OFB keystream by re-encrypting the feedback block and XORs it over the data, carrying position between calls.
 */

#include "TwofishOfb.h"

#include <algorithm>
#include <stdexcept>

TwofishOfb::TwofishOfb(std::span<uint8 const> key, std::span<uint8 const, Twofish::BlockSize> iv) : _cipher(key)
{
    Restart(iv);
}

void TwofishOfb::Apply(std::span<uint8> data)
{
    Apply(data, data);
}

void TwofishOfb::Apply(std::span<uint8 const> input, std::span<uint8> output)
{
    if (output.size() < input.size())
        throw std::invalid_argument("OFB output buffer is smaller than the input");
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        if (_used == Twofish::BlockSize)
        {
            _feedback = _cipher.EncryptBlock(_feedback);
            _used = 0;
        }
        output[i] = static_cast<uint8>(input[i] ^ _feedback[_used++]);
    }
}

std::vector<uint8> TwofishOfb::Process(std::span<uint8 const> data)
{
    std::vector<uint8> output(data.size());
    Apply(data, output);
    return output;
}

void TwofishOfb::Restart(std::span<uint8 const, Twofish::BlockSize> iv)
{
    std::copy(iv.begin(), iv.end(), _feedback.begin());
    _used = Twofish::BlockSize;
}

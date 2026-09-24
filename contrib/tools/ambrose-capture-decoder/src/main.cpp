/*
 * Project Ambrose by Imjustchico
 * Reads private pcapng TCP captures and reports Ambrose frame metadata without exposing payloads.
 */

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using Bytes = std::vector<std::uint8_t>;

struct Options
{
    std::string capture;
    std::uint16_t port = 0;
    bool self_test = false;
};

struct StreamKey
{
    std::uint16_t source_port;
    std::uint16_t destination_port;
    bool operator<(const StreamKey& other) const
    {
        return std::tie(source_port, destination_port) < std::tie(other.source_port, other.destination_port);
    }
};

struct Stream
{
    std::uint64_t packets = 0;
    Bytes bytes;
    std::size_t offset = 0;
    std::uint64_t frame_number = 0;
};

struct Interface
{
    std::uint16_t link_type = 0;
};

std::uint16_t u16(const Bytes& data, std::size_t offset)
{
    if (offset + 2 > data.size())
        throw std::runtime_error("truncated u16");
    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

std::uint32_t u32(const Bytes& data, std::size_t offset)
{
    if (offset + 4 > data.size())
        throw std::runtime_error("truncated u32");
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

Bytes read_file(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("could not open capture");
    input.seekg(0, std::ios::end);
    auto size = input.tellg();
    if (size < 0 || size > 256 * 1024 * 1024)
        throw std::runtime_error("capture exceeds the 256 MiB safety bound");
    input.seekg(0);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

Bytes synthetic_frame()
{
    return {0x0D, 0xF0, 0x13, 0x00, 0x01, 0x03, 0x00, 0x00,
            0x34, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
}

void report_frames(Stream& stream, const std::string& direction, std::uint64_t packet_number)
{
    while (stream.offset + 4 <= stream.bytes.size())
    {
        if (stream.bytes[stream.offset] != 0x0D || stream.bytes[stream.offset + 1] != 0xF0)
        {
            ++stream.offset;
            continue;
        }
        std::size_t prefix = stream.offset;
        std::uint32_t declared = u16(stream.bytes, prefix + 2);
        std::size_t header_size = 4;
        if (declared == 0x8000)
        {
            if (prefix + 8 > stream.bytes.size())
                return;
            declared = u32(stream.bytes, prefix + 4);
            header_size = 8;
        }
        std::size_t total = header_size + declared;
        if (total < header_size + 1 || total > 4 * 1024 * 1024)
        {
            ++stream.offset;
            continue;
        }
        if (prefix + total > stream.bytes.size())
            return;
        std::size_t control_offset = prefix + header_size;
        std::size_t body_offset = control_offset + 4;
        ++stream.frame_number;
        if (stream.bytes[control_offset] == 1)
        {
            std::cout << "frame=" << stream.frame_number << " packet=" << packet_number
                      << " direction=" << direction << " kind=control"
                      << " opcode=" << static_cast<unsigned>(stream.bytes[control_offset + 1])
                      << " declared_length=" << declared
                      << " body_length=" << (total - header_size - 5) << '\n';
        }
        else if (stream.bytes[control_offset] == 0 && body_offset + 4 <= prefix + total)
        {
            std::uint16_t dml_length = u16(stream.bytes, body_offset + 2);
            std::cout << "frame=" << stream.frame_number << " packet=" << packet_number
                      << " direction=" << direction << " kind=dml"
                      << " service=" << static_cast<unsigned>(stream.bytes[body_offset])
                      << " order=" << static_cast<unsigned>(stream.bytes[body_offset + 1])
                      << " declared_length=" << declared
                      << " body_length=" << (total - header_size - 5)
                      << " first_dml_length=" << dml_length << '\n';
        }
        else
        {
            std::cout << "frame=" << stream.frame_number << " packet=" << packet_number
                      << " direction=" << direction << " kind=invalid-control"
                      << " declared_length=" << declared << '\n';
        }
        stream.offset = prefix + total;
    }
    if (stream.offset > 64 * 1024)
    {
        stream.bytes.erase(stream.bytes.begin(), stream.bytes.begin() + static_cast<std::ptrdiff_t>(stream.offset));
        stream.offset = 0;
    }
}

Bytes tcp_payload(const Bytes& packet, std::uint16_t link_type, std::uint16_t& source_port,
                  std::uint16_t& destination_port)
{
    std::size_t network_offset = 0;
    if (link_type == 1)
    {
        if (packet.size() < 14)
            return {};
        std::uint16_t ether_type = static_cast<std::uint16_t>((packet[12] << 8) | packet[13]);
        network_offset = 14;
        if (ether_type == 0x8100)
        {
            if (packet.size() < 18)
                return {};
            ether_type = static_cast<std::uint16_t>((packet[16] << 8) | packet[17]);
            network_offset = 18;
        }
        if (ether_type != 0x0800 && ether_type != 0x86DD)
            return {};
    }
    else if (link_type != 101)
        return {};
    if (network_offset >= packet.size())
        return {};
    std::uint8_t version = packet[network_offset] >> 4;
    std::size_t transport = network_offset;
    if (version == 4)
    {
        std::size_t header = static_cast<std::size_t>(packet[network_offset] & 0x0F) * 4;
        if (header < 20 || network_offset + header + 20 > packet.size() || packet[network_offset + 9] != 6)
            return {};
        transport += header;
    }
    else if (version == 6)
    {
        if (network_offset + 40 > packet.size() || packet[network_offset + 6] != 6)
            return {};
        transport += 40;
    }
    else
        return {};
    if (transport + 20 > packet.size())
        return {};
    source_port = static_cast<std::uint16_t>((packet[transport] << 8) | packet[transport + 1]);
    destination_port = static_cast<std::uint16_t>((packet[transport + 2] << 8) | packet[transport + 3]);
    std::size_t tcp_header = static_cast<std::size_t>(packet[transport + 12] >> 4) * 4;
    if (tcp_header < 20 || transport + tcp_header > packet.size())
        return {};
    return {packet.begin() + static_cast<std::ptrdiff_t>(transport + tcp_header), packet.end()};
}

void parse_capture(const Bytes& capture, std::uint16_t port)
{
    if (capture.size() < 12 || u32(capture, 0) != 0x0A0D0D0A || u32(capture, 8) != 0x4D3C2B1A)
        throw std::runtime_error("capture is not a little-endian pcapng file");
    std::vector<Interface> interfaces;
    std::map<StreamKey, Stream> streams;
    std::size_t offset = 0;
    std::uint64_t packet_number = 0;
    while (offset + 12 <= capture.size())
    {
        std::uint32_t type = u32(capture, offset);
        std::uint32_t length = u32(capture, offset + 4);
        if (length < 12 || offset + length > capture.size() || u32(capture, offset + length - 4) != length)
            throw std::runtime_error("truncated or malformed pcapng block");
        if (type == 1 && length >= 20)
            interfaces.push_back({u16(capture, offset + 8)});
        else if (type == 6 && length >= 32)
        {
            std::uint32_t interface_id = u32(capture, offset + 8);
            std::uint32_t captured_length = u32(capture, offset + 20);
            if (interface_id >= interfaces.size() || captured_length > length - 32)
                throw std::runtime_error("invalid enhanced packet block");
            Bytes packet(capture.begin() + static_cast<std::ptrdiff_t>(offset + 28),
                         capture.begin() + static_cast<std::ptrdiff_t>(offset + 28 + captured_length));
            std::uint16_t source_port = 0;
            std::uint16_t destination_port = 0;
            Bytes payload = tcp_payload(packet, interfaces[interface_id].link_type, source_port, destination_port);
            if (!payload.empty() && (source_port == port || destination_port == port))
            {
                ++packet_number;
                Stream& stream = streams[{source_port, destination_port}];
                stream.packets++;
                if (stream.bytes.size() + payload.size() > 4 * 1024 * 1024)
                    throw std::runtime_error("TCP stream exceeds the 4 MiB safety bound");
                stream.bytes.insert(stream.bytes.end(), payload.begin(), payload.end());
                report_frames(stream, source_port == port ? "server-to-client" : "client-to-server", packet_number);
            }
        }
        offset += length;
    }
    for (const auto& [key, stream] : streams)
    {
        if (stream.offset < stream.bytes.size())
            std::cout << "incomplete_stream=source_port:" << key.source_port
                      << ",destination_port:" << key.destination_port
                      << " buffered_bytes=" << (stream.bytes.size() - stream.offset) << '\n';
    }
    std::cout << "packets_with_tcp_payload=" << packet_number << " streams=" << streams.size() << '\n';
}

void self_test()
{
    Bytes frame = synthetic_frame();
    Stream stream{0, frame, 0, 0};
    report_frames(stream, "server-to-client", 1);
    if (stream.frame_number != 1)
        throw std::runtime_error("frame self-test failed");
    std::cout << "self-test: passed\n";
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--self-test")
        {
            self_test();
            return 0;
        }
        if (argc != 5 || std::string(argv[1]) != "--capture" || std::string(argv[3]) != "--port")
            throw std::runtime_error("usage: ambrose-capture-decoder --capture <file.pcapng> --port <tcp-port>");
        int port = std::stoi(argv[4]);
        if (port < 1 || port > 65535)
            throw std::runtime_error("port must be between 1 and 65535");
        parse_capture(read_file(argv[2]), static_cast<std::uint16_t>(port));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}

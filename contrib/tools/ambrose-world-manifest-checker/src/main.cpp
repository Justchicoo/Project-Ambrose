/*
 * Project Ambrose by Imjustchico
 * Compares canonical world manifests and reports disagreement metadata without exposing values.
 */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct Row
{
    std::string key;
    std::string table;
    std::string field;
    std::string value;
};

using Manifest = std::map<std::string, Row>;

std::vector<std::string> split_tabs(const std::string& line)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true)
    {
        std::size_t end = line.find('\t', start);
        fields.push_back(line.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos)
            return fields;
        start = end + 1;
    }
}

bool identifier(std::string_view value)
{
    if (value.empty())
        return false;
    for (char character : value)
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') || character == '_' ||
              character == '.' || character == ':' || character == '-'))
            return false;
    return true;
}

bool key_value(std::string_view value)
{
    if (value.empty())
        return false;
    for (char character : value)
        if (static_cast<unsigned char>(character) < 0x20 || character == '\t')
            return false;
    return true;
}

Manifest parse(std::istream& input, const std::string& label)
{
    Manifest result;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line))
    {
        ++line_number;
        if (line.size() > 1024 * 1024)
            throw std::runtime_error(label + ": line " + std::to_string(line_number) + " exceeds 1 MiB");
        if (line.empty())
            continue;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto fields = split_tabs(line);
        if (fields.size() != 4 || !identifier(fields[0]) || !key_value(fields[1]) ||
            !identifier(fields[2]) || fields[3].find('\r') != std::string::npos ||
            fields[3].find('\n') != std::string::npos)
            throw std::runtime_error(label + ": malformed line " + std::to_string(line_number));
        std::string key = fields[0] + "/" + fields[1] + "/" + fields[2];
        if (!result.emplace(key, Row{key, fields[0], fields[2], fields[3]}).second)
            throw std::runtime_error(label + ": duplicate key " + key);
        if (result.size() > 100000)
            throw std::runtime_error(label + ": more than 100000 rows");
    }
    if (input.bad())
        throw std::runtime_error(label + ": read failure");
    return result;
}

Manifest parse_text(const std::string& text, const std::string& label)
{
    std::istringstream input(text);
    return parse(input, label);
}

void print_report(const Manifest& world, const Manifest& client)
{
    std::size_t missing_client = 0;
    std::size_t missing_world = 0;
    std::size_t differing = 0;
    for (const auto& [key, row] : world)
    {
        auto found = client.find(key);
        if (found == client.end())
        {
            ++missing_client;
            std::cout << "missing_from_client key=" << key << '\n';
        }
        else if (found->second.value != row.value)
        {
            ++differing;
            std::cout << "different_value key=" << key << '\n';
        }
    }
    for (const auto& [key, row] : client)
        if (!world.contains(key))
        {
            ++missing_world;
            std::cout << "missing_from_world key=" << key << '\n';
        }
    std::cout << "world_rows=" << world.size() << " client_rows=" << client.size()
              << " missing_from_client=" << missing_client
              << " missing_from_world=" << missing_world
              << " different_values=" << differing << '\n';
    if (missing_client || missing_world || differing)
        std::cout << "result=different\n";
    else
        std::cout << "result=match\n";
}

void self_test()
{
    const std::string world = "zone_teleport\tWizardCity/WC_Hub/door\t" "dest_zone\tWizardCity/WC_Bazaar\n"
                              "zone_teleport\tWizardCity/WC_Hub/door\t" "transition_id\t0\n";
    const std::string client = "zone_teleport\tWizardCity/WC_Hub/door\t" "dest_zone\tWizardCity/WC_Bazaar\n"
                               "zone_teleport\tWizardCity/WC_Hub/door\t" "transition_id\t1\n"
                               "zone_teleport\textra/door\t" "dest_zone\tWizardCity/WC_School\n";
    Manifest left = parse_text(world, "world");
    Manifest right = parse_text(client, "client");
    if (left.size() != 2 || right.size() != 3 || left.at("zone_teleport/WizardCity/WC_Hub/door/dest_zone").value != "WizardCity/WC_Bazaar")
        throw std::runtime_error("manifest parsing self-test failed");
    bool duplicate_rejected = false;
    try
    {
        parse_text("a\tb\tc\tone\na\tb\tc\ttwo\n", "duplicate");
    }
    catch (const std::runtime_error&)
    {
        duplicate_rejected = true;
    }
    if (!duplicate_rejected)
        throw std::runtime_error("duplicate-key self-test failed");
    bool malformed_rejected = false;
    try
    {
        parse_text("a\tb\tc\n", "malformed");
    }
    catch (const std::runtime_error&)
    {
        malformed_rejected = true;
    }
    if (!malformed_rejected)
        throw std::runtime_error("malformed-line self-test failed");
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
        if (argc != 5 || std::string(argv[1]) != "--world" || std::string(argv[3]) != "--client")
            throw std::runtime_error("usage: ambrose-world-manifest-checker --world <world.tsv> --client <client.tsv>");
        std::ifstream world_file(argv[2], std::ios::binary);
        std::ifstream client_file(argv[4], std::ios::binary);
        if (!world_file || !client_file)
            throw std::runtime_error("could not open one or both manifests");
        Manifest world = parse(world_file, "world");
        Manifest client = parse(client_file, "client");
        print_report(world, client);
        for (const auto& [key, row] : world)
            if (!client.contains(key) || client.at(key).value != row.value)
                return 1;
        for (const auto& [key, row] : client)
            if (!world.contains(key))
                return 1;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}

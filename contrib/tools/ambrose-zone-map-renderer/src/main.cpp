/*
 * Project Ambrose by Imjustchico
 * Renders a bounded operator-owned zone manifest as an SVG map without client assets.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct Object
{
    std::string zone;
    std::string id;
    std::string kind;
    double x = 0;
    double y = 0;
    double yaw = 0;
    std::string layer;
};

bool identifier(std::string_view value)
{
    if (value.empty())
        return false;
    for (char character : value)
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') ||
              character == '_' || character == '/' || character == '-' || character == '.'))
            return false;
    return true;
}

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

double number(const std::string& value, const std::string& label)
{
    std::size_t consumed = 0;
    double parsed = 0;
    try
    {
        parsed = std::stod(value, &consumed);
    }
    catch (const std::exception&)
    {
        throw std::runtime_error(label + " is not a number");
    }
    if (consumed != value.size() || !std::isfinite(parsed))
        throw std::runtime_error(label + " is not finite");
    return parsed;
}

std::vector<Object> parse(std::istream& input)
{
    std::vector<Object> objects;
    std::map<std::pair<std::string, std::string>, bool> keys;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line))
    {
        ++line_number;
        if (line.size() > 1024 * 1024)
            throw std::runtime_error("line " + std::to_string(line_number) + " exceeds 1 MiB");
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        auto fields = split_tabs(line);
        if (fields.size() != 7 || !identifier(fields[0]) || !identifier(fields[1]) ||
            !identifier(fields[2]) || !identifier(fields[6]))
            throw std::runtime_error("malformed line " + std::to_string(line_number));
        auto key = std::make_pair(fields[0], fields[1]);
        if (!keys.emplace(key, true).second)
            throw std::runtime_error("duplicate object key at line " + std::to_string(line_number));
        objects.push_back({fields[0], fields[1], fields[2], number(fields[3], "x"),
                           number(fields[4], "y"), number(fields[5], "yaw"), fields[6]});
        if (objects.size() > 100000)
            throw std::runtime_error("manifest exceeds 100000 objects");
    }
    if (input.bad())
        throw std::runtime_error("manifest read failure");
    return objects;
}

std::string escape_xml(std::string_view value)
{
    std::string escaped;
    for (char character : value)
    {
        if (character == '&')
            escaped += "&amp;";
        else if (character == '<')
            escaped += "&lt;";
        else if (character == '>')
            escaped += "&gt;";
        else if (character == '"')
            escaped += "&quot;";
        else
            escaped += character;
    }
    return escaped;
}

std::string color(std::string_view kind)
{
    std::uint32_t hash = 2166136261u;
    for (unsigned char character : kind)
        hash = (hash ^ character) * 16777619u;
    std::ostringstream output;
    output << '#' << std::hex << std::setw(6) << std::setfill('0') << (hash & 0xFFFFFFu);
    return output.str();
}

std::string format(double value)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << value;
    return output.str();
}

void render(const std::vector<Object>& all, const std::string& zone, const std::string& output_path)
{
    std::vector<Object> objects;
    for (const auto& object : all)
        if (object.zone == zone)
            objects.push_back(object);
    if (objects.empty())
        throw std::runtime_error("zone has no objects: " + zone);

    double min_x = objects.front().x;
    double max_x = objects.front().x;
    double min_y = objects.front().y;
    double max_y = objects.front().y;
    for (const auto& object : objects)
    {
        min_x = std::min(min_x, object.x);
        max_x = std::max(max_x, object.x);
        min_y = std::min(min_y, object.y);
        max_y = std::max(max_y, object.y);
    }
    const double width = std::max(1.0, max_x - min_x);
    const double height = std::max(1.0, max_y - min_y);
    auto map_x = [&](double value) { return 80.0 + (value - min_x) / width * 1440.0; };
    auto map_y = [&](double value) { return 920.0 - (value - min_y) / height * 840.0; };

    std::ofstream output(output_path, std::ios::binary);
    if (!output)
        throw std::runtime_error("could not open SVG output");
    output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1600\" height=\"1000\" "
              "viewBox=\"0 0 1600 1000\" role=\"img\" aria-label=\"Ambrose zone map\">\n";
    output << "<title>" << escape_xml(zone) << "</title>\n"
              "<rect width=\"1600\" height=\"1000\" fill=\"#17151f\"/>\n"
              "<text x=\"40\" y=\"45\" fill=\"#f5f0e6\" font-family=\"sans-serif\" font-size=\"24\">"
           << escape_xml(zone) << " (" << objects.size() << " objects)</text>\n";
    for (const auto& object : objects)
    {
        double x = map_x(object.x);
        double y = map_y(object.y);
        double angle = object.yaw * std::numbers::pi / 180.0;
        double tip_x = x + std::cos(angle) * 14.0;
        double tip_y = y - std::sin(angle) * 14.0;
        output << "<g data-id=\"" << escape_xml(object.id) << "\" data-kind=\"" << escape_xml(object.kind)
               << "\" data-layer=\"" << escape_xml(object.layer) << "\">"
               << "<circle cx=\"" << format(x) << "\" cy=\"" << format(y) << "\" r=\"6\" fill=\""
               << color(object.kind) << "\"/>"
               << "<line x1=\"" << format(x) << "\" y1=\"" << format(y) << "\" x2=\"" << format(tip_x)
               << "\" y2=\"" << format(tip_y) << "\" stroke=\"#f5f0e6\" stroke-width=\"2\"/>"
               << "</g>\n";
    }
    output << "</svg>\n";
    if (!output)
        throw std::runtime_error("could not write SVG output");
    std::cout << "zone=" << zone << " objects=" << objects.size()
              << " bounds_x=" << format(min_x) << ":" << format(max_x)
              << " bounds_y=" << format(min_y) << ":" << format(max_y)
              << " output=" << output_path << '\n';
}

void self_test()
{
    std::istringstream input("Zone/A\tobject_1\tNPC\t0\t0\t0\tretail\n"
                             "Zone/A\tportal\tPortal\t10\t10\t90\tcustom\n");
    auto objects = parse(input);
    if (objects.size() != 2 || objects[0].id != "object_1")
        throw std::runtime_error("manifest self-test failed");
    if (escape_xml("obj<&\"") != "obj&lt;&amp;&quot;")
        throw std::runtime_error("XML escaping self-test failed");
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
        if (argc != 7 || std::string(argv[1]) != "--manifest" ||
            std::string(argv[3]) != "--zone" || std::string(argv[5]) != "--output")
            throw std::runtime_error("usage: ambrose-zone-map-renderer --manifest <objects.tsv> --zone <zone> --output <map.svg>");
        std::ifstream input(argv[2], std::ios::binary);
        if (!input)
            throw std::runtime_error("could not open manifest");
        auto objects = parse(input);
        render(objects, argv[4], argv[6]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}

/*
 * Project Ambrose by Imjustchico
 * Compares format-v2 type dumps and reports metadata, class, and property changes.
 */

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct JsonValue
{
    using Object = std::map<std::string, JsonValue>;
    using Array = std::vector<JsonValue>;
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Object, Array>;

    Storage Value;
};

class JsonParser
{
public:
    explicit JsonParser(std::string text) : _text(std::move(text))
    {
    }

    JsonValue Parse()
    {
        JsonValue value = ParseValue();
        SkipWhitespace();
        if (_position != _text.size())
            throw std::runtime_error("Unexpected data after JSON value");
        return value;
    }

private:
    void SkipWhitespace()
    {
        while (_position < _text.size() && std::isspace(static_cast<unsigned char>(_text[_position])))
            ++_position;
    }

    char Take()
    {
        if (_position >= _text.size())
            throw std::runtime_error("Unexpected end of JSON");
        return _text[_position++];
    }

    void Expect(char expected)
    {
        if (Take() != expected)
            throw std::runtime_error("Invalid JSON punctuation");
    }

    JsonValue ParseValue()
    {
        SkipWhitespace();
        if (_position >= _text.size())
            throw std::runtime_error("Missing JSON value");
        switch (_text[_position])
        {
            case '{':
                return ParseObject();
            case '[':
                return ParseArray();
            case '"':
                return JsonValue{ParseString()};
            case 't':
                return ParseLiteral("true", true);
            case 'f':
                return ParseLiteral("false", false);
            case 'n':
                return ParseLiteral("null", nullptr);
            default:
                return JsonValue{ParseNumber()};
        }
    }

    JsonValue ParseObject()
    {
        JsonValue::Object object;
        Expect('{');
        SkipWhitespace();
        if (_position < _text.size() && _text[_position] == '}')
        {
            ++_position;
            return JsonValue{std::move(object)};
        }
        while (true)
        {
            SkipWhitespace();
            std::string key = ParseString();
            SkipWhitespace();
            Expect(':');
            object.emplace(std::move(key), ParseValue());
            SkipWhitespace();
            char separator = Take();
            if (separator == '}')
                return JsonValue{std::move(object)};
            if (separator != ',')
                throw std::runtime_error("Invalid JSON object separator");
        }
    }

    JsonValue ParseArray()
    {
        JsonValue::Array array;
        Expect('[');
        SkipWhitespace();
        if (_position < _text.size() && _text[_position] == ']')
        {
            ++_position;
            return JsonValue{std::move(array)};
        }
        while (true)
        {
            array.push_back(ParseValue());
            SkipWhitespace();
            char separator = Take();
            if (separator == ']')
                return JsonValue{std::move(array)};
            if (separator != ',')
                throw std::runtime_error("Invalid JSON array separator");
        }
    }

    JsonValue ParseLiteral(std::string_view literal, JsonValue::Storage value)
    {
        if (_text.compare(_position, literal.size(), literal) != 0)
            throw std::runtime_error("Invalid JSON literal");
        _position += literal.size();
        return JsonValue{std::move(value)};
    }

    std::string ParseString()
    {
        Expect('"');
        std::string result;
        while (true)
        {
            char character = Take();
            if (character == '"')
                return result;
            if (character == '\\')
            {
                char escaped = Take();
                switch (escaped)
                {
                    case '"': result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/': result.push_back('/'); break;
                    case 'b': result.push_back('\b'); break;
                    case 'f': result.push_back('\f'); break;
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    default: throw std::runtime_error("Unsupported JSON escape");
                }
            }
            else
            {
                result.push_back(character);
            }
        }
    }

    double ParseNumber()
    {
        std::size_t end = _position;
        while (end < _text.size() && std::string_view("-+.eE0123456789").find(_text[end]) != std::string_view::npos)
            ++end;
        if (end == _position)
            throw std::runtime_error("Invalid JSON number");
        double value = std::stod(_text.substr(_position, end - _position));
        _position = end;
        return value;
    }

    std::string _text;
    std::size_t _position = 0;
};

static std::string ReadFile(char const* path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error(std::string("Could not open file: ") + path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

static JsonValue::Object const& Object(JsonValue const& value, std::string const& label)
{
    auto object = std::get_if<JsonValue::Object>(&value.Value);
    if (!object)
        throw std::runtime_error("Expected object for " + label);
    return *object;
}

static std::string String(JsonValue::Object const& object, std::string const& key)
{
    auto iterator = object.find(key);
    if (iterator == object.end())
        return {};
    auto value = std::get_if<std::string>(&iterator->second.Value);
    return value ? *value : std::string();
}

static std::string Number(JsonValue::Object const& object, std::string const& key)
{
    auto iterator = object.find(key);
    if (iterator == object.end())
        return {};
    auto value = std::get_if<double>(&iterator->second.Value);
    return value ? std::to_string(static_cast<std::uint64_t>(*value)) : std::string();
}

static JsonValue::Object const& Classes(JsonValue::Object const& root)
{
    auto iterator = root.find("classes");
    if (iterator == root.end())
        throw std::runtime_error("Type dump has no classes object");
    return Object(iterator->second, "classes");
}

static bool Equal(JsonValue const& left, JsonValue const& right)
{
    if (left.Value.index() != right.Value.index())
        return false;
    if (std::holds_alternative<std::nullptr_t>(left.Value))
        return true;
    if (auto value = std::get_if<bool>(&left.Value))
        return *value == std::get<bool>(right.Value);
    if (auto value = std::get_if<double>(&left.Value))
        return *value == std::get<double>(right.Value);
    if (auto value = std::get_if<std::string>(&left.Value))
        return *value == std::get<std::string>(right.Value);
    if (auto value = std::get_if<JsonValue::Object>(&left.Value))
    {
        auto const& other = std::get<JsonValue::Object>(right.Value);
        if (value->size() != other.size())
            return false;
        for (auto const& [key, item] : *value)
        {
            auto iterator = other.find(key);
            if (iterator == other.end() || !Equal(item, iterator->second))
                return false;
        }
        return true;
    }
    auto const& array = std::get<JsonValue::Array>(left.Value);
    auto const& other = std::get<JsonValue::Array>(right.Value);
    if (array.size() != other.size())
        return false;
    for (std::size_t index = 0; index < array.size(); ++index)
        if (!Equal(array[index], other[index]))
            return false;
    return true;
}

constexpr int SupportedVersion = 2;

static std::string Describe(JsonValue const& value)
{
    if (auto const* text = std::get_if<std::string>(&value.Value))
        return *text;
    if (auto const* number = std::get_if<double>(&value.Value))
    {
        std::ostringstream out;
        out << *number;
        return out.str();
    }
    if (auto const* flag = std::get_if<bool>(&value.Value))
        return *flag ? "true" : "false";
    if (std::get_if<std::nullptr_t>(&value.Value))
        return "null";
    return "a value of its own";
}

static void RequireSupportedVersion(JsonValue::Object const& root, std::string const& path)
{
    auto const version = root.find("version");
    auto const* number = version == root.end() ? nullptr : std::get_if<double>(&version->second.Value);
    if (!number)
        throw std::runtime_error(path + ": names no version, so it is not a type dump this tool can read");
    if (static_cast<int>(*number) != SupportedVersion)
        throw std::runtime_error(path + ": is version " + Describe(version->second) + ", and this tool reads version " + std::to_string(SupportedVersion));
}

static void ReportMetadataChange(char const* name, std::string const& oldValue, std::string const& newValue)
{
    if (oldValue != newValue)
        std::cout << name << ' ' << oldValue << " -> " << newValue << '\n';
}

static void ReportPropertyChange(std::string const& key, std::string const& name, JsonValue const& oldProperty, JsonValue const& newProperty)
{
    auto const* oldFields = std::get_if<JsonValue::Object>(&oldProperty.Value);
    auto const* newFields = std::get_if<JsonValue::Object>(&newProperty.Value);
    if (!oldFields || !newFields)
    {
        std::cout << "property changed " << key << '.' << name << '\n';
        return;
    }
    for (auto const& [field, value] : *newFields)
    {
        auto const previous = oldFields->find(field);
        if (previous == oldFields->end())
            std::cout << "property changed " << key << '.' << name << ' ' << field << " added " << Describe(value) << '\n';
        else if (!Equal(previous->second, value))
            std::cout << "property changed " << key << '.' << name << ' ' << field << ' ' << Describe(previous->second) << " -> " << Describe(value) << '\n';
    }
    for (auto const& [field, value] : *oldFields)
        if (!newFields->contains(field))
            std::cout << "property changed " << key << '.' << name << ' ' << field << " removed " << Describe(value) << '\n';
}

static void CompareClasses(JsonValue::Object const& oldRoot, JsonValue::Object const& newRoot)
{
    JsonValue::Object const& oldClasses = Classes(oldRoot);
    JsonValue::Object const& newClasses = Classes(newRoot);
    for (auto const& [key, value] : newClasses)
    {
        auto old = oldClasses.find(key);
        if (old == oldClasses.end())
        {
            std::cout << "class added " << key << '\n';
            continue;
        }
        JsonValue::Object const& oldClass = Object(old->second, "class");
        JsonValue::Object const& newClass = Object(value, "class");
        if (Number(oldClass, "hash") != Number(newClass, "hash"))
            std::cout << "class changed " << key << " hash " << Number(oldClass, "hash") << " -> " << Number(newClass, "hash") << '\n';

        JsonValue::Object const& oldProperties = Object(oldClass.at("properties"), "properties");
        JsonValue::Object const& newProperties = Object(newClass.at("properties"), "properties");
        for (auto const& [name, property] : newProperties)
        {
            auto oldProperty = oldProperties.find(name);
            if (oldProperty == oldProperties.end())
            {
                std::cout << "property added " << key << '.' << name << '\n';
                continue;
            }
            if (!Equal(oldProperty->second, property))
                ReportPropertyChange(key, name, oldProperty->second, property);
        }
        for (auto const& [name, property] : oldProperties)
            if (!newProperties.contains(name))
                std::cout << "property removed " << key << '.' << name << '\n';
    }
    for (auto const& [key, value] : oldClasses)
        if (!newClasses.contains(key))
            std::cout << "class removed " << key << '\n';
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: ambrose-type-diff <old-type-dump> <new-type-dump>\n";
        return 2;
    }
    try
    {
        JsonValue oldDocument = JsonParser(ReadFile(argv[1])).Parse();
        JsonValue newDocument = JsonParser(ReadFile(argv[2])).Parse();
        JsonValue::Object const& oldRoot = Object(oldDocument, "root");
        JsonValue::Object const& newRoot = Object(newDocument, "root");
        RequireSupportedVersion(oldRoot, argv[1]);
        RequireSupportedVersion(newRoot, argv[2]);
        ReportMetadataChange("revision", String(oldRoot, "revision"), String(newRoot, "revision"));
        ReportMetadataChange("executable_sha256", String(oldRoot, "executable_sha256"), String(newRoot, "executable_sha256"));
        CompareClasses(oldRoot, newRoot);
    }
    catch (std::exception const& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

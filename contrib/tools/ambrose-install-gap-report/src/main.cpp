/*
 * Project Ambrose by Imjustchico
 * Reports candidate type-dump and source-reference gaps without retaining client data.
 */

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
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
    Storage value;
};

class JsonParser
{
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    JsonValue parse()
    {
        JsonValue result = parse_value();
        skip();
        if (position_ != text_.size())
            throw std::runtime_error("unexpected data after JSON value");
        return result;
    }

private:
    void skip()
    {
        while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_])))
            ++position_;
    }

    char take()
    {
        if (position_ >= text_.size())
            throw std::runtime_error("unexpected end of JSON");
        return text_[position_++];
    }

    void expect(char expected)
    {
        if (take() != expected)
            throw std::runtime_error("invalid JSON punctuation");
    }

    JsonValue parse_value()
    {
        skip();
        if (position_ >= text_.size())
            throw std::runtime_error("missing JSON value");
        switch (text_[position_])
        {
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        case '"':
            return JsonValue{parse_string()};
        case 't':
            return parse_literal("true", true);
        case 'f':
            return parse_literal("false", false);
        case 'n':
            return parse_literal("null", nullptr);
        default:
            return JsonValue{parse_number()};
        }
    }

    JsonValue parse_object()
    {
        JsonValue::Object result;
        expect('{');
        skip();
        if (position_ < text_.size() && text_[position_] == '}')
        {
            ++position_;
            return JsonValue{std::move(result)};
        }
        while (true)
        {
            skip();
            std::string key = parse_string();
            skip();
            expect(':');
            if (!result.emplace(std::move(key), parse_value()).second)
                throw std::runtime_error("duplicate JSON object key");
            skip();
            char separator = take();
            if (separator == '}')
                return JsonValue{std::move(result)};
            if (separator != ',')
                throw std::runtime_error("invalid JSON object separator");
        }
    }

    JsonValue parse_array()
    {
        JsonValue::Array result;
        expect('[');
        skip();
        if (position_ < text_.size() && text_[position_] == ']')
        {
            ++position_;
            return JsonValue{std::move(result)};
        }
        while (true)
        {
            result.push_back(parse_value());
            skip();
            char separator = take();
            if (separator == ']')
                return JsonValue{std::move(result)};
            if (separator != ',')
                throw std::runtime_error("invalid JSON array separator");
        }
    }

    std::string parse_string()
    {
        expect('"');
        std::string result;
        while (true)
        {
            char character = take();
            if (character == '"')
                return result;
            if (character == '\\')
            {
                char escaped = take();
                if (escaped == '"' || escaped == '\\' || escaped == '/')
                    result.push_back(escaped);
                else if (escaped == 'b')
                    result.push_back('\b');
                else if (escaped == 'f')
                    result.push_back('\f');
                else if (escaped == 'n')
                    result.push_back('\n');
                else if (escaped == 'r')
                    result.push_back('\r');
                else if (escaped == 't')
                    result.push_back('\t');
                else
                    throw std::runtime_error("unsupported JSON escape");
            }
            else
            {
                if (static_cast<unsigned char>(character) < 0x20)
                    throw std::runtime_error("control character in JSON string");
                result.push_back(character);
            }
        }
    }

    JsonValue parse_literal(std::string_view literal, JsonValue::Storage value)
    {
        if (text_.compare(position_, literal.size(), literal) != 0)
            throw std::runtime_error("invalid JSON literal");
        position_ += literal.size();
        return JsonValue{std::move(value)};
    }

    double parse_number()
    {
        std::size_t consumed = 0;
        try
        {
            double value = std::stod(text_.substr(position_), &consumed);
            if (consumed == 0)
                throw std::runtime_error("invalid JSON number");
            position_ += consumed;
            return value;
        }
        catch (const std::invalid_argument&)
        {
            throw std::runtime_error("invalid JSON value");
        }
        catch (const std::out_of_range&)
        {
            throw std::runtime_error("JSON number out of range");
        }
    }

    std::string text_;
    std::size_t position_ = 0;
};

using Object = JsonValue::Object;

const JsonValue* member(const Object& object, const std::string& key)
{
    auto found = object.find(key);
    return found == object.end() ? nullptr : &found->second;
}

const Object* object_value(const JsonValue* value)
{
    return value == nullptr ? nullptr : std::get_if<Object>(&value->value);
}

bool is_version_two(const JsonValue* value)
{
    const double* number = value == nullptr ? nullptr : std::get_if<double>(&value->value);
    return number != nullptr && *number == 2;
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("could not read " + path.string());
    input.seekg(0, std::ios::end);
    auto size = input.tellg();
    if (size < 0 || size > 4 * 1024 * 1024)
        return {};
    input.seekg(0);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool source_extension(const std::filesystem::path& path)
{
    static const std::vector<std::string> extensions = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".ipp", ".inl"};
    std::string extension = path.extension().string();
    for (const auto& allowed : extensions)
        if (extension == allowed)
            return true;
    return false;
}

int count_identifier(const std::string& text, const std::string& identifier)
{
    const std::regex pattern("(^|[^A-Za-z0-9_])" + identifier + "([^A-Za-z0-9_]|$)");
    return static_cast<int>(std::distance(std::sregex_iterator(text.begin(), text.end(), pattern), std::sregex_iterator()));
}

void self_test()
{
    JsonValue root = JsonParser(R"({"version":2,"classes":{"Known":{"name":"Known"}}})").parse();
    const Object* object = object_value(&root);
    if (object == nullptr || !is_version_two(member(*object, "version")) ||
        object_value(member(*object, "classes")) == nullptr)
        throw std::runtime_error("parser self-test failed");
    if (count_identifier("Known Knownish _Known Known", "Known") != 2)
        throw std::runtime_error("identifier self-test failed");
    std::cout << "self-test: passed\n";
}

int run(const std::filesystem::path& dump, const std::filesystem::path& source_root)
{
    JsonValue document = JsonParser(read_file(dump)).parse();
    const Object* root = object_value(&document);
    if (root == nullptr || !is_version_two(member(*root, "version")))
        throw std::runtime_error("dump must be a version-2 JSON object");
    const Object* classes = object_value(member(*root, "classes"));
    if (classes == nullptr)
        throw std::runtime_error("dump has no classes object");
    if (!std::filesystem::is_directory(source_root))
        throw std::runtime_error("source root is not a directory");

    std::map<std::string, int> references;
    for (const auto& [name, ignored] : *classes)
        references.emplace(name, 0);
    int scanned_files = 0;
    int candidate_files = 0;
    std::error_code walk_error;
    for (std::filesystem::recursive_directory_iterator iterator(source_root, walk_error), end;
         iterator != end && scanned_files < 10000; iterator.increment(walk_error))
    {
        if (walk_error)
            break;
        if (!iterator->is_regular_file() || !source_extension(iterator->path()))
            continue;
        ++scanned_files;
        std::string content = read_file(iterator->path());
        bool has_reference = false;
        for (auto& [name, count] : references)
        {
            int found = count_identifier(content, name);
            count += found;
            has_reference = has_reference || found > 0;
        }
        if (!has_reference)
        {
            ++candidate_files;
            std::cout << "candidate_file=" << std::filesystem::relative(iterator->path(), source_root).string() << '\n';
        }
    }
    const std::string revision = [&]()
    {
        const JsonValue* value = member(*root, "revision");
        const auto* text = value == nullptr ? nullptr : std::get_if<std::string>(&value->value);
        return text == nullptr ? std::string("unknown") : *text;
    }();
    std::cout << "revision=" << revision << " classes=" << references.size()
              << " source_files=" << scanned_files << " candidate_files=" << candidate_files << '\n';
    for (const auto& [name, count] : references)
        if (count == 0)
            std::cout << "unreferenced_class=" << name << '\n';
        else
            std::cout << "class=" << name << " references=" << count << '\n';
    return 0;
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
        if (argc != 5 || std::string(argv[1]) != "--dump" || std::string(argv[3]) != "--source-root")
            throw std::runtime_error("usage: ambrose-install-gap-report --dump <type-dump.json> --source-root <source>");
        return run(argv[2], argv[4]);
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}

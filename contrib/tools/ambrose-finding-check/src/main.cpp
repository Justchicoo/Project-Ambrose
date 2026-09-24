/*
 * Project Ambrose by Imjustchico
 * Validates machine-checkable finding blocks without executing their commands or reading private inputs.
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
    explicit JsonParser(std::string text) : text_(std::move(text))
    {
    }

    JsonValue parse()
    {
        JsonValue result = parse_value();
        skip_whitespace();
        if (position_ != text_.size())
            throw std::runtime_error("unexpected data after JSON value");
        return result;
    }

private:
    void skip_whitespace()
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
        skip_whitespace();
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
        skip_whitespace();
        if (position_ < text_.size() && text_[position_] == '}')
        {
            ++position_;
            return JsonValue{std::move(result)};
        }
        while (true)
        {
            skip_whitespace();
            std::string key = parse_string();
            skip_whitespace();
            expect(':');
            auto [_, inserted] = result.emplace(std::move(key), parse_value());
            if (!inserted)
                throw std::runtime_error("duplicate JSON object key");
            skip_whitespace();
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
        skip_whitespace();
        if (position_ < text_.size() && text_[position_] == ']')
        {
            ++position_;
            return JsonValue{std::move(result)};
        }
        while (true)
        {
            result.push_back(parse_value());
            skip_whitespace();
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
                switch (escaped)
                {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escaped);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                default:
                    throw std::runtime_error("unsupported JSON string escape");
                }
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
        double value = 0;
        try
        {
            value = std::stod(text_.substr(position_), &consumed);
        }
        catch (const std::exception&)
        {
            throw std::runtime_error("invalid JSON number");
        }
        if (consumed == 0)
            throw std::runtime_error("invalid JSON value");
        position_ += consumed;
        return value;
    }

    std::string text_;
    std::size_t position_ = 0;
};

using Object = JsonValue::Object;
using Array = JsonValue::Array;

const JsonValue* member(const Object& object, const std::string& name)
{
    auto found = object.find(name);
    return found == object.end() ? nullptr : &found->second;
}

const Object* object_value(const JsonValue* value)
{
    return value == nullptr ? nullptr : std::get_if<Object>(&value->value);
}

const Array* array_value(const JsonValue* value)
{
    return value == nullptr ? nullptr : std::get_if<Array>(&value->value);
}

const std::string* string_value(const JsonValue* value)
{
    return value == nullptr ? nullptr : std::get_if<std::string>(&value->value);
}

bool is_integer(const JsonValue* value, int expected)
{
    const double* number = value == nullptr ? nullptr : std::get_if<double>(&value->value);
    return number != nullptr && *number == expected;
}

void add_problem(std::vector<std::string>& problems, const std::string& field, const std::string& detail)
{
    problems.push_back(field + ": " + detail);
}

bool safe_entrypoint(const std::string& entrypoint)
{
    if (entrypoint.empty() || std::filesystem::path(entrypoint).is_absolute())
        return false;
    std::filesystem::path path(entrypoint);
    for (const auto& part : path)
        if (part == ".." || part == ".")
            return false;
    return true;
}

bool entrypoint_is_below(const std::filesystem::path& finding, const std::string& entrypoint)
{
    std::error_code error;
    const std::filesystem::path base = std::filesystem::weakly_canonical(finding.parent_path(), error);
    if (error)
        return false;
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(finding.parent_path() / entrypoint, error);
    if (error)
        return false;
    const std::filesystem::path relative = std::filesystem::relative(resolved, base, error);
    if (error || relative.empty() || relative == ".")
        return false;
    return relative.begin()->string() != "..";
}

void validate_result_contract(const Object& block, const char* name, const char* result,
                             std::vector<std::string>& problems)
{
    const JsonValue* value = member(block, name);
    const Object* contract = object_value(value);
    if (contract == nullptr)
    {
        add_problem(problems, std::string("machine_check.") + name, "must be an object");
        return;
    }
    if (!is_integer(member(*contract, "exit_code"), name == std::string("success") ? 0 :
                    name == std::string("failure") ? 1 : 77))
        add_problem(problems, std::string("machine_check.") + name + ".exit_code", "has the wrong exit code");
    const std::string* actual = string_value(member(*contract, "result"));
    if (actual == nullptr || *actual != result)
        add_problem(problems, std::string("machine_check.") + name + ".result", "has the wrong result");
}

std::vector<std::string> validate_finding(const std::filesystem::path& finding)
{
    std::vector<std::string> problems;
    std::ifstream input(finding);
    if (!input)
    {
        add_problem(problems, finding.string(), "cannot read file");
        return problems;
    }
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    JsonValue root;
    try
    {
        root = JsonParser(std::move(text)).parse();
    }
    catch (const std::exception& error)
    {
        add_problem(problems, finding.string(), error.what());
        return problems;
    }
    const Object* document = object_value(&root);
    if (document == nullptr)
    {
        add_problem(problems, finding.string(), "finding must be a JSON object");
        return problems;
    }
    const JsonValue* machine = member(*document, "machine_check");
    if (machine == nullptr)
        return problems;
    const Object* block = object_value(machine);
    if (block == nullptr)
    {
        add_problem(problems, "machine_check", "must be an object");
        return problems;
    }
    if (!is_integer(member(*block, "schema"), 1))
        add_problem(problems, "machine_check.schema", "must be integer 1");
    const std::string* kind = string_value(member(*block, "kind"));
    if (kind == nullptr || *kind != "command")
        add_problem(problems, "machine_check.kind", "must be command");
    const std::string* entrypoint = string_value(member(*block, "entrypoint"));
    if (entrypoint == nullptr || !safe_entrypoint(entrypoint == nullptr ? "" : *entrypoint))
        add_problem(problems, "machine_check.entrypoint", "must be a relative path without . or .. components");
    else
    {
        std::error_code error;
        std::filesystem::path resolved = finding.parent_path() / *entrypoint;
        if (!entrypoint_is_below(finding, *entrypoint) || !std::filesystem::is_regular_file(resolved, error))
            add_problem(problems, "machine_check.entrypoint", "does not name a regular file below the finding");
    }

    const Array* arguments = array_value(member(*block, "arguments"));
    if (arguments == nullptr)
        add_problem(problems, "machine_check.arguments", "must be an array");
    const Array* inputs = array_value(member(*block, "inputs"));
    if (inputs == nullptr)
        add_problem(problems, "machine_check.inputs", "must be an array");

    std::map<std::string, int> input_names;
    if (inputs != nullptr)
    {
        for (std::size_t index = 0; index < inputs->size(); ++index)
        {
            const Object* input = object_value(&(*inputs)[index]);
            if (input == nullptr)
            {
                add_problem(problems, "machine_check.inputs[" + std::to_string(index) + "]", "must be an object");
                continue;
            }
            const std::string* name = string_value(member(*input, "name"));
            const std::string* input_kind = string_value(member(*input, "kind"));
            if (name == nullptr || name->empty())
                add_problem(problems, "machine_check.inputs[" + std::to_string(index) + "].name", "must be non-empty");
            else if (++input_names[*name] != 1)
                add_problem(problems, "machine_check.inputs." + *name, "is declared more than once");
            if (input_kind == nullptr || (*input_kind != "client_install" && *input_kind != "owned_capture"))
                add_problem(problems, "machine_check.inputs[" + std::to_string(index) + "].kind",
                            "must be client_install or owned_capture");
            if (member(*input, "required") == nullptr || std::get_if<bool>(&member(*input, "required")->value) == nullptr)
                add_problem(problems, "machine_check.inputs[" + std::to_string(index) + "].required",
                            "must be a boolean");
        }
    }

    static const std::regex placeholder(R"(\$\{([A-Za-z_][A-Za-z0-9_]*)\})");
    if (arguments != nullptr)
    {
        for (std::size_t index = 0; index < arguments->size(); ++index)
        {
            const std::string* argument = string_value(&(*arguments)[index]);
            if (argument == nullptr)
            {
                add_problem(problems, "machine_check.arguments[" + std::to_string(index) + "]", "must be a string");
                continue;
            }
            for (std::sregex_iterator match(argument->begin(), argument->end(), placeholder), end; match != end; ++match)
            {
                std::string name = (*match)[1].str();
                if (input_names[name] != 1)
                    add_problem(problems, "machine_check.arguments[" + std::to_string(index) + "]",
                                "uses an undeclared input " + name);
            }
        }
    }
    validate_result_contract(*block, "success", "pass", problems);
    validate_result_contract(*block, "failure", "fail", problems);
    validate_result_contract(*block, "unavailable", "unable", problems);
    return problems;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: ambrose-finding-check <finding.json> [...]\n";
        return 2;
    }
    int invalid = 0;
    for (int index = 1; index < argc; ++index)
    {
        auto problems = validate_finding(argv[index]);
        if (problems.empty())
        {
            std::cout << "valid: " << argv[index] << '\n';
            continue;
        }
        ++invalid;
        for (const auto& problem : problems)
            std::cout << "invalid: " << argv[index] << ": " << problem << '\n';
    }
    return invalid == 0 ? 0 : 1;
}

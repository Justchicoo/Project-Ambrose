/*
 * Project Ambrose by Imjustchico
 * Runtime registry of the client's messages: loads definitions, finds them by service and order or tag, and encodes declared C++ messages by their loaded layout.
 */

#ifndef AMBROSE_MESSAGEREGISTRY_H
#define AMBROSE_MESSAGEREGISTRY_H

#include "MessageDeclaration.h"
#include "MessageDefinitionSet.h"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct MessageInfo
{
    ProtocolDef const* Protocol = nullptr;
    MessageDef const* Definition = nullptr;
    std::vector<DmlValue> Defaults;
};

enum class MessageDecodeStatus : uint8
{
    Ok,
    Truncated,
    TrailingBytes
};

class MessageRegistry
{
public:
    static constexpr char const* LogFilter = "server.loading";

    MessageRegistry();
    ~MessageRegistry();
    MessageRegistry(MessageRegistry const&) = delete;
    MessageRegistry& operator=(MessageRegistry const&) = delete;

    static MessageRegistry& Instance();

    bool Load(MessageDefinitionSet definitions);
    bool LoadFromArchive(std::filesystem::path const& archivePath);
    bool LoadFromClient(std::filesystem::path const& clientDirectory);
    void Clear();

    bool IsLoaded() const noexcept { return _definitions != nullptr; }
    MessageDefinitionSet const* GetDefinitions() const noexcept { return _definitions.get(); }
    std::vector<MessageIssue> const& GetErrors() const noexcept { return _errors; }
    std::vector<MessageIssue> const& GetWarnings() const noexcept { return _warnings; }
    std::size_t GetMessageCount() const noexcept { return _infos.size(); }

    MessageInfo const* Find(uint8 serviceId, uint8 order) const noexcept;
    MessageInfo const* Find(uint8 serviceId, std::string_view tag) const noexcept;

    template<DeclaredMessage... Messages>
    bool Declare(std::vector<std::string>& errors);

    template<DeclaredMessage T>
    bool IsDeclared() const noexcept;

    template<DeclaredMessage T>
    MessageInfo const& GetInfo() const;

    template<DeclaredMessage T>
    void Encode(T const& message, ByteBuffer& buffer) const;

    template<DeclaredMessage T>
    bool Decode(ByteBuffer& buffer, T& message) const;

    template<DeclaredMessage T>
    MessageDecodeStatus Decode(std::span<uint8 const> body, T& message) const;

    static std::filesystem::path GetClientArchivePath(std::filesystem::path const& clientDirectory);

private:
    struct Binding
    {
        MessageInfo const* Info = nullptr;
        std::vector<int32> MemberForField;
    };

    template<DeclaredMessage T>
    bool DeclareOne(std::vector<std::string>& errors);

    template<DeclaredMessage T>
    Binding const& GetBinding() const;

    void ReportLoadIssues() const;
    void ReportDeclarationError(std::string const& error) const;
    static void SkipField(ByteBuffer& buffer, DmlType type);

    std::unique_ptr<MessageDefinitionSet> _definitions;
    std::vector<MessageInfo> _infos;
    std::vector<uint16> _index;
    std::vector<Binding> _bindings;
    std::vector<MessageIssue> _errors;
    std::vector<MessageIssue> _warnings;
};

#define sMessageRegistry MessageRegistry::Instance()

template<DeclaredMessage... Messages>
bool MessageRegistry::Declare(std::vector<std::string>& errors)
{
    bool succeeded = true;
    ((succeeded = DeclareOne<Messages>(errors) && succeeded), ...);
    return succeeded;
}

template<DeclaredMessage T>
bool MessageRegistry::DeclareOne(std::vector<std::string>& errors)
{
    using Fields = std::remove_cvref_t<decltype(T::Fields())>;
    static_assert(IsMessageFieldTuple<T, Fields>::value, "Fields() must return a std::tuple of Field() entries whose members belong to the message or its bases and are non-const DML-compatible types");

    uint8 const serviceId = static_cast<uint8>(T::ServiceId);
    std::string_view const tag = T::Tag;
    std::size_t const slot = MessageDeclarationDetail::TypeIndex<T>();
    if (_bindings.size() <= slot)
        _bindings.resize(slot + 1);
    _bindings[slot] = Binding();

    auto fail = [&](std::string error)
    {
        ReportDeclarationError(error);
        errors.push_back(std::move(error));
        return false;
    };

    if (!IsLoaded())
        return fail(fmt::format("{} (service {}) cannot be declared before message definitions are loaded", tag, serviceId));
    MessageInfo const* const info = Find(serviceId, tag);
    if (!info)
        return fail(fmt::format("{} is not a message of service {}", tag, serviceId));

    MessageDef const& definition = *info->Definition;
    Binding binding;
    binding.Info = info;
    binding.MemberForField.assign(definition.Fields.size(), -1);
    bool valid = true;
    Fields const fields = T::Fields();
    MessageDeclarationDetail::ForEachField(fields, [&](std::size_t member, auto const& field)
    {
        using Member = std::remove_cvref_t<decltype(std::declval<T&>().*(field.Pointer))>;
        auto const found = std::find_if(definition.Fields.begin(), definition.Fields.end(), [&](FieldDef const& candidate) { return candidate.Name == field.Name; });
        if (found == definition.Fields.end())
        {
            valid = fail(fmt::format("{} (service {}) has no field {}", tag, serviceId, field.Name)) && valid;
            return;
        }
        std::size_t const index = static_cast<std::size_t>(found - definition.Fields.begin());
        if (!MessageMember::IsCompatible<Member>(found->Type))
        {
            valid = fail(fmt::format("{}.{} is {} in the client definition, which the declared C++ member type cannot hold exactly", tag, field.Name, Dml::GetTypeName(found->Type))) && valid;
            return;
        }
        if (binding.MemberForField[index] >= 0)
        {
            valid = fail(fmt::format("{}.{} is declared by more than one member", tag, field.Name)) && valid;
            return;
        }
        binding.MemberForField[index] = static_cast<int32>(member);
    });
    if (!valid)
        return false;
    _bindings[slot] = std::move(binding);
    return true;
}

template<DeclaredMessage T>
bool MessageRegistry::IsDeclared() const noexcept
{
    std::size_t const slot = MessageDeclarationDetail::TypeIndex<T>();
    return slot < _bindings.size() && _bindings[slot].Info != nullptr;
}

template<DeclaredMessage T>
MessageRegistry::Binding const& MessageRegistry::GetBinding() const
{
    std::size_t const slot = MessageDeclarationDetail::TypeIndex<T>();
    if (slot >= _bindings.size() || !_bindings[slot].Info)
        throw std::logic_error(fmt::format("{} (service {}) was used before MessageRegistry::Declare resolved it", T::Tag, static_cast<uint32>(T::ServiceId)));
    return _bindings[slot];
}

template<DeclaredMessage T>
MessageInfo const& MessageRegistry::GetInfo() const
{
    return *GetBinding<T>().Info;
}

template<DeclaredMessage T>
void MessageRegistry::Encode(T const& message, ByteBuffer& buffer) const
{
    Binding const& binding = GetBinding<T>();
    MessageDef const& definition = *binding.Info->Definition;
    auto const fields = T::Fields();
    MessageDeclarationDetail::ForEachField(fields, [&](std::size_t, auto const& field) { MessageMember::CheckEncodable(message.*(field.Pointer)); });
    for (std::size_t i = 0; i < definition.Fields.size(); ++i)
    {
        int32 const member = binding.MemberForField[i];
        if (member < 0)
        {
            Dml::WriteValue(buffer, definition.Fields[i].Type, binding.Info->Defaults[i]);
            continue;
        }
        MessageDeclarationDetail::VisitField(fields, static_cast<std::size_t>(member), [&](auto const& field) { MessageMember::Write(buffer, message.*(field.Pointer)); });
    }
}

template<DeclaredMessage T>
bool MessageRegistry::Decode(ByteBuffer& buffer, T& message) const
{
    Binding const& binding = GetBinding<T>();
    MessageDef const& definition = *binding.Info->Definition;
    auto const fields = T::Fields();
    std::size_t const start = buffer.GetReadPosition();
    T decoded{};
    try
    {
        for (std::size_t i = 0; i < definition.Fields.size(); ++i)
        {
            int32 const member = binding.MemberForField[i];
            if (member < 0)
            {
                SkipField(buffer, definition.Fields[i].Type);
                continue;
            }
            MessageDeclarationDetail::VisitField(fields, static_cast<std::size_t>(member), [&](auto const& field) { MessageMember::Read(buffer, decoded.*(field.Pointer)); });
        }
    }
    catch (ByteBufferException const&)
    {
        buffer.SetReadPosition(start);
        return false;
    }
    message = std::move(decoded);
    return true;
}

template<DeclaredMessage T>
MessageDecodeStatus MessageRegistry::Decode(std::span<uint8 const> body, T& message) const
{
    ByteBuffer buffer(body);
    if (!Decode(buffer, message))
        return MessageDecodeStatus::Truncated;
    return buffer.GetRemaining() == 0 ? MessageDecodeStatus::Ok : MessageDecodeStatus::TrailingBytes;
}

#endif

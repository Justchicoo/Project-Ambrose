/*
 * Project Ambrose by Imjustchico
 * Live message registry: immutable catalog snapshots of the client's messages that reload atomically, with lookups and encoding of declared C++ messages.
 */

#ifndef AMBROSE_MESSAGEREGISTRY_H
#define AMBROSE_MESSAGEREGISTRY_H

#include "MessageDeclaration.h"
#include "MessageDefinitionSet.h"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <mutex>
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
    std::size_t MinSize = 0;
};

enum class MessageDecodeStatus : uint8
{
    Ok,
    Truncated,
    TrailingBytes
};

struct MessageBinding
{
    MessageInfo const* Info = nullptr;
    std::vector<int32> MemberForField;
};

class MessageCatalog
{
public:
    uint64 GetGeneration() const noexcept { return _generation; }
    MessageDefinitionSet const& GetDefinitions() const noexcept { return *_tables->Definitions; }
    std::vector<MessageInfo> const& GetMessages() const noexcept { return _tables->Infos; }
    std::size_t GetMessageCount() const noexcept { return _tables->Infos.size(); }
    std::vector<MessageIssue> const& GetWarnings() const noexcept { return _tables->Warnings; }
    bool Contains(MessageInfo const& info) const noexcept;

    MessageInfo const* Find(uint8 serviceId, uint8 order) const noexcept;
    MessageInfo const* Find(uint8 serviceId, std::string_view tag) const noexcept;

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

    template<DeclaredMessage T>
    static bool Resolve(MessageCatalog const& catalog, MessageBinding& binding, std::vector<std::string>& errors);

private:
    friend class MessageRegistry;

    struct Tables
    {
        std::unique_ptr<MessageDefinitionSet> Definitions;
        std::vector<MessageInfo> Infos;
        std::vector<uint16> Index;
        std::vector<MessageIssue> Warnings;
    };

    MessageCatalog(std::shared_ptr<Tables const> tables, std::vector<MessageBinding> bindings, uint64 generation);

    template<DeclaredMessage T>
    MessageBinding const& GetBinding() const;

    static void SkipField(ByteBuffer& buffer, DmlType type);

    std::shared_ptr<Tables const> _tables;
    std::vector<MessageBinding> _bindings;
    uint64 _generation;
};

using MessageCatalogPtr = std::shared_ptr<MessageCatalog const>;
using MessageInfoPtr = std::shared_ptr<MessageInfo const>;

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

    MessageCatalogPtr GetCatalog() const;
    bool IsLoaded() const;
    uint64 GetGeneration() const;
    std::vector<MessageIssue> GetErrors() const;
    std::vector<MessageIssue> GetWarnings() const;

    template<DeclaredMessage... Messages>
    bool Declare(std::vector<std::string>& errors);

    template<DeclaredMessage T>
    bool IsDeclared() const;

    MessageInfoPtr Find(uint8 serviceId, uint8 order) const;
    MessageInfoPtr Find(uint8 serviceId, std::string_view tag) const;
    std::size_t GetMessageCount() const;

    template<DeclaredMessage T>
    MessageInfoPtr GetInfo() const;

    template<DeclaredMessage T>
    void Encode(T const& message, ByteBuffer& buffer) const;

    template<DeclaredMessage T>
    bool Decode(ByteBuffer& buffer, T& message) const;

    template<DeclaredMessage T>
    MessageDecodeStatus Decode(std::span<uint8 const> body, T& message) const;

    static std::filesystem::path GetClientArchivePath(std::filesystem::path const& clientDirectory);

private:
    using Resolver = bool (*)(MessageCatalog const&, MessageBinding&, std::vector<std::string>&);

    struct Declaration
    {
        std::size_t TypeIndex = 0;
        Resolver Resolve = nullptr;
    };

    template<DeclaredMessage T>
    static bool ResolveDeclaration(MessageCatalog const& catalog, MessageBinding& binding, std::vector<std::string>& errors);

    bool DeclareOne(std::size_t typeIndex, Resolver resolve, std::vector<std::string>& errors);
    MessageCatalogPtr GetLoadedCatalog() const;
    void Publish(MessageCatalogPtr catalog);
    void ReportLoadIssues(std::vector<MessageIssue> const& errors, std::vector<MessageIssue> const& warnings) const;

    mutable std::mutex _catalogMutex;
    MessageCatalogPtr _catalog;
    mutable std::mutex _writeMutex;
    std::vector<Declaration> _declarations;
    std::vector<MessageIssue> _errors;
    std::vector<MessageIssue> _warnings;
    uint64 _nextGeneration = 1;
};

#define sMessageRegistry MessageRegistry::Instance()

template<DeclaredMessage T>
bool MessageCatalog::Resolve(MessageCatalog const& catalog, MessageBinding& binding, std::vector<std::string>& errors)
{
    using Fields = std::remove_cvref_t<decltype(T::Fields())>;
    static_assert(IsMessageFieldTuple<T, Fields>::value, "Fields() must return a std::tuple of DmlField() entries whose members belong to the message or its bases and are non-const DML-compatible types");

    uint8 const serviceId = static_cast<uint8>(T::ServiceId);
    std::string_view const tag = T::Tag;
    binding = MessageBinding();
    MessageInfo const* const info = catalog.Find(serviceId, tag);
    if (!info)
    {
        errors.push_back(fmt::format("{} is not a message of service {}", tag, serviceId));
        return false;
    }

    MessageDef const& definition = *info->Definition;
    MessageBinding resolved;
    resolved.Info = info;
    resolved.MemberForField.assign(definition.Fields.size(), -1);
    bool valid = true;
    Fields const fields = T::Fields();
    MessageDeclarationDetail::ForEachField(fields, [&](std::size_t member, auto const& field)
    {
        using Member = std::remove_cvref_t<decltype(std::declval<T&>().*(field.Pointer))>;
        auto const found = std::find_if(definition.Fields.begin(), definition.Fields.end(), [&](FieldDef const& candidate) { return candidate.Name == field.Name; });
        if (found == definition.Fields.end())
        {
            errors.push_back(fmt::format("{} (service {}) has no field {}", tag, serviceId, field.Name));
            valid = false;
            return;
        }
        std::size_t const index = static_cast<std::size_t>(found - definition.Fields.begin());
        if (!MessageMember::IsCompatible<Member>(found->Type))
        {
            errors.push_back(fmt::format("{}.{} is {} in the client definition, which the declared C++ member type cannot hold exactly", tag, field.Name, Dml::GetTypeName(found->Type)));
            valid = false;
            return;
        }
        if (resolved.MemberForField[index] >= 0)
        {
            errors.push_back(fmt::format("{}.{} is declared by more than one member", tag, field.Name));
            valid = false;
            return;
        }
        resolved.MemberForField[index] = static_cast<int32>(member);
    });
    if (!valid)
        return false;
    binding = std::move(resolved);
    return true;
}

template<DeclaredMessage T>
bool MessageCatalog::IsDeclared() const noexcept
{
    std::size_t const slot = MessageDeclarationDetail::TypeIndex<T>();
    return slot < _bindings.size() && _bindings[slot].Info != nullptr;
}

template<DeclaredMessage T>
MessageBinding const& MessageCatalog::GetBinding() const
{
    std::size_t const slot = MessageDeclarationDetail::TypeIndex<T>();
    if (slot >= _bindings.size() || !_bindings[slot].Info)
        throw std::logic_error(fmt::format("{} (service {}) was used before MessageRegistry::Declare resolved it", T::Tag, static_cast<uint32>(T::ServiceId)));
    return _bindings[slot];
}

template<DeclaredMessage T>
MessageInfo const& MessageCatalog::GetInfo() const
{
    return *GetBinding<T>().Info;
}

template<DeclaredMessage T>
void MessageCatalog::Encode(T const& message, ByteBuffer& buffer) const
{
    MessageBinding const& binding = GetBinding<T>();
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
bool MessageCatalog::Decode(ByteBuffer& buffer, T& message) const
{
    MessageBinding const& binding = GetBinding<T>();
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
MessageDecodeStatus MessageCatalog::Decode(std::span<uint8 const> body, T& message) const
{
    ByteBuffer buffer(body);
    if (!Decode(buffer, message))
        return MessageDecodeStatus::Truncated;
    return buffer.GetRemaining() == 0 ? MessageDecodeStatus::Ok : MessageDecodeStatus::TrailingBytes;
}

template<DeclaredMessage... Messages>
bool MessageRegistry::Declare(std::vector<std::string>& errors)
{
    bool succeeded = true;
    ((succeeded = DeclareOne(MessageDeclarationDetail::TypeIndex<Messages>(), &ResolveDeclaration<Messages>, errors) && succeeded), ...);
    return succeeded;
}

template<DeclaredMessage T>
bool MessageRegistry::ResolveDeclaration(MessageCatalog const& catalog, MessageBinding& binding, std::vector<std::string>& errors)
{
    return MessageCatalog::Resolve<T>(catalog, binding, errors);
}

template<DeclaredMessage T>
bool MessageRegistry::IsDeclared() const
{
    MessageCatalogPtr const catalog = GetCatalog();
    return catalog && catalog->IsDeclared<T>();
}

template<DeclaredMessage T>
MessageInfoPtr MessageRegistry::GetInfo() const
{
    MessageCatalogPtr catalog = GetLoadedCatalog();
    MessageInfo const& info = catalog->GetInfo<T>();
    return MessageInfoPtr(std::move(catalog), &info);
}

template<DeclaredMessage T>
void MessageRegistry::Encode(T const& message, ByteBuffer& buffer) const
{
    GetLoadedCatalog()->Encode(message, buffer);
}

template<DeclaredMessage T>
bool MessageRegistry::Decode(ByteBuffer& buffer, T& message) const
{
    return GetLoadedCatalog()->Decode(buffer, message);
}

template<DeclaredMessage T>
MessageDecodeStatus MessageRegistry::Decode(std::span<uint8 const> body, T& message) const
{
    return GetLoadedCatalog()->Decode(body, message);
}

#endif

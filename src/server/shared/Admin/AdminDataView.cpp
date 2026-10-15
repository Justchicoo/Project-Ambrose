/*
 * Project Ambrose by Imjustchico
 * The /api/data/<kind>/ route family built on one byte-free writer: every answer carries the install revision, the type dump's program hash and the store generation it was read from, pages hold at most 200 rows behind an opaque cursor, and a request past its time budget, its byte budget or the concurrent decode limit is refused naming the budget it passed.
 */

#include "AdminDataView.h"
#include "AdminRouter.h"
#include "Base64.h"
#include "PropertyFlags.h"
#include "Settings.h"
#include "TypeInfo.h"
#include "TypeRegistry.h"

#include <nlohmann/json.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    constexpr std::string_view Permission = "clientdata.browse";
    constexpr uint32 DataCost = 5;

    std::mutex& KindsMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    std::vector<std::pair<std::string, std::function<uint64_t()>>>& KindRegistry()
    {
        static std::vector<std::pair<std::string, std::function<uint64_t()>>> kinds;
        return kinds;
    }

    std::string EncodeCursor(std::size_t offset)
    {
        std::string const text = std::to_string(offset);
        std::span<uint8 const> const bytes(reinterpret_cast<uint8 const*>(text.data()), text.size());
        return Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    }

    std::optional<std::size_t> DecodeCursor(std::string_view cursor)
    {
        std::optional<std::vector<uint8>> const bytes = Base64::Decode(cursor, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
        if (!bytes || bytes->empty() || bytes->size() > 20)
            return std::nullopt;
        std::string const text(reinterpret_cast<char const*>(bytes->data()), bytes->size());
        std::size_t offset = 0;
        for (char const digit : text)
        {
            if (digit < '0' || digit > '9')
                return std::nullopt;
            offset = offset * 10 + static_cast<std::size_t>(digit - '0');
        }
        return offset;
    }

    nlohmann::json StampJson(AdminDataView::Stamp const& stamp)
    {
        nlohmann::json out;
        out["revision"] = stamp.Revision;
        out["program_sha256"] = stamp.ProgramSha256;
        out["generation"] = stamp.Generation;
        return out;
    }

    AdminResponse Stamped(int status, nlohmann::json body, AdminDataView::Stamp const& stamp)
    {
        body["schema"] = AdminDataView::SchemaVersion;
        body["stamp"] = StampJson(stamp);
        AdminResponse answer = AdminResponse::Json(status, body.dump());
        answer.Headers.emplace_back("Cache-Control", "no-store");
        return answer;
    }

    std::optional<AdminResponse> RefuseOverBudget(std::string const& body)
    {
        uint64 const byteBudget = sSettings.Get<uint64>("Data.ByteBudgetBytes");
        if (byteBudget > 0 && static_cast<uint64>(body.size()) > byteBudget)
        {
            return AdminResponse::Problem(503, "budget_exceeded",
                fmt::format("Passed the Data.ByteBudgetBytes budget of {} bytes", byteBudget));
        }
        return std::nullopt;
    }

    struct DecodeSlot
    {
        std::atomic<int64_t>& Active;
        bool Held = false;

        explicit DecodeSlot(std::atomic<int64_t>& active) : Active(active)
        {
            uint64 const maxActive = sSettings.Get<uint64>("Data.MaxConcurrentDecodes");
            int64_t const now = Active.fetch_add(1) + 1;
            Held = maxActive == 0 || static_cast<uint64>(now) <= maxActive;
        }

        ~DecodeSlot()
        {
            Active.fetch_sub(1);
        }

        DecodeSlot(DecodeSlot const&) = delete;
        DecodeSlot& operator=(DecodeSlot const&) = delete;
    };

    std::optional<AdminResponse> RefuseWhenSlow(std::chrono::steady_clock::time_point start)
    {
        uint64 const timeBudgetMs = sSettings.Get<uint64>("Data.TimeBudgetMs");
        if (timeBudgetMs == 0)
            return std::nullopt;
        auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        if (static_cast<uint64>(elapsed.count()) > timeBudgetMs)
        {
            return AdminResponse::Problem(503, "budget_exceeded",
                fmt::format("Passed the Data.TimeBudgetMs budget of {} ms", timeBudgetMs));
        }
        return std::nullopt;
    }

    std::string LowerOf(std::string_view text)
    {
        std::string out(text);
        for (char& letter : out)
            letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
        return out;
    }

    std::optional<uint32> ParseHash(std::string_view text)
    {
        if (text.empty())
            return std::nullopt;
        std::string_view digits = text;
        int base = 10;
        if (digits.starts_with("0x") || digits.starts_with("0X"))
        {
            digits.remove_prefix(2);
            base = 16;
        }
        if (digits.empty())
            return std::nullopt;
        uint32 value = 0;
        for (char const digit : digits)
        {
            uint32 nibble = 0xFFFFFFFFu;
            if (digit >= '0' && digit <= '9')
                nibble = static_cast<uint32>(digit - '0');
            else if (base == 16 && digit >= 'a' && digit <= 'f')
                nibble = static_cast<uint32>(digit - 'a' + 10);
            else if (base == 16 && digit >= 'A' && digit <= 'F')
                nibble = static_cast<uint32>(digit - 'A' + 10);
            if (nibble == 0xFFFFFFFFu || (base == 10 && nibble > 9))
                return std::nullopt;
            value = value * static_cast<uint32>(base) + nibble;
        }
        return value;
    }

    std::optional<ClassKind> ParseKind(std::string_view text)
    {
        std::string const lower = LowerOf(text);
        if (lower == "propertyclass" || lower == "property_class")
            return ClassKind::PropertyClass;
        if (lower == "enum")
            return ClassKind::Enum;
        if (lower == "valuetype" || lower == "value_type")
            return ClassKind::ValueType;
        if (lower == "primitive")
            return ClassKind::Primitive;
        if (lower == "container")
            return ClassKind::Container;
        if (lower == "opaque")
            return ClassKind::Opaque;
        return std::nullopt;
    }

    std::optional<PropertyFlag> ParseFlag(std::string_view text)
    {
        std::string const lower = LowerOf(text);
        for (auto const& [flag, name] : PropertyFlags::Names)
            if (LowerOf(name) == lower)
                return flag;
        return std::nullopt;
    }

    bool MatchesQuery(ClassInfo const& info, std::string_view query)
    {
        if (query.empty())
            return true;
        if (std::optional<uint32> const hash = ParseHash(query))
            return info.Hash == *hash;
        return LowerOf(info.Name).find(LowerOf(query)) != std::string::npos;
    }

    bool MatchesKind(ClassInfo const& info, std::optional<ClassKind> kind)
    {
        return !kind || info.Kind == *kind;
    }

    bool MatchesFlag(ClassInfo const& info, std::optional<PropertyFlag> flag)
    {
        if (!flag)
            return true;
        for (PropertyInfo const& property : info.Properties)
            if (property.HasFlag(*flag))
                return true;
        return false;
    }

    nlohmann::json FlagNames(uint32 flags)
    {
        nlohmann::json names = nlohmann::json::array();
        uint32 known = 0;
        for (auto const& [flag, name] : PropertyFlags::Names)
        {
            if ((flags & PropertyFlags::Bit(flag)) != 0)
            {
                names.push_back(std::string(name));
                known |= PropertyFlags::Bit(flag);
            }
        }
        if ((flags & ~known) != 0)
            names.push_back(fmt::format("{:#x}", flags & ~known));
        return names;
    }

    nlohmann::json ClassSummaryJson(ClassInfo const& info, TypeRegistry& registry)
    {
        nlohmann::json out;
        out["name"] = info.Name;
        out["hash"] = info.Hash;
        out["kind"] = std::string(TypeKinds::GetName(info.Kind));
        out["property_count"] = info.Properties.size();
        out["from_server_class"] = registry.IsFromSupplement(info.Hash);
        return out;
    }

    nlohmann::json PropertyJson(PropertyInfo const& property)
    {
        nlohmann::json out;
        out["id"] = property.Id;
        out["name"] = property.Name;
        out["type"] = property.TypeName;
        out["container"] = std::string(TypeKinds::GetName(property.Container));
        out["flags"] = FlagNames(property.Flags);
        out["offset"] = property.Offset;
        out["hash"] = property.Hash;
        nlohmann::json options = nlohmann::json::array();
        for (EnumOption const& option : property.Options)
            options.push_back({ { "name", option.Name }, { "value", option.Value } });
        out["enum_options"] = std::move(options);
        if (property.Default)
        {
            if (int64 const* const number = std::get_if<int64>(&*property.Default))
                out["default"] = *number;
            else if (std::string const* const text = std::get_if<std::string>(&*property.Default))
                out["default"] = *text;
        }
        if (!property.OptionBaseClass.empty())
            out["option_base_class"] = property.OptionBaseClass;
        return out;
    }

    nlohmann::json ClassDetailJson(ClassInfo const& info, TypeCatalog const& catalog, TypeRegistry& registry)
    {
        nlohmann::json out = ClassSummaryJson(info, registry);
        nlohmann::json bases = nlohmann::json::array();
        for (ClassInfo const* base : info.Bases)
            bases.push_back(base != nullptr ? base->Name : std::string("?"));
        out["bases"] = std::move(bases);
        nlohmann::json derived = nlohmann::json::array();
        for (ClassInfo const* candidate : catalog.GetClasses())
        {
            if (candidate == &info || candidate == nullptr)
                continue;
            for (ClassInfo const* base : candidate->Bases)
            {
                if (base == &info)
                {
                    derived.push_back(candidate->Name);
                    break;
                }
            }
        }
        std::sort(derived.begin(), derived.end());
        out["derived"] = std::move(derived);
        nlohmann::json properties = nlohmann::json::array();
        for (PropertyInfo const& property : info.Properties)
            properties.push_back(PropertyJson(property));
        out["properties"] = std::move(properties);
        if (registry.IsFromSupplement(info.Hash))
            out["provenance"] = "server_class";
        else
            out["provenance"] = "type_dump";
        nlohmann::json views = nlohmann::json::array();
        for (ViewBinding const& view : catalog.GetViews())
        {
            if (view.Class == &info && view.Definition != nullptr)
                views.push_back(std::string(view.Definition->Name));
        }
        out["typed_views"] = std::move(views);
        return out;
    }

    AdminResponse ListTypes(AdminRequest const& request, AdminDataView::Stamp const& stamp, TypeRegistry& registry, std::atomic<int64_t>& active)
    {
        auto const start = std::chrono::steady_clock::now();
        DecodeSlot const slot(active);
        if (!slot.Held)
        {
            return AdminResponse::Problem(503, "budget_exceeded",
                fmt::format("Passed the Data.MaxConcurrentDecodes budget of {}", sSettings.Get<uint64>("Data.MaxConcurrentDecodes")));
        }

        std::string_view const limitText = request.Query("limit");
        std::size_t limit = AdminDataView::MaxRowsPerPage;
        if (!limitText.empty())
        {
            std::optional<uint32> const parsed = ParseHash(limitText);
            if (!parsed)
                return AdminResponse::Problem(400, "invalid", "The limit is not a number");
            limit = *parsed;
        }
        if (limit == 0 || limit > AdminDataView::MaxRowsPerPage)
        {
            return AdminResponse::Problem(400, "page_too_large",
                fmt::format("A page holds at most {} rows", AdminDataView::MaxRowsPerPage));
        }

        std::size_t offset = 0;
        std::string_view const cursorText = request.Query("cursor");
        if (!cursorText.empty())
        {
            std::optional<std::size_t> const parsed = DecodeCursor(cursorText);
            if (!parsed)
                return AdminResponse::Problem(400, "invalid", "The cursor is not one this route wrote");
            offset = *parsed;
        }

        std::string_view const query = request.Query("q");
        std::optional<ClassKind> kind;
        std::string_view const kindText = request.Query("kind");
        if (!kindText.empty())
        {
            kind = ParseKind(kindText);
            if (!kind)
                return AdminResponse::Problem(400, "invalid", "The kind is not a class kind the dump holds");
        }
        std::optional<PropertyFlag> flag;
        std::string_view const flagText = request.Query("flag");
        if (!flagText.empty())
        {
            flag = ParseFlag(flagText);
            if (!flag)
                return AdminResponse::Problem(400, "invalid", "The flag is not a property flag the dump holds");
        }

        TypeCatalogPtr const catalog = registry.GetCatalog();
        std::vector<ClassInfo const*> matches;
        if (catalog)
        {
            for (ClassInfo const* info : catalog->GetClasses())
            {
                if (info != nullptr && MatchesQuery(*info, query) && MatchesKind(*info, kind) && MatchesFlag(*info, flag))
                    matches.push_back(info);
            }
        }
        std::sort(matches.begin(), matches.end(),
            [](ClassInfo const* left, ClassInfo const* right) { return left->Name < right->Name; });

        nlohmann::json body;
        nlohmann::json rows = nlohmann::json::array();
        std::size_t const end = std::min(offset + limit, matches.size());
        for (std::size_t index = offset; index < end; ++index)
            rows.push_back(ClassSummaryJson(*matches[index], registry));
        body["rows"] = std::move(rows);
        body["total"] = matches.size();
        if (end < matches.size())
        {
            body["cursor"] = EncodeCursor(end);
            body["has_more"] = true;
        }
        else
        {
            body["has_more"] = false;
        }

        if (std::optional<AdminResponse> slow = RefuseWhenSlow(start))
            return std::move(*slow);
        AdminResponse answer = Stamped(200, std::move(body), stamp);
        if (std::optional<AdminResponse> over = RefuseOverBudget(answer.Body))
            return std::move(*over);
        return answer;
    }

    std::string UrlDecode(std::string_view text)
    {
        std::string out;
        out.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '%' && i + 2 < text.size())
            {
                auto hex = [](char c) -> int {
                    if (c >= '0' && c <= '9')
                        return c - '0';
                    if (c >= 'a' && c <= 'f')
                        return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F')
                        return c - 'A' + 10;
                    return -1;
                };
                int const high = hex(text[i + 1]);
                int const low = hex(text[i + 2]);
                if (high >= 0 && low >= 0)
                {
                    out.push_back(static_cast<char>(high * 16 + low));
                    i += 2;
                    continue;
                }
            }
            out.push_back(text[i] == '+' ? ' ' : text[i]);
        }
        return out;
    }

    AdminResponse ShowType(AdminRequest const& request, AdminDataView::Stamp const& stamp, TypeRegistry& registry, std::atomic<int64_t>& active)
    {
        auto const start = std::chrono::steady_clock::now();
        DecodeSlot const slot(active);
        if (!slot.Held)
        {
            return AdminResponse::Problem(503, "budget_exceeded",
                fmt::format("Passed the Data.MaxConcurrentDecodes budget of {}", sSettings.Get<uint64>("Data.MaxConcurrentDecodes")));
        }

        std::string_view const prefix = "/api/data/types/";
        std::string_view encoded = request.Path.size() > prefix.size() ? std::string_view(request.Path).substr(prefix.size()) : std::string_view();
        std::size_t const slash = encoded.find('/');
        if (slash != std::string_view::npos)
            encoded = encoded.substr(0, slash);
        std::string const name = UrlDecode(encoded);

        TypeCatalogPtr const catalog = registry.GetCatalog();
        ClassInfo const* found = nullptr;
        if (catalog && !name.empty())
        {
            if (std::optional<uint32> const hash = ParseHash(name))
                found = catalog->FindClass(*hash);
            if (found == nullptr)
                found = catalog->FindClass(name);
        }
        if (found == nullptr)
            return AdminResponse::Problem(404, "not_found", "The type registry holds no class under that name or hash");

        nlohmann::json body = ClassDetailJson(*found, *catalog, registry);
        if (std::optional<AdminResponse> slow = RefuseWhenSlow(start))
            return std::move(*slow);
        AdminResponse answer = Stamped(200, std::move(body), stamp);
        if (std::optional<AdminResponse> over = RefuseOverBudget(answer.Body))
            return std::move(*over);
        return answer;
    }
}

void AdminDataView::Register(AdminRouter& router, StampSource source, TypeRegistry& registry)
{
    auto active = std::make_shared<std::atomic<int64_t>>(0);
    router.AddCosting("GET", "/api/data/types", std::string(Permission), DataCost,
        [source, active, &registry](AdminRequest const& request)
        {
            return ListTypes(request, source(), registry, *active);
        });
    router.AddGuardedPrefix("GET", "/api/data/types/", std::string(Permission),
        [source, active, &registry](AdminRequest const& request)
        {
            return ShowType(request, source(), registry, *active);
        });
    RegisterKind("types", [source] { return source().Generation; });
}

void AdminDataView::RegisterKind(std::string_view kind, std::function<uint64_t()> generation)
{
    std::lock_guard const lock(KindsMutex());
    std::string const name(kind);
    for (auto& [registered, _] : KindRegistry())
    {
        if (registered == name)
            return;
    }
    KindRegistry().emplace_back(name, std::move(generation));
}

std::vector<std::pair<std::string, uint64_t>> AdminDataView::DataKinds()
{
    std::lock_guard const lock(KindsMutex());
    std::vector<std::pair<std::string, uint64_t>> kinds;
    for (auto const& [name, generation] : KindRegistry())
        kinds.emplace_back(name, generation());
    return kinds;
}

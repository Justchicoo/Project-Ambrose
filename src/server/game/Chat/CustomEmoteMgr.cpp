/*
 * Project Ambrose by Imjustchico
 * Reads each custom-emote template from the user's install through TemplateFolder, indexes its animation names and ids by the purchased bitfield, and atomically replaces the catalog only after the full folder has loaded.
 */

#include "CustomEmoteMgr.h"
#include "Log.h"
#include "ObjectTemplateMgr.h"
#include "PropertyObject.h"
#include "PropertyValue.h"
#include "ReloadMgr.h"
#include "TemplateFolder.h"
#include "TemplateManifest.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <chrono>
#include <iterator>
#include <optional>
#include <utility>

namespace
{
    constexpr char const* EmoteLog = "server.loading";

    template<typename T>
    T const* Field(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        return value == nullptr ? nullptr : value->GetIf<T>();
    }

    bool ReadEmoteTemplate(PropertyObject const& object, uint32 templateId, std::string_view path, std::vector<CustomEmoteAnimation>& animations, std::string& error)
    {
        PropertyValue const* const behaviorsValue = object.Get("m_behaviors");
        if (behaviorsValue == nullptr)
            return true;
        PropertyValue::List const* const behaviors = behaviorsValue->GetIf<PropertyValue::List>();
        if (behaviors == nullptr)
        {
            error = fmt::format("{} has no readable behavior list", path);
            return false;
        }
        for (PropertyValue const& behaviorValue : *behaviors)
        {
            PropertyObject const* const behavior = behaviorValue.AsObject();
            if (behavior == nullptr || behavior->GetClass().Name != "class CustomEmoteBehaviorTemplate")
                continue;
            int64 const* const type = Field<int64>(*behavior, "m_emoteType");
            if (type == nullptr || (*type != 0 && *type != 1 && *type != 2))
            {
                error = fmt::format("{} has a custom emote with an unknown type", path);
                return false;
            }
            if (*type == 1)
                continue;
            int32 const* const bitFieldNumber = Field<int32>(*behavior, "m_bitFieldNumber");
            bool const* const isDefault = Field<bool>(*behavior, "m_isDefaultEmote");
            std::string const* const animation1 = Field<std::string>(*behavior, "m_animation1");
            std::string const* const animation2 = Field<std::string>(*behavior, "m_animation2");
            if (bitFieldNumber == nullptr || isDefault == nullptr || animation1 == nullptr || animation2 == nullptr)
            {
                error = fmt::format("{} has an incomplete custom emote behavior", path);
                return false;
            }
            if (animation1->empty() && animation2->empty())
            {
                error = fmt::format("{} has a custom emote with no animation", path);
                return false;
            }
            if (!animation1->empty())
                animations.push_back({ *animation1, *bitFieldNumber, *isDefault, templateId });
            if (!animation2->empty())
                animations.push_back({ *animation2, *bitFieldNumber, *isDefault, templateId });
        }
        return true;
    }
}

std::optional<CustomEmoteStore> CustomEmoteStore::Build(std::vector<CustomEmoteAnimation> animations, std::vector<std::string>& errors)
{
    errors.clear();
    CustomEmoteStore store;
    for (CustomEmoteAnimation& animation : animations)
    {
        if (animation.Animation.empty())
        {
            errors.emplace_back("a custom emote animation name is empty");
            return std::nullopt;
        }
        if (!animation.IsDefault && (animation.BitFieldNumber < -1 || animation.BitFieldNumber >= static_cast<int32>(RankCount * 32)))
        {
            errors.push_back(fmt::format("custom emote animation {} has bitfield number {}, outside the three ownership ranks", animation.Animation, animation.BitFieldNumber));
            return std::nullopt;
        }
        CustomEmoteStore::Ownership const ownership{ animation.BitFieldNumber, animation.IsDefault };
        store._animations[animation.Animation].push_back(ownership);
        if (animation.TemplateId != 0)
            store._templates[animation.TemplateId].push_back(ownership);
    }
    if (store._animations.empty())
    {
        errors.emplace_back("the custom-emote templates contain no usable animations");
        return std::nullopt;
    }
    return store;
}

std::optional<CustomEmoteStore> CustomEmoteStore::Read(std::filesystem::path const& gameData, TypeCatalogPtr const& catalog, TemplateManifest const& manifest,
    std::vector<std::string>& errors, std::size_t& threads)
{
    errors.clear();
    threads = 0;
    if (gameData.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so no custom emote can be read");
        return std::nullopt;
    }
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so no custom emote can be read");
        return std::nullopt;
    }
    if (manifest.Size() == 0)
    {
        errors.emplace_back("the template manifest is not loaded, so no custom emote can be found");
        return std::nullopt;
    }
    std::vector<TemplateFolderEntry> const entries = TemplateFolder::List(manifest, Folder);
    if (entries.empty())
    {
        errors.push_back(fmt::format("{} lists no template under {}", TemplateManifest::Entry, Folder));
        return std::nullopt;
    }

    std::vector<std::vector<CustomEmoteAnimation>> decoded(entries.size());
    if (!TemplateFolder::ReadAll(gameData, catalog, entries, "custom emotes", Folder,
            [&decoded](std::size_t index, TemplateFolderEntry const& entry, PropertyObject const& object, std::vector<DecodeIssue> const&, std::string& error)
            {
                return ReadEmoteTemplate(object, entry.Id, entry.Location.Path, decoded[index], error);
            },
            errors, threads))
        return std::nullopt;

    std::vector<CustomEmoteAnimation> animations;
    for (std::vector<CustomEmoteAnimation>& templateAnimations : decoded)
        animations.insert(animations.end(), std::make_move_iterator(templateAnimations.begin()), std::make_move_iterator(templateAnimations.end()));
    return Build(std::move(animations), errors);
}

std::vector<std::string> CustomEmoteStore::AnimationNames() const
{
    std::vector<std::string> names;
    names.reserve(_animations.size());
    for (auto const& animation : _animations)
        names.push_back(animation.first);
    return names;
}

bool CustomEmoteStore::OwnsAnimation(std::string_view animation, std::array<uint32, RankCount> const& ownership) const noexcept
{
    auto const found = _animations.find(animation);
    if (found == _animations.end())
        return false;
    for (Ownership const& requirement : found->second)
    {
        if (requirement.IsDefault)
            return true;
        if (requirement.BitFieldNumber < 0)
            continue;
        uint32 const bit = static_cast<uint32>(requirement.BitFieldNumber);
        if ((ownership[bit / 32] & (uint32{ 1 } << (bit % 32))) != 0)
            return true;
    }
    return false;
}

std::vector<uint32> CustomEmoteStore::OwnedTemplateIds(std::array<uint32, RankCount> const& ownership) const
{
    std::vector<uint32> templates;
    for (auto const& [templateId, requirements] : _templates)
    {
        for (Ownership const& requirement : requirements)
        {
            if (requirement.IsDefault)
            {
                templates.push_back(templateId);
                break;
            }
            if (requirement.BitFieldNumber < 0)
                continue;
            uint32 const bit = static_cast<uint32>(requirement.BitFieldNumber);
            if ((ownership[bit / 32] & (uint32{ 1 } << (bit % 32))) != 0)
            {
                templates.push_back(templateId);
                break;
            }
        }
    }
    return templates;
}

CustomEmoteMgr& CustomEmoteMgr::Instance()
{
    static CustomEmoteMgr instance;
    return instance;
}

void CustomEmoteMgr::SetInstall(std::filesystem::path root)
{
    std::lock_guard const lock(_installMutex);
    _install = std::move(root);
}

void CustomEmoteMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); }, { std::string(ObjectTemplateMgr::ManifestTarget) });
}

std::filesystem::path CustomEmoteMgr::GameData() const
{
    std::lock_guard const lock(_installMutex);
    return _install.empty() ? std::filesystem::path() : _install / "Data" / "GameData";
}

bool CustomEmoteMgr::Load(std::vector<std::string>& errors)
{
    std::filesystem::path const gameData = GameData();
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    std::shared_ptr<TemplateManifest const> const manifest = sObjectTemplateMgr.GetManifest();
    auto const started = std::chrono::steady_clock::now();
    std::size_t threads = 0;
    std::optional<CustomEmoteStore> store = CustomEmoteStore::Read(gameData, catalog, *manifest, errors, threads);
    if (!store)
        return false;
    std::size_t const count = store->Size();
    _emotes.Replace(std::move(*store));
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(EmoteLog, "Read {} custom-emote animations from {} in {} ms using {} worker(s)", count, Folder, took.count(), threads);
    return true;
}

void CustomEmoteMgr::Clear()
{
    _emotes.Replace(CustomEmoteStore{});
}

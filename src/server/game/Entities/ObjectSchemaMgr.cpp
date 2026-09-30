/*
 * Project Ambrose by Imjustchico
 * Reads server_class with its bases, properties and the options of its enum properties through ServerClassScript and hands it to the type registry, which checks every hash and rebuilds its catalog with it; reads core_object_type with core_template_type in one snapshot, and behavior_client_class, into tables checked against the catalog then in use, where a class must be listed, a game object's must be a CoreObject, a template class a CoreTemplate and a behavior's a BehaviorInstance, so a row the client would never accept is refused with its name before anything is swapped. A base list with a gap or a class named twice is a refusal too, because either means a row was lost or doubled rather than meant.
 */

#include "ObjectSchemaMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ReloadMgr.h"
#include "ServerClassScript.h"
#include "TypeDumpLoader.h"
#include "WorldDatabase.h"

#include <fmt/format.h>

#include <map>
#include <utility>

namespace
{
    constexpr char const* SchemaLog = "server.loading";

    std::vector<CoreObjectType> ReadCoreObjectTypes(PreparedResultSet* result)
    {
        std::vector<CoreObjectType> types;
        if (!result || result->GetRowCount() == 0)
            return types;
        do
        {
            Field const* const row = result->Fetch();
            CoreObjectType type;
            type.CoreType = row[0].Get<uint8>();
            type.ClassName = row[1].Get<std::string>();
            types.push_back(std::move(type));
        } while (result->NextRow());
        return types;
    }

    std::vector<CoreTemplateType> ReadCoreTemplateTypes(PreparedResultSet* result)
    {
        std::vector<CoreTemplateType> templates;
        if (!result || result->GetRowCount() == 0)
            return templates;
        do
        {
            Field const* const row = result->Fetch();
            CoreTemplateType entry;
            entry.TemplateClass = row[0].Get<std::string>();
            entry.CoreType = row[1].Get<uint8>();
            entry.TemplateType = row[2].Get<uint8>();
            templates.push_back(std::move(entry));
        } while (result->NextRow());
        return templates;
    }

    std::vector<BehaviorClientClass> ReadBehaviorClientClasses(PreparedResultSet* result)
    {
        std::vector<BehaviorClientClass> rows;
        if (!result || result->GetRowCount() == 0)
            return rows;
        do
        {
            Field const* const row = result->Fetch();
            BehaviorClientClass entry;
            entry.BehaviorName = row[0].Get<std::string>();
            if (!row[1].IsNull())
                entry.ClassName = row[1].Get<std::string>();
            rows.push_back(std::move(entry));
        } while (result->NextRow());
        return rows;
    }
}

std::shared_ptr<BehaviorClientClasses const> BehaviorClientClasses::Build(std::vector<BehaviorClientClass> rows, TypeCatalog const& catalog, std::vector<std::string>& errors)
{
    std::size_t const before = errors.size();
    ClassInfo const* const behavior = catalog.FindClass(BehaviorClass);
    if (!behavior)
        errors.push_back(fmt::format("the type dump does not list {}, so no class can be checked to be a behavior", BehaviorClass));
    auto classes = std::make_shared<BehaviorClientClasses>();
    for (BehaviorClientClass& entry : rows)
    {
        if (entry.BehaviorName.empty())
        {
            errors.emplace_back("behavior_client_class has a row with no behavior name");
            continue;
        }
        if (entry.ClassName)
        {
            ClassInfo const* const type = catalog.FindClass(*entry.ClassName);
            if (!type)
            {
                errors.push_back(fmt::format("behavior {} names {}, which the type dump does not list", entry.BehaviorName, *entry.ClassName));
                continue;
            }
            if (type->Kind != ClassKind::PropertyClass || (behavior && !type->IsA(*behavior)))
            {
                errors.push_back(fmt::format("behavior {} names {}, which is not a {}", entry.BehaviorName, type->Name, BehaviorClass));
                continue;
            }
            entry.ClassName = type->Name;
            entry.ClassHash = type->Hash;
        }
        std::string const name = entry.BehaviorName;
        if (!classes->_byName.emplace(name, std::move(entry)).second)
            errors.push_back(fmt::format("behavior {} is listed twice", name));
    }
    if (errors.size() != before)
        return nullptr;
    return classes;
}

BehaviorClientClass const* BehaviorClientClasses::Find(std::string_view behaviorName) const
{
    auto const found = _byName.find(behaviorName);
    return found == _byName.end() ? nullptr : &found->second;
}

ObjectSchemaMgr& ObjectSchemaMgr::Instance()
{
    static ObjectSchemaMgr instance;
    return instance;
}

void ObjectSchemaMgr::RegisterClassReloadTarget()
{
    sReloadMgr.Register(std::string(ClassTarget), [this](std::vector<std::string>& errors) { return LoadClasses(errors); });
}

void ObjectSchemaMgr::RegisterReloadTargets()
{
    RegisterClassReloadTarget();
    sReloadMgr.Register(std::string(CoreObjectTypeTarget), [this](std::vector<std::string>& errors) { return LoadCoreObjectTypes(errors); }, { std::string(ClassTarget) });
    sReloadMgr.Register(std::string(CoreTemplateTypeTarget), [this](std::vector<std::string>& errors) { return LoadCoreObjectTypes(errors); }, { std::string(ClassTarget) });
    sReloadMgr.Register(std::string(BehaviorTarget), [this](std::vector<std::string>& errors) { return LoadBehaviorClientClasses(errors); }, { std::string(ClassTarget) });
}

bool ObjectSchemaMgr::LoadClasses(std::vector<std::string>& errors)
{
    TypeDumpLoader::RawDump dump;
    if (!ServerClassScript::Read(dump, errors))
        return false;
    return sTypeRegistry.SetSupplement(std::move(dump), std::string(ClassSource), errors);
}

bool ObjectSchemaMgr::LoadCoreObjectTypes(std::vector<std::string>& errors)
{
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so the classes core_object_type and core_template_type name cannot be checked");
        return false;
    }
    auto const types = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(WORLD_SEL_CORE_OBJECT_TYPES) : nullptr;
    auto const templates = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(WORLD_SEL_CORE_TEMPLATE_TYPES) : nullptr;
    if (!types || !templates)
    {
        errors.emplace_back("the world database is not open, so the core object types cannot be read");
        return false;
    }
    std::vector<PreparedQueryResult> results;
    if (!WorldDatabase.QuerySnapshot({ types.get(), templates.get() }, results))
    {
        errors.emplace_back("core_object_type and core_template_type cannot be read from the world database");
        return false;
    }
    CoreObjectTypeTablePtr table = CoreObjectTypeTable::Build(ReadCoreObjectTypes(results[0].get()), ReadCoreTemplateTypes(results[1].get()), *catalog, errors);
    if (!table)
        return false;
    _coreObjectTypes.Replace(std::move(table));
    return true;
}

bool ObjectSchemaMgr::LoadBehaviorClientClasses(std::vector<std::string>& errors)
{
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so the classes behavior_client_class names cannot be checked");
        return false;
    }
    auto const statement = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(WORLD_SEL_BEHAVIOR_CLIENT_CLASSES) : nullptr;
    if (!statement)
    {
        errors.emplace_back("the world database is not open, so the behavior classes cannot be read");
        return false;
    }
    std::shared_ptr<BehaviorClientClasses const> classes = BehaviorClientClasses::Build(ReadBehaviorClientClasses(WorldDatabase.Query(*statement).get()), *catalog, errors);
    if (!classes)
        return false;
    _behaviors.Replace(std::move(classes));
    return true;
}

ObjectSchemaLoadResult ObjectSchemaMgr::LoadTables()
{
    ObjectSchemaLoadResult outcome;
    auto const started = std::chrono::steady_clock::now();
    bool const types = LoadCoreObjectTypes(outcome.Errors);
    bool const behaviors = LoadBehaviorClientClasses(outcome.Errors);
    outcome.Took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    outcome.Loaded = types && behaviors;
    outcome.Classes = sTypeRegistry.GetSupplementClassCount();
    outcome.CoreObjectTypes = GetCoreObjectTypes()->Count();
    outcome.Behaviors = GetBehaviorClientClasses()->Count();
    if (outcome.Loaded)
        LOG_INFO(SchemaLog, "Loaded {} core object type(s) and {} behavior class(es) in {} ms", outcome.CoreObjectTypes, outcome.Behaviors, outcome.Took.count());
    return outcome;
}

void ObjectSchemaMgr::Clear()
{
    _coreObjectTypes.Replace(CoreObjectTypeTable());
    _behaviors.Replace(BehaviorClientClasses());
}

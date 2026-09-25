/*
 * Project Ambrose by Imjustchico
 * What the world database knows about the client's object classes that the type dump cannot say (sObjectSchemaMgr): the classes the dump does not describe, which it hands to the type registry to rebuild its catalog with; the block and type each game object class is created with; and the class the client builds for each behavior a template names, or none where the client takes that behavior's slot empty. Each is read in one snapshot of its tables, built and checked off to the side against the catalog in use and swapped in whole, a build that fails keeps what was serving and names every row at fault, and each reloads alone, the core object types and behavior classes after the classes they may name.
 */

#ifndef AMBROSE_OBJECTSCHEMAMGR_H
#define AMBROSE_OBJECTSCHEMAMGR_H

#include "CoreObjectSerializer.h"
#include "ReloadableStore.h"
#include "Types.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PreparedResultSet;

struct BehaviorClientClass
{
    std::string BehaviorName;
    std::optional<std::string> ClassName;
    uint32 ClassHash = 0;
};

class BehaviorClientClasses
{
public:
    static constexpr std::string_view BehaviorClass = "class BehaviorInstance";

    BehaviorClientClasses() = default;

    static std::shared_ptr<BehaviorClientClasses const> Build(std::vector<BehaviorClientClass> rows, TypeCatalog const& catalog, std::vector<std::string>& errors);

    BehaviorClientClass const* Find(std::string_view behaviorName) const;
    std::size_t Count() const noexcept { return _byName.size(); }

private:
    std::map<std::string, BehaviorClientClass, std::less<>> _byName;
};

struct ObjectSchemaLoadResult
{
    bool Loaded = false;
    std::size_t Classes = 0;
    std::size_t CoreObjectTypes = 0;
    std::size_t Behaviors = 0;
    std::chrono::milliseconds Took{ 0 };
    std::vector<std::string> Errors;
};

class ObjectSchemaMgr
{
public:
    static constexpr std::string_view ClassTarget = "server_class_schema";
    static constexpr std::string_view CoreObjectTypeTarget = "core_object_type";
    static constexpr std::string_view BehaviorTarget = "behavior_client_class";
    static constexpr std::string_view ClassSource = "the world database's server_class tables";

    static ObjectSchemaMgr& Instance();

    ObjectSchemaMgr(ObjectSchemaMgr const&) = delete;
    ObjectSchemaMgr& operator=(ObjectSchemaMgr const&) = delete;

    void RegisterClassReloadTarget();
    void RegisterReloadTargets();
    bool LoadClasses(std::vector<std::string>& errors);
    bool LoadCoreObjectTypes(std::vector<std::string>& errors);
    bool LoadBehaviorClientClasses(std::vector<std::string>& errors);
    ObjectSchemaLoadResult LoadTables();

    CoreObjectTypeTablePtr GetCoreObjectTypes() const { return _coreObjectTypes.Get(); }
    std::shared_ptr<BehaviorClientClasses const> GetBehaviorClientClasses() const { return _behaviors.Get(); }
    void Clear();

private:
    ObjectSchemaMgr() = default;

    ReloadableStore<CoreObjectTypeTable> _coreObjectTypes;
    ReloadableStore<BehaviorClientClasses> _behaviors;
};

#define sObjectSchemaMgr ObjectSchemaMgr::Instance()

#endif

/*
 * Project Ambrose by Imjustchico
 * Every Lua script of the install's Scripts folder, read whole from Root.wad and kept by its name under that folder, looked up as the client's archives are, whatever the case or slashes a caller spells it with, and handed to a LuaState as the reader its Include calls.
 */

#ifndef AMBROSE_LUASCRIPTLIBRARY_H
#define AMBROSE_LUASCRIPTLIBRARY_H

#include "LuaState.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class KiwadArchive;

class LuaScriptLibrary
{
public:
    static constexpr std::string_view Extension = ".lua";

    static std::shared_ptr<LuaScriptLibrary const> Read(KiwadArchive const& root, std::vector<std::string>& errors);
    static std::shared_ptr<LuaScriptLibrary const> Build(std::map<std::string, std::string> scripts);
    static LuaState::SourceReader ReaderOf(std::shared_ptr<LuaScriptLibrary const> library);

    std::optional<std::string> Find(std::string_view name) const;
    std::vector<std::string> GetNames() const;
    std::size_t Size() const noexcept { return _scripts.size(); }

private:
    struct Script
    {
        std::string Name;
        std::string Source;
    };

    std::map<std::string, Script, std::less<>> _scripts;
};

#endif

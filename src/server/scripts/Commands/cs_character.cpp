/*
 * Project Ambrose by Imjustchico
 * The character group: what a game master changes about a wizard. 'level', 'gold' and 'xp' take a value and for now say what they would set and refuse politely, because the character they would reach lives in a world no session has entered yet; milestone 4.09 gives them the player to act on, and the table, the levels and the parsing they are reached through do not change when it does. 'deleted list' shows the newest deleted wizards, of one account or all, 'deleted restore' gives one back to the account it was deleted from, which sees it on its select screen at its next list, and 'rename' makes a wizard choose a new name at its next login.
 */

#include "AccountMgr.h"
#include "CharacterRepository.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "ScriptMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <optional>

namespace
{
    bool NotInAWorldYet(CommandCaller& caller, std::string what, std::vector<std::string> const& arguments)
    {
        if (arguments.empty())
        {
            caller.Reply(fmt::format("Give the {} to set, and the character to set it on", what));
            return false;
        }
        std::optional<int64> const value = Ambrose::StringTo<int64>(arguments.front());
        if (!value)
        {
            caller.Reply(fmt::format("{} is not a number", arguments.front()));
            return false;
        }
        caller.Reply(fmt::format("Setting {} to {} needs a character in a world, which no session has entered yet", what, *value));
        return true;
    }

    constexpr uint32 DeletedListed = 20;

    std::string_view Describe(CharacterOpResult result)
    {
        switch (result)
        {
            case CharacterOpResult::Ok: return "done";
            case CharacterOpResult::NotFound: return "no such character";
            case CharacterOpResult::CharacterOnline: return "the character is in the world";
            case CharacterOpResult::DatabaseError: return "the characters database failed or is not open";
            default: return "the character could not be changed";
        }
    }

    bool DeletedList(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        uint64 account = 0;
        if (arguments.size() > 1)
        {
            caller.Reply("Give an account's username to list its deleted wizards, or nothing to list everyone's");
            return false;
        }
        if (arguments.size() == 1)
        {
            AccountLookup const lookup = sAccountMgr.GetAccountByName(arguments[0]);
            if (!lookup.Account)
            {
                caller.Reply(fmt::format("Account {} does not exist", arguments[0]));
                return false;
            }
            account = lookup.Account->Id;
        }
        std::optional<std::vector<DeletedCharacter>> const deleted = CharacterRepository::ListDeleted(account, DeletedListed);
        if (!deleted)
        {
            caller.Reply("The characters database failed or is not open");
            return false;
        }
        for (DeletedCharacter const& character : *deleted)
            caller.Reply(fmt::format("character {} of account {}, level {}, school {}, deleted at {}", character.Guid, character.Account, character.Level, character.School, character.DeletedAt));
        caller.Reply(fmt::format("{} deleted wizard(s) listed, newest first, at most {}", deleted->size(), DeletedListed));
        return true;
    }

    bool DeletedRestore(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<uint64> const guid = arguments.size() == 1 ? Ambrose::StringTo<uint64>(arguments[0]) : std::nullopt;
        if (!guid)
        {
            caller.Reply("Give the character id of the deleted wizard to restore");
            return false;
        }
        CharacterOpResult const result = CharacterRepository::Restore(*guid);
        caller.Reply(result == CharacterOpResult::Ok ? fmt::format("Restored character {} to the account it was deleted from", *guid)
            : fmt::format("Character {} was not restored: {}", *guid, result == CharacterOpResult::NotFound ? std::string_view("no deleted wizard has that id") : Describe(result)));
        return result == CharacterOpResult::Ok;
    }

    bool Rename(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<uint64> const guid = arguments.size() == 1 ? Ambrose::StringTo<uint64>(arguments[0]) : std::nullopt;
        if (!guid)
        {
            caller.Reply("Give the character id of the wizard that should choose a new name");
            return false;
        }
        CharacterOpResult const result = CharacterRepository::FlagRename(*guid);
        caller.Reply(result == CharacterOpResult::Ok ? fmt::format("Character {} will choose a new name at its next login", *guid)
            : fmt::format("Character {} was not flagged: {}", *guid, Describe(result)));
        return result == CharacterOpResult::Ok;
    }

    class CharacterCommands : public CommandScript
    {
    public:
        CharacterCommands() : CommandScript("cs_character") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            auto const sets = [](char const* what)
            {
                return [what](CommandCaller& caller, std::vector<std::string> const& arguments) { return NotInAWorldYet(caller, what, arguments); };
            };
            return {
                { .Name = "character", .SecurityLevel = SEC_GAMEMASTER, .Help = "change a wizard", .Children = {
                    { .Name = "level", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a wizard's level", .Run = sets("level") },
                    { .Name = "gold", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a wizard's gold", .Run = sets("gold") },
                    { .Name = "xp", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a wizard's experience", .Run = sets("experience") },
                    { .Name = "deleted", .SecurityLevel = SEC_GAMEMASTER, .Help = "deleted wizards", .Children = {
                        { .Name = "list", .SecurityLevel = SEC_GAMEMASTER, .Help = "list the newest deleted wizards: [username]", .Run = DeletedList },
                        { .Name = "restore", .SecurityLevel = SEC_GAMEMASTER, .Help = "give a deleted wizard back to its account: <character id>", .Run = DeletedRestore },
                    } },
                    { .Name = "rename", .SecurityLevel = SEC_GAMEMASTER, .Help = "make a wizard choose a new name at its next login: <character id>", .Run = Rename },
                } },
            };
        }
    };
}

void AddSC_cs_character()
{
    new CharacterCommands();
}

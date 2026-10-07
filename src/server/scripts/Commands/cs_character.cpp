/*
 * Project Ambrose by Imjustchico
 * Game-master commands for changing the current wizard's live stats and potion capacity, managing deleted characters and requesting a rename.
 */

#include "AccountMgr.h"
#include "CharacterRepository.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "GameSession.h"
#include "ScriptMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <string_view>

namespace
{
    GameSession* InWorldSession(CommandCaller& caller)
    {
        GameSession* const session = caller.GetGameSession();
        if (!session || !session->GetStats())
        {
            caller.Reply("This command needs a wizard in the world.");
            return nullptr;
        }
        return session;
    }

    std::optional<int64> ReadValue(CommandCaller& caller, std::string_view what, std::vector<std::string> const& arguments)
    {
        if (arguments.size() != 1)
        {
            caller.Reply(fmt::format("Usage: .character {} <value>", what));
            return std::nullopt;
        }
        std::optional<int64> const value = Ambrose::StringTo<int64>(arguments.front());
        if (!value)
        {
            caller.Reply(fmt::format("{} is not a number", arguments.front()));
            return std::nullopt;
        }
        return value;
    }

    bool SetGold(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const value = ReadValue(caller, "gold", arguments);
        if (!value)
            return false;
        GameSession* const session = InWorldSession(caller);
        if (!session)
            return false;
        session->SetGold(*value);
        PlayerStats const* const stats = session->GetStats();
        caller.Reply(fmt::format("Gold is now {} of {}.", stats->GetGold(), stats->GetBase().Gold));
        return true;
    }

    bool SetMana(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const value = ReadValue(caller, "mana", arguments);
        if (!value)
            return false;
        GameSession* const session = InWorldSession(caller);
        if (!session)
            return false;
        session->SetMana(static_cast<int32>(std::clamp<int64>(*value, std::numeric_limits<int32>::min(), std::numeric_limits<int32>::max())));
        PlayerStats const* const stats = session->GetStats();
        caller.Reply(fmt::format("Mana is now {} of {}.", stats->GetMana(), stats->GetMaxMana()));
        return true;
    }

    bool Heal(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        if (!arguments.empty())
        {
            caller.Reply("Usage: .character heal");
            return false;
        }
        GameSession* const session = InWorldSession(caller);
        if (!session)
            return false;
        PlayerStats const* const stats = session->GetStats();
        session->SetHealth(stats->GetMaxHitpoints());
        caller.Reply(fmt::format("Health is now {} of {}.", stats->GetHitpoints(), stats->GetMaxHitpoints()));
        return true;
    }

    bool SetPotion(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const value = ReadValue(caller, "potion", arguments);
        if (!value)
            return false;
        if (*value < 0 || *value > 1000)
        {
            caller.Reply("Potion capacity must be between 0 and 1000.");
            return false;
        }
        GameSession* const session = InWorldSession(caller);
        if (!session)
            return false;
        session->SetPotionCapacity(static_cast<uint32>(*value));
        PlayerStats const* const stats = session->GetStats();
        caller.Reply(fmt::format("Potion charges are now {} of {}.", stats->GetPotionCharge(), stats->GetPotionMax()));
        return true;
    }

    bool NotInWorldYet(CommandCaller& caller, std::string what, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const value = ReadValue(caller, what, arguments);
        if (!value)
            return false;
        if (!InWorldSession(caller))
            return false;
        caller.Reply(fmt::format("Setting {} to {} is not implemented yet", what, *value));
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
                return [what](CommandCaller& caller, std::vector<std::string> const& arguments) { return NotInWorldYet(caller, what, arguments); };
            };
            return {
                { .Name = "character", .SecurityLevel = SEC_GAMEMASTER, .Help = "change a wizard", .Children = {
                    { .Name = "level", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a wizard's level", .Run = sets("level") },
                    { .Name = "gold", .SecurityLevel = SEC_GAMEMASTER, .Help = "set the current wizard's gold", .Run = SetGold },
                    { .Name = "mana", .SecurityLevel = SEC_GAMEMASTER, .Help = "set the current wizard's mana", .Run = SetMana },
                    { .Name = "heal", .SecurityLevel = SEC_GAMEMASTER, .Help = "restore the current wizard to full health", .Run = Heal },
                    { .Name = "potion", .SecurityLevel = SEC_GAMEMASTER, .Help = "set the current wizard's potion capacity and fill it", .Run = SetPotion },
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

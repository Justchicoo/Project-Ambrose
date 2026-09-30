/*
 * Project Ambrose by Imjustchico
 * Answers the client's friend, best-friend, max-friend and ignore messages through the social manager.
 */

#include "GameSession.h"

#include "Log.h"
#include "SocialMgr.h"

void GameSession::HandleBuddyRequestList(GameMessages::BuddyRequestList& message)
{
    if (!SocialMgr::IsRequestOwnerForCharacter(message.ListOwnerGid, GetCharacterId()))
    {
        LOG_WARN("server.social", "Session {} requested the buddy list for wizard {}", GetSessionId(), message.ListOwnerGid);
        return;
    }
    sSocialMgr.SendLists(*this);
}

void GameSession::HandleBuddyRequestAdd(GameMessages::BuddyRequestAdd& message)
{
    sSocialMgr.AddFriendRequest(*this, message);
}

void GameSession::HandleBuddyRequestAccept(GameMessages::BuddyRequestAccept& message)
{
    sSocialMgr.AcceptFriendRequest(*this, message);
}

void GameSession::HandleBuddyRequestDeny(GameMessages::BuddyRequestDeny& message)
{
    sSocialMgr.DenyFriendRequest(*this, message);
}

void GameSession::HandleBuddyRequestDrop(GameMessages::BuddyRequestDrop& message)
{
    sSocialMgr.DropFriendRequest(*this, message);
}

void GameSession::HandleBestFriend(GameMessages::BestFriend& message)
{
    sSocialMgr.SetBestFriend(*this, message);
}

void GameSession::HandleRequestMaxFriends(GameMessages::RequestMaxFriends& message)
{
    sSocialMgr.SendMaximumFriends(*this, message);
}

void GameSession::HandleIgnoreAdd(GameMessages::IgnoreAdd& message)
{
    sSocialMgr.AddIgnore(*this, message);
}

void GameSession::HandleIgnoreDrop(GameMessages::IgnoreDrop& message)
{
    sSocialMgr.DropIgnore(*this, message);
}

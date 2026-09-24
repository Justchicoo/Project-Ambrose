/*
 * Project Ambrose by Imjustchico
 * Checks the login message table against the user's own client install: its declarations resolve, every one of the 29 LOGIN messages and every SYSTEM and EXTENDEDBASE message has exactly one rule with matching order and tag, game messages stay outside the login server's services, the shared ping rule is handled, and a server message encodes against the real definitions.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "LoginMessageTable.h"
#include "MessageRegistry.h"
#include "SystemMessages.h"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

TEST(LoginMessageTableClientTest, EveryLoginMessageHasOneRuleThatMatchesTheInstall)
{
    std::optional<std::string> const directory = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
    if (!directory || directory->empty())
        GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to a Wizard101 install folder to run client data tests";

    MessageHandlerTable<LoginSession> const& table = LoginMessageTable::Get();
    MessageRegistry registry;
    std::vector<std::string> errors;
    ASSERT_TRUE(table.Declare(registry, errors));
    ASSERT_TRUE(registry.LoadFromClient(LogConfig::Utf8Path(*directory)));
    MessageCatalogPtr const catalog = registry.GetCatalog();
    ASSERT_TRUE(catalog);
    ASSERT_TRUE(table.Validate(*catalog, errors)) << (errors.empty() ? std::string() : errors.front());
    EXPECT_TRUE(errors.empty());

    auto const& protocols = catalog->GetDefinitions().GetProtocols();
    auto const login = protocols.find(LoginMessages::LoginService);
    ASSERT_NE(login, protocols.end());
    EXPECT_EQ(login->second.ProtocolType, "LOGIN");
    ASSERT_EQ(login->second.Messages.size(), 29u);

    std::set<uint32> orders;
    std::size_t handled = 0;
    std::size_t pending = 0;
    std::size_t refused = 0;
    for (MessageDef const& message : login->second.Messages)
    {
        orders.insert(message.Order);
        MessageRule const* const rule = table.FindRule(catalog, LoginMessages::LoginService, message.Order);
        ASSERT_NE(rule, nullptr) << message.Tag << " has no rule";
        EXPECT_EQ(rule->Tag, message.Tag);
        switch (rule->Kind)
        {
            case MessageRuleKind::Handled: ++handled; break;
            case MessageRuleKind::Pending: ++pending; break;
            case MessageRuleKind::Refused: ++refused; break;
        }
    }
    EXPECT_EQ(orders.size(), 29u);
    EXPECT_EQ(*orders.begin(), 1u);
    EXPECT_EQ(*orders.rbegin(), 29u);
    EXPECT_EQ(handled, 9u) << "MSG_SELECTCHARACTER joined the handled messages when 4.05 answered it";
    EXPECT_EQ(pending, 7u) << "and left the pending ones, which is the same message counted once either way";
    EXPECT_EQ(refused, 13u);
    EXPECT_EQ(handled + pending + refused, orders.size()) << "every message of this service is counted exactly once";

    MessageRule const* const authen = table.FindRule(catalog, LoginMessages::LoginService, 27);
    ASSERT_NE(authen, nullptr);
    EXPECT_EQ(authen->Tag, "MSG_USER_AUTHEN_V3");
    EXPECT_EQ(authen->Kind, MessageRuleKind::Handled);
    EXPECT_EQ(authen->HandlerName, "LoginSession::HandleUserAuthenV3");
    EXPECT_EQ(authen->Statuses, SessionStatuses::Connected);

    MessageInfo const* const attach = catalog->Find(5, 7);
    ASSERT_NE(attach, nullptr);
    EXPECT_EQ(attach->Definition->Tag, "MSG_ATTACH");
    EXPECT_EQ(table.FindRule(catalog, 5, 7), nullptr);
    EXPECT_FALSE(table.IsOwnService(5));

    MessageInfo const* const serverMessageInfo = catalog->Find(SystemMessages::ExtendedBaseService, "MSG_SERVERMESSAGE");
    ASSERT_NE(serverMessageInfo, nullptr);
    MessageRule const* const serverMessage = table.FindRule(catalog, SystemMessages::ExtendedBaseService, serverMessageInfo->Definition->Order);
    ASSERT_NE(serverMessage, nullptr);
    EXPECT_EQ(serverMessage->Kind, MessageRuleKind::Refused);

    for (uint8 const service : { SystemMessages::SystemService, SystemMessages::ExtendedBaseService })
    {
        auto const protocol = protocols.find(service);
        ASSERT_NE(protocol, protocols.end());
        for (MessageDef const& message : protocol->second.Messages)
        {
            MessageRule const* const rule = table.FindRule(catalog, service, message.Order);
            ASSERT_NE(rule, nullptr) << message.Tag << " has no rule";
            EXPECT_EQ(rule->Tag, message.Tag);
        }
    }
    EXPECT_EQ(table.GetRules().size(), 37u);

    MessageInfo const* const pingInfo = catalog->Find(SystemMessages::SystemService, "MSG_PING");
    ASSERT_NE(pingInfo, nullptr);
    MessageRule const* const ping = table.FindRule(catalog, SystemMessages::SystemService, pingInfo->Definition->Order);
    ASSERT_NE(ping, nullptr);
    EXPECT_EQ(ping->Kind, MessageRuleKind::Handled);
    EXPECT_EQ(ping->HandlerName, "SessionBase::HandlePing");

    EXPECT_EQ(serverMessageInfo->Definition->Order, 6);
    MessageInfo const* const forceDisconnect = catalog->Find(SystemMessages::ExtendedBaseService, "MSG_FORCE_DISCONNECT");
    ASSERT_NE(forceDisconnect, nullptr);
    EXPECT_EQ(forceDisconnect->Definition->Order, 3);

    SystemMessages::ServerMessage shown;
    shown.Modal = 1;
    shown.Message = u"Hi";
    ByteBuffer encoded;
    catalog->Encode(shown, encoded);
    EXPECT_EQ(std::vector<uint8>(encoded.GetData().begin(), encoded.GetData().end()), (std::vector<uint8>{ 1, 2, 0, 'H', 0, 'i', 0 }));
}

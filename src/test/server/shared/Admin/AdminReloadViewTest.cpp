/*
 * Project Ambrose by Imjustchico
 * Tests the reload routes through the admin router: the listing names every target in the order it runs with its generation, whether it ran, how it went and when it finished, a reload naming no target and one naming a target nobody registered answer 404, a target that fails answers 409 with every error while one that holds answers 200 with its new generation, and all answers every target with whether each held.
 */

#include "AdminAuth.h"
#include "AdminReloadView.h"
#include "AdminRouter.h"
#include "ReloadMgr.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";

    class AdminReloadViewTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sReloadMgr.Clear();
            _auth.SetToken(Token);
            AdminReloadView::Register(_routes);
            sReloadMgr.Register("config", [](std::vector<std::string>&) { return true; });
            sReloadMgr.Register("messages", [this](std::vector<std::string>& errors)
            {
                if (!_broken)
                    return true;
                errors.push_back("the first fault");
                errors.push_back("the second fault");
                return false;
            }, { "config" });
        }

        void TearDown() override { sReloadMgr.Clear(); }

        nlohmann::json Answer(std::string method, std::string path, int expected)
        {
            AdminRequest request;
            request.Method = std::move(method);
            request.Path = std::move(path);
            request.RemoteAddress = "127.0.0.1";
            request.Authorization = std::string("Bearer ") + Token;
            AdminResponse const response = _routes.Dispatch(request);
            EXPECT_EQ(response.Status, expected) << request.Path << ": " << response.Body;
            return nlohmann::json::parse(response.Body, nullptr, false);
        }

        AdminAuth _auth{ 10, 1.0 };
        AdminRouter _routes{ _auth };
        bool _broken = false;
    };
}

TEST_F(AdminReloadViewTest, TheListingNamesEveryTargetInOrderWithWhatItLastDid)
{
    nlohmann::json const before = Answer("GET", "/api/reload", 200);
    ASSERT_EQ(before["targets"].size(), 2u);
    EXPECT_EQ(before["targets"][0]["target"], "config");
    EXPECT_EQ(before["targets"][1]["target"], "messages");
    EXPECT_EQ(before["targets"][1]["ran"], false);
    EXPECT_TRUE(before["targets"][1]["finished_ms"].is_null());

    Answer("POST", "/api/reload/messages", 200);
    nlohmann::json const after = Answer("GET", "/api/reload", 200);
    EXPECT_EQ(after["targets"][1]["ran"], true);
    EXPECT_EQ(after["targets"][1]["generation"], 1u);
    EXPECT_TRUE(after["targets"][1]["finished_ms"].is_number_integer());
}

TEST_F(AdminReloadViewTest, AReloadNamesItsTargetOrIsRefused)
{
    nlohmann::json const empty = Answer("POST", "/api/reload/", 404);
    EXPECT_EQ(empty.value("error", ""), "not_found") << "a prefix route answers only a path that names something after it";
    nlohmann::json const unknown = Answer("POST", "/api/reload/nobody", 404);
    EXPECT_EQ(unknown["error"], "reload_target_unknown");
}

TEST_F(AdminReloadViewTest, AFailedTargetAnswers409WithEveryErrorAndKeepsItsGeneration)
{
    Answer("POST", "/api/reload/messages", 200);
    _broken = true;
    nlohmann::json const failed = Answer("POST", "/api/reload/messages", 409);
    EXPECT_EQ(failed["ok"], false);
    ASSERT_EQ(failed["targets"].size(), 1u);
    EXPECT_EQ(failed["targets"][0]["errors"], (std::vector<std::string>{ "the first fault", "the second fault" }));
    EXPECT_EQ(failed["targets"][0]["generation"], 1u) << "the generation that was serving goes on serving";

    nlohmann::json const all = Answer("POST", "/api/reload/all", 200);
    EXPECT_EQ(all["ok"], false) << "all says whether every target held";
    ASSERT_EQ(all["targets"].size(), 2u);
    EXPECT_EQ(all["targets"][0]["target"], "config");
    EXPECT_EQ(all["targets"][0]["ok"], true);
    EXPECT_EQ(all["targets"][1]["ok"], false);
}

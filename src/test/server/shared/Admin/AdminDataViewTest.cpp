/*
 * Project Ambrose by Imjustchico
 * Tests the /api/data/types route family: paged class lists with search and filters, class detail with properties, the install stamp on every answer, the 200-row page limit, and the budgets that refuse naming themselves.
 */

#include "AdminDataView.h"
#include "AdminRouter.h"
#include "AdminAuth.h"
#include "CharacterTypeFixtures.h"
#include "Settings.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";

    struct Fixture
    {
        AdminAuth Auth{ 10, 1.0 };
        AdminRouter Router{ Auth };
        TypeRegistry Registry;
        bool Loaded = false;

        Fixture()
        {
            Auth.SetToken(Token);
            Loaded = Registry.LoadFromText(CharacterTypeFixtures::Dump(), "data-view-test.json");
            AdminDataView::Stamp stamp;
            stamp.Revision = "r1";
            stamp.ProgramSha256 = "abc123";
            stamp.Generation = Registry.GetGeneration();
            AdminDataView::Register(Router, [stamp] { return stamp; }, Registry);
            Router.SetPermissionKnown([](std::string_view) { return true; });
        }
    };

    AdminRequest Get(std::string path)
    {
        AdminRequest request;
        request.Method = "GET";
        request.Path = std::move(path);
        request.RemoteAddress = "127.0.0.1";
        request.Authorization = std::string("Bearer ") + Token;
        return request;
    }

    std::string HeaderValue(AdminResponse const& response, std::string const& name)
    {
        for (auto const& [key, value] : response.Headers)
            if (key == name)
                return value;
        return {};
    }

    bool HasHeader(AdminResponse const& response, std::string const& name)
    {
        return !HeaderValue(response, name).empty();
    }
}

TEST(AdminDataViewTest, ListsClassesWithStamp)
{
    Fixture const fixture;
    ASSERT_TRUE(fixture.Loaded);

    AdminResponse const answer = fixture.Router.Dispatch(Get("/api/data/types"));
    EXPECT_EQ(answer.Status, 200);
    nlohmann::json const body = nlohmann::json::parse(answer.Body);
    EXPECT_EQ(body["schema"], 1);
    EXPECT_EQ(body["stamp"]["revision"], "r1");
    EXPECT_EQ(body["stamp"]["program_sha256"], "abc123");
    EXPECT_TRUE(body["stamp"].contains("generation"));
    EXPECT_EQ(HeaderValue(answer, "Cache-Control"), "no-store");
    EXPECT_FALSE(HasHeader(answer, "Content-Disposition"));
    EXPECT_GE(body["total"], 2);
    EXPECT_FALSE(body["rows"].empty());
}

TEST(AdminDataViewTest, SearchesByNameAndHash)
{
    Fixture const fixture;
    ASSERT_TRUE(fixture.Loaded);

    AdminRequest byName = Get("/api/data/types");
    byName.QueryValues["q"] = "WizardCharacterBehavior";
    AdminResponse const nameAnswer = fixture.Router.Dispatch(byName);
    EXPECT_EQ(nameAnswer.Status, 200);
    nlohmann::json const nameBody = nlohmann::json::parse(nameAnswer.Body);
    EXPECT_EQ(nameBody["total"], 1);
    EXPECT_EQ(nameBody["rows"][0]["name"], "class WizardCharacterBehavior");

    uint32 const hash = nameBody["rows"][0]["hash"].get<uint32>();
    AdminRequest byHash = Get("/api/data/types");
    byHash.QueryValues["q"] = std::to_string(hash);
    AdminResponse const hashAnswer = fixture.Router.Dispatch(byHash);
    EXPECT_EQ(hashAnswer.Status, 200);
    nlohmann::json const hashBody = nlohmann::json::parse(hashAnswer.Body);
    EXPECT_EQ(hashBody["total"], 1);
    EXPECT_EQ(hashBody["rows"][0]["name"], "class WizardCharacterBehavior");
}

TEST(AdminDataViewTest, ShowsClassDetail)
{
    Fixture const fixture;
    ASSERT_TRUE(fixture.Loaded);

    AdminResponse const answer = fixture.Router.Dispatch(Get("/api/data/types/class%20WizardCharacterBehavior"));
    EXPECT_EQ(answer.Status, 200);
    nlohmann::json const body = nlohmann::json::parse(answer.Body);
    EXPECT_EQ(body["name"], "class WizardCharacterBehavior");
    EXPECT_FALSE(body["properties"].empty());
    bool found = false;
    for (auto const& property : body["properties"])
    {
        if (property["name"] == "m_behaviorTemplateNameID")
        {
            EXPECT_EQ(property["type"], "unsigned int");
            found = true;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_EQ(body["bases"][0], "class BehaviorInstance");
    EXPECT_EQ(body["provenance"], "type_dump");
}

TEST(AdminDataViewTest, RefusesAPagePast200Rows)
{
    Fixture const fixture;
    ASSERT_TRUE(fixture.Loaded);

    AdminRequest request = Get("/api/data/types");
    request.QueryValues["limit"] = "201";
    AdminResponse const answer = fixture.Router.Dispatch(request);
    EXPECT_EQ(answer.Status, 400);
    EXPECT_NE(answer.Body.find("200"), std::string::npos);
}

TEST(AdminDataViewTest, PagesWithAnOpaqueCursor)
{
    Fixture const fixture;
    ASSERT_TRUE(fixture.Loaded);

    AdminRequest first = Get("/api/data/types");
    first.QueryValues["limit"] = "1";
    AdminResponse const firstAnswer = fixture.Router.Dispatch(first);
    EXPECT_EQ(firstAnswer.Status, 200);
    nlohmann::json const firstBody = nlohmann::json::parse(firstAnswer.Body);
    EXPECT_TRUE(firstBody["has_more"]);
    EXPECT_TRUE(firstBody.contains("cursor"));
    std::string const cursor = firstBody["cursor"].get<std::string>();
    EXPECT_FALSE(cursor.empty());
    EXPECT_EQ(cursor.find("1"), std::string::npos);

    AdminRequest second = Get("/api/data/types");
    second.QueryValues["limit"] = "1";
    second.QueryValues["cursor"] = cursor;
    AdminResponse const secondAnswer = fixture.Router.Dispatch(second);
    EXPECT_EQ(secondAnswer.Status, 200);
    nlohmann::json const secondBody = nlohmann::json::parse(secondAnswer.Body);
    EXPECT_NE(firstBody["rows"][0]["name"], secondBody["rows"][0]["name"]);
}

TEST(AdminDataViewTest, SweepFindsNoMarkerRunsAndEveryAnswerStamped)
{
    Fixture const fixture;
    ASSERT_TRUE(fixture.Loaded);

    std::string const marker(64, 'M');
    std::vector<std::string> routes{ "/api/data/types", "/api/data/types/class%20WizardCharacterBehavior" };
    for (std::string const& route : routes)
    {
        AdminResponse const answer = fixture.Router.Dispatch(Get(route));
        EXPECT_EQ(answer.Status, 200) << route;
        nlohmann::json const body = nlohmann::json::parse(answer.Body);
        EXPECT_TRUE(body.contains("stamp")) << route;
        EXPECT_TRUE(body["stamp"].contains("revision")) << route;
        EXPECT_TRUE(body["stamp"].contains("program_sha256")) << route;
        EXPECT_TRUE(body["stamp"].contains("generation")) << route;
        EXPECT_EQ(HeaderValue(answer, "Cache-Control"), "no-store") << route;
        EXPECT_FALSE(HasHeader(answer, "Content-Disposition")) << route;
        EXPECT_EQ(answer.Body.find(marker.substr(0, 32)), std::string::npos) << route << " carries a raw marker run";
    }
}

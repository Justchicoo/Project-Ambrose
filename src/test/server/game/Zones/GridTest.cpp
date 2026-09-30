/*
 * Project Ambrose by Imjustchico
 * Tests the cell grid of a zone instance: a query finds exactly the ids within its radius, height included, across cell borders and on the negative side of the origin, in ascending order; moving an id files it in its new cell and removing it takes it out; a changed cell size answers the same; a radius of zero reaches everything; and a thousand random places agree with a search of every id.
 */

#include "Grid.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <vector>

namespace
{
    std::vector<uint64> IdsOf(std::vector<GridNeighbor> const& near)
    {
        std::vector<uint64> ids;
        for (GridNeighbor const& neighbor : near)
            ids.push_back(neighbor.Id);
        return ids;
    }
}

TEST(GridTest, AQueryFindsExactlyTheIdsWithinItsRadiusAcrossCellBorders)
{
    Grid grid(100.0f);
    grid.Place(3, { 95.0f, 0.0f, 0.0f });
    grid.Place(1, { 105.0f, 0.0f, 0.0f });
    grid.Place(2, { -95.0f, -5.0f, 0.0f });
    grid.Place(4, { 0.0f, 0.0f, 150.0f });
    grid.Place(5, { 400.0f, 400.0f, 0.0f });

    EXPECT_EQ(IdsOf(grid.Near({ 0.0f, 0.0f, 0.0f }, 110.0f)), (std::vector<uint64>{ 1, 2, 3 })) << "height counts, so 4 is too far";
    EXPECT_EQ(IdsOf(grid.Near({ 0.0f, 0.0f, 0.0f }, 200.0f)), (std::vector<uint64>{ 1, 2, 3, 4 }));
    std::vector<GridNeighbor> const one = grid.Near({ 100.0f, 0.0f, 0.0f }, 5.0f);
    ASSERT_EQ(one.size(), 2u);
    EXPECT_FLOAT_EQ(one[0].DistanceSquared, 25.0f);
}

TEST(GridTest, AMovedIdIsFoundWhereItIsNowAndARemovedOneNowhere)
{
    Grid grid(50.0f);
    grid.Place(7, { 0.0f, 0.0f, 0.0f });
    grid.Place(7, { 500.0f, -500.0f, 0.0f });
    EXPECT_TRUE(grid.Near({ 0.0f, 0.0f, 0.0f }, 60.0f).empty());
    EXPECT_EQ(IdsOf(grid.Near({ 510.0f, -500.0f, 0.0f }, 20.0f)), (std::vector<uint64>{ 7 }));
    EXPECT_EQ(grid.Size(), 1u);
    EXPECT_TRUE(grid.Remove(7));
    EXPECT_FALSE(grid.Remove(7));
    EXPECT_FALSE(grid.Contains(7));
    EXPECT_TRUE(grid.Near({ 510.0f, -500.0f, 0.0f }, 20.0f).empty());
}

TEST(GridTest, AChangedCellSizeAndARadiusOfZeroAnswerAsExpected)
{
    Grid grid(10.0f);
    for (uint64 id = 1; id <= 20; ++id)
        grid.Place(id, { static_cast<float>(id) * 37.0f, static_cast<float>(id) * -11.0f, 0.0f });
    std::vector<uint64> const before = IdsOf(grid.Near({ 300.0f, -90.0f, 0.0f }, 120.0f));
    grid.SetCellSize(1000.0f);
    EXPECT_EQ(IdsOf(grid.Near({ 300.0f, -90.0f, 0.0f }, 120.0f)), before);
    grid.SetCellSize(0.0f);
    EXPECT_EQ(grid.GetCellSize(), Grid::MinimumCellSize);
    EXPECT_EQ(IdsOf(grid.Near({ 300.0f, -90.0f, 0.0f }, 120.0f)), before);
    EXPECT_EQ(grid.Near({ 0.0f, 0.0f, 0.0f }, 0.0f).size(), 20u) << "a radius of zero reaches everything";
}

TEST(GridTest, AThousandRandomPlacesAgreeWithASearchOfEveryId)
{
    std::mt19937 random(20260930);
    std::uniform_real_distribution<float> coordinate(-2000.0f, 2000.0f);
    Grid grid(128.0f);
    std::vector<GridPoint> points(1000);
    for (uint64 id = 0; id < points.size(); ++id)
    {
        points[id] = { coordinate(random), coordinate(random), coordinate(random) / 10.0f };
        grid.Place(id, points[id]);
    }
    for (int query = 0; query < 50; ++query)
    {
        GridPoint const at{ coordinate(random), coordinate(random), 0.0f };
        float const radius = 50.0f + static_cast<float>(query) * 20.0f;
        std::vector<uint64> expected;
        for (uint64 id = 0; id < points.size(); ++id)
        {
            float const dx = points[id].X - at.X;
            float const dy = points[id].Y - at.Y;
            float const dz = points[id].Z - at.Z;
            if (dx * dx + dy * dy + dz * dz <= radius * radius)
                expected.push_back(id);
        }
        EXPECT_EQ(IdsOf(grid.Near(at, radius)), expected) << "query " << query;
    }
}

/*
 * Project Ambrose by Imjustchico
 * Tests where a duel puts each participant: on an unturned sigil a circle's angle lands it on the ring's axes facing the centre, and on a turned and moved sigil the four monster and four player circles of an eight-circle sigil land where the client puts them, at the sigil's height, every one facing the centre.
 */

#include "SubCircle.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

namespace
{
    struct Expected
    {
        float Rotation;
        float X;
        float Y;
        float Yaw;
    };

    void ExpectPlaced(SubCirclePlacement const& placement, Expected const& expected, float z)
    {
        EXPECT_NEAR(placement.Position.X, expected.X, 0.01f) << expected.Rotation;
        EXPECT_NEAR(placement.Position.Y, expected.Y, 0.01f) << expected.Rotation;
        EXPECT_FLOAT_EQ(placement.Position.Z, z) << expected.Rotation;
        EXPECT_NEAR(placement.Yaw, expected.Yaw, 0.0001f) << expected.Rotation;
    }
}

TEST(SubCircleTest, AnUnturnedSigilPutsEachCircleOnItsAxisFacingTheCentre)
{
    PropertyTypes::Vector3D const centre{0.0f, 0.0f, 0.0f};
    constexpr float pi = std::numbers::pi_v<float>;
    Expected const circles[] = {
        {90.0f, 0.0f, 600.0f, 0.0f},
        {-90.0f, 0.0f, -600.0f, pi},
        {0.0f, 600.0f, 0.0f, pi / 2.0f},
        {180.0f, -600.0f, 0.0f, 3.0f * pi / 2.0f},
    };
    for (Expected const& circle : circles)
        ExpectPlaced(SubCircle::Place(centre, 0.0f, circle.Rotation, 600.0f), circle, 0.0f);
}

TEST(SubCircleTest, ATurnedSigilPutsItsMonsterAndPlayerCirclesWhereTheClientDoes)
{
    PropertyTypes::Vector3D const centre{1000.0f, -2000.0f, 50.0f};
    Expected const circles[] = {
        {144.0f, 743.092f, -1457.784f, 5.84071f},
        {108.0f, 1110.864f, -1410.331f, 0.18584f},
        {72.0f, 1436.289f, -1588.112f, 0.81416f},
        {36.0f, 1595.067f, -1923.220f, 1.44248f},
        {-36.0f, 1256.908f, -2542.216f, 2.69911f},
        {-72.0f, 889.136f, -2589.669f, 3.32743f},
        {-108.0f, 563.711f, -2411.888f, 3.95575f},
        {-144.0f, 404.933f, -2076.780f, 4.58407f},
    };
    for (Expected const& circle : circles)
    {
        SubCirclePlacement const placement = SubCircle::Place(centre, 0.5f, circle.Rotation, 600.0f);
        ExpectPlaced(placement, circle, 50.0f);
        float const toCentreX = centre.X - placement.Position.X;
        float const toCentreY = centre.Y - placement.Position.Y;
        EXPECT_NEAR(std::hypot(toCentreX, toCentreY), 600.0f, 0.01f);
    }
}

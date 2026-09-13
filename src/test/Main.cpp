/*
 * Project Ambrose by Imjustchico
 * Unit test entry point that initializes GoogleTest and GoogleMock and runs every test.
 */

#include <gmock/gmock.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}

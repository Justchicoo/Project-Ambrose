/*
 * Project Ambrose by Imjustchico
 * Tests the space guard over a volume a test describes: a write that would leave less than the minimum is refused naming the volume with its free, reserved, minimum and requested figures, before the jail makes the file, so nothing reaches the disk and the ledger is as it was; a write that fits is made and its reservation settled; what other writers hold counts against the minimum until they finish; the larger of the byte and percent minimums holds; and the machine's own volumes can be read.
 */

#include "FileJail.h"
#include "LogTestDirectory.h"
#include "SpaceGuard.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>

namespace
{
    constexpr uint64 MiB = 1024ull * 1024;
    constexpr uint64 GiB = 1024ull * MiB;

    class FixedProbe final : public Ambrose::SpaceProbe
    {
    public:
        FixedProbe(uint64 free, uint64 total) : _free(free), _total(total) {}

        std::optional<Ambrose::VolumeSpace> Probe(std::filesystem::path const&, std::string&) const override
        {
            Ambrose::VolumeSpace space;
            space.Id = 7;
            space.Name = "/srv/ambrose";
            space.Free = _free;
            space.Total = _total;
            return space;
        }

    private:
        uint64 _free = 0;
        uint64 _total = 0;
    };

    Ambrose::JailPath PathOf(std::string_view text)
    {
        Ambrose::JailPathResult const parsed = Ambrose::JailPaths::ParseText(text);
        EXPECT_TRUE(parsed.Ok()) << parsed.Reason;
        return parsed.Path;
    }
}

TEST(SpaceGuardTest, AWriteThatWouldLeaveLessThanTheMinimumIsRefusedWithTheVolumeAndFigureBeforeAnyByte)
{
    LogTestDirectory directory;
    std::string error;
    std::shared_ptr<Ambrose::JailRoot> const root = Ambrose::JailRoot::Open(directory.Path(), error);
    ASSERT_NE(root, nullptr) << error;
    Ambrose::SpaceGuard guard(std::make_shared<FixedProbe>(GiB + 10 * MiB, 100 * GiB));
    guard.SetMinimum(GiB, 0);

    std::string const contents(1024, 'x');
    Ambrose::JailError refusedError;
    Ambrose::SpaceRefusal refusal;
    std::string const big(static_cast<std::size_t>(11 * MiB), 'b');
    EXPECT_FALSE(Ambrose::FileJail::Create(*root, PathOf("big.bin"), big, guard, refusedError, &refusal));
    EXPECT_EQ(refusedError.Failure, Ambrose::JailFailure::NoSpace);
    EXPECT_EQ(refusal.Volume, "/srv/ambrose");
    EXPECT_EQ(refusal.Free, GiB + 10 * MiB);
    EXPECT_EQ(refusal.Minimum, GiB);
    EXPECT_EQ(refusal.Requested, 11 * MiB);
    EXPECT_NE(refusedError.Message.find("/srv/ambrose"), std::string::npos) << refusedError.Message;
    EXPECT_NE(refusedError.Message.find("1073741824"), std::string::npos) << "the minimum is named in bytes: " << refusedError.Message;
    EXPECT_FALSE(std::filesystem::exists(directory.Path() / "big.bin")) << "the file is not made when its space is refused";
    EXPECT_EQ(guard.ReservedOn(7), 0u) << "a refused reservation leaves the ledger as it was";

    Ambrose::JailError madeError;
    ASSERT_TRUE(Ambrose::FileJail::Create(*root, PathOf("small.txt"), contents, guard, madeError)) << madeError.Message;
    std::ifstream stream(directory.Path() / "small.txt", std::ios::binary);
    std::string const written((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(written, contents);
    EXPECT_EQ(guard.ReservedOn(7), 0u) << "a finished write settles its reservation";

    Ambrose::JailError again;
    EXPECT_FALSE(Ambrose::FileJail::Create(*root, PathOf("small.txt"), contents, guard, again));
    EXPECT_EQ(again.Failure, Ambrose::JailFailure::Conflict) << "a new file is made exclusively and never replaces one";
}

TEST(SpaceGuardTest, ReservationsHeldByOtherWritersCountAgainstTheMinimum)
{
    Ambrose::SpaceGuard guard(std::make_shared<FixedProbe>(GiB + 100 * MiB, 100 * GiB));
    guard.SetMinimum(GiB, 0);
    Ambrose::SpaceRefusal refusal;
    std::string error;
    std::optional<Ambrose::SpaceReservation> first = guard.Reserve("/srv/ambrose/logs", 60 * MiB, refusal, error);
    ASSERT_TRUE(first.has_value()) << error;
    EXPECT_EQ(guard.ReservedOn(7), 60 * MiB);

    std::optional<Ambrose::SpaceReservation> const second = guard.Reserve("/srv/ambrose/data", 60 * MiB, refusal, error);
    EXPECT_FALSE(second.has_value()) << "the first writer's 60 MiB leaves only 40 MiB above the minimum";
    EXPECT_EQ(refusal.Reserved, 60 * MiB);
    EXPECT_NE(error.find("reserved"), std::string::npos) << error;

    first->Release();
    EXPECT_EQ(guard.ReservedOn(7), 0u);
    std::optional<Ambrose::SpaceReservation> third = guard.Reserve("/srv/ambrose/data", 60 * MiB, refusal, error);
    ASSERT_TRUE(third.has_value()) << "space the first writer gave back is free again";
    {
        std::optional<Ambrose::SpaceReservation> moved = std::move(third);
        EXPECT_EQ(guard.ReservedOn(7), 60 * MiB);
    }
    EXPECT_EQ(guard.ReservedOn(7), 0u) << "a reservation dropped without settling gives its bytes back once";
}

TEST(SpaceGuardTest, TheLargerOfTheByteAndPercentMinimumsHolds)
{
    Ambrose::SpaceGuard guard(std::make_shared<FixedProbe>(50 * GiB, 100 * GiB));
    guard.SetMinimum(GiB, 5);
    EXPECT_EQ(guard.MinimumFor(100 * GiB), 5 * GiB) << "5% of 100 GiB is more than 1 GiB";
    EXPECT_EQ(guard.MinimumFor(10 * GiB), GiB) << "1 GiB is more than 5% of 10 GiB";
    guard.SetMinimum(0, 0);
    EXPECT_EQ(guard.MinimumFor(10 * GiB), 0u);

    guard.SetMinimum(GiB, 60);
    Ambrose::SpaceRefusal refusal;
    std::string error;
    EXPECT_FALSE(guard.Reserve("/srv/ambrose", 1, refusal, error).has_value()) << "60% of the volume must stay free and only 50% is";
    EXPECT_EQ(refusal.Minimum, 60 * GiB);

    std::optional<Ambrose::VolumeFigures> const figures = guard.Figures("/srv/ambrose", error);
    ASSERT_TRUE(figures.has_value()) << error;
    EXPECT_EQ(figures->Space.Name, "/srv/ambrose");
    EXPECT_EQ(figures->Minimum, 60 * GiB);
}

TEST(SpaceGuardTest, ReadsTheMachinesOwnVolumes)
{
    LogTestDirectory directory;
    Ambrose::SystemSpaceProbe const probe;
    std::string error;
    std::optional<Ambrose::VolumeSpace> const space = probe.Probe(directory.Path() / "not-yet-made" / "file", error);
    ASSERT_TRUE(space.has_value()) << error;
    EXPECT_GT(space->Total, 0u);
    EXPECT_LE(space->Free, space->Total);
    EXPECT_FALSE(space->Name.empty());
}

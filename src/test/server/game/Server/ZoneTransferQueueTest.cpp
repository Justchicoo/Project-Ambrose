/*
 * Project Ambrose by Imjustchico
 * Checks the zone transfer queue: a second request while one waits on the client is ignored and the first stays, a MSG_ZONETRANSFERNACK clears it so a new one is taken, an acknowledgement hands the waiting transfer over exactly once, and while one is being carried out no other is taken until it finishes.
 */

#include "ZoneTransferQueue.h"

#include <gtest/gtest.h>

namespace
{
    ZoneTransfer To(std::string zone)
    {
        return ZoneTransfer{ zone, zone, "Start", PlayerPosition{ 1.0f, 2.0f, 3.0f, 0.5f } };
    }
}

TEST(ZoneTransferQueueTest, ASecondRequestWhileOneWaitsIsIgnoredAndANackClearsIt)
{
    ZoneTransferQueue queue;
    EXPECT_TRUE(queue.Request(To("WizardCity/WC_Ravenwood")));
    EXPECT_FALSE(queue.Request(To("WizardCity/WC_Hub")));
    ASSERT_TRUE(queue.Waiting());
    EXPECT_EQ(queue.Waiting()->Zone, "WizardCity/WC_Ravenwood");

    EXPECT_TRUE(queue.Nack());
    EXPECT_FALSE(queue.Busy());
    EXPECT_FALSE(queue.Nack()) << "a refusal with nothing waiting changes nothing";
    EXPECT_TRUE(queue.Request(To("WizardCity/WC_Hub")));
}

TEST(ZoneTransferQueueTest, AnAcknowledgementHandsTheTransferOverOnceAndHoldsTheQueueUntilItFinishes)
{
    ZoneTransferQueue queue;
    EXPECT_FALSE(queue.Ack()) << "an acknowledgement nobody asked for carries nothing out";
    ASSERT_TRUE(queue.Request(To("WizardCity/WC_Ravenwood")));
    std::optional<ZoneTransfer> const carried = queue.Ack();
    ASSERT_TRUE(carried);
    EXPECT_EQ(carried->Zone, "WizardCity/WC_Ravenwood");
    EXPECT_FLOAT_EQ(carried->Place.Z, 3.0f);
    EXPECT_FALSE(queue.Ack());
    EXPECT_TRUE(queue.Busy());
    EXPECT_FALSE(queue.Request(To("WizardCity/WC_Hub"))) << "no transfer is taken while one is being carried out";
    queue.Finish();
    EXPECT_TRUE(queue.Request(To("WizardCity/WC_Hub")));
}

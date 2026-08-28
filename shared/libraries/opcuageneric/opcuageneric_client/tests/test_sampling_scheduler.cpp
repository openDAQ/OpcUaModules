#include <gtest/gtest.h>
#include <opcuageneric_client/sampling_scheduler.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

using namespace daq::opcua::generic;
using namespace std::chrono_literals;

namespace
{
    using Clock = SamplingScheduler::Clock;

    class FakeItem : public ISampledItem
    {
    public:
        explicit FakeItem(uint32_t intervalMs)
            : interval(intervalMs)
        {
        }

        uint32_t getSamplingInterval() const override
        {
            return interval.load();
        }

        void setSamplingInterval(uint32_t intervalMs)
        {
            interval = intervalMs;
        }

        void processSample() override
        {
            const auto delay = sampleDelay.load();
            {
                std::scoped_lock lock(mutex);
                sampleTimes.push_back(Clock::now());
            }
            cv.notify_all();

            if (delay.count() > 0)
            {
                sampleDelay = 0ms;
                std::this_thread::sleep_for(delay);
            }
        }

        void onConnectionRestored() override
        {
            revalidations++;
            cv.notify_all();
        }

        size_t sampleCount() const
        {
            std::scoped_lock lock(mutex);
            return sampleTimes.size();
        }

        std::vector<Clock::time_point> takeSampleTimes() const
        {
            std::scoped_lock lock(mutex);
            return sampleTimes;
        }

        // Blocks until at least count samples were taken, or the timeout elapses.
        bool waitForSamples(size_t count, std::chrono::milliseconds timeout)
        {
            std::unique_lock lock(mutex);
            return cv.wait_for(lock, timeout, [&] { return sampleTimes.size() >= count; });
        }

        std::atomic<std::chrono::milliseconds> sampleDelay{0ms};
        std::atomic<size_t> revalidations{0};

    private:
        std::atomic<uint32_t> interval;
        mutable std::mutex mutex;
        std::condition_variable cv;
        std::vector<Clock::time_point> sampleTimes;
    };

    // Counts samples that followed the previous one almost immediately, i.e. catch-up reads.
    size_t countBackToBack(const std::vector<Clock::time_point>& times, std::chrono::milliseconds interval)
    {
        size_t count = 0;
        for (size_t i = 1; i < times.size(); ++i)
        {
            if (times[i] - times[i - 1] < interval / 2)
                count++;
        }
        return count;
    }

}

TEST(SamplingSchedulerTest, AdvanceDeadlineNotLate)
{
    const auto now = Clock::now();
    const auto due = now - 10ms;  // due right now, no backlog

    EXPECT_EQ(SamplingScheduler::advanceDeadline(due, now, 100ms), due + 100ms);
}

TEST(SamplingSchedulerTest, AdvanceDeadlineLateByLessThanOneInterval)
{
    const auto now = Clock::now();
    const auto due = now - 60ms;  // one interval of debt: the shifted deadline is still in the future

    EXPECT_EQ(SamplingScheduler::advanceDeadline(due, now, 100ms), due + 100ms);
}

TEST(SamplingSchedulerTest, AdvanceDeadlineDropsBacklog)
{
    const auto now = Clock::now();
    const auto interval = 100ms;

    // Whatever the depth of the backlog, the deadline is re-anchored to exactly one interval ahead.
    for (const int intervalsLate : {2, 3, 10, 3000})
    {
        const auto due = now - interval * intervalsLate;
        EXPECT_EQ(SamplingScheduler::advanceDeadline(due, now, interval), now + interval) << "late by " << intervalsLate << " intervals";
    }
}

TEST(SamplingSchedulerTest, AdvanceDeadlinePostcondition)
{
    const auto now = Clock::now();
    const auto interval = 100ms;

    for (const int intervalsLate : {0, 1, 2, 10, 3000})
    {
        const auto due = now - interval * intervalsLate;
        const auto next = SamplingScheduler::advanceDeadline(due, now, interval);

        EXPECT_GT(next, now) << "late by " << intervalsLate << " intervals";             // there is always a wait
        EXPECT_LE(next, now + interval) << "late by " << intervalsLate << " intervals";  // and it never exceeds one interval
    }
}

TEST(SamplingSchedulerTest, AdvanceDeadlineGuardsAgainstZeroInterval)
{
    const auto now = Clock::now();
    EXPECT_GT(SamplingScheduler::advanceDeadline(now, now, 0ms), now);
}

TEST(SamplingSchedulerTest, SamplesItemUntilStopped)
{
    FakeItem item(20);
    SamplingScheduler scheduler(nullptr);
    scheduler.start();
    scheduler.registerItem(&item);

    ASSERT_TRUE(item.waitForSamples(3, 1s));

    scheduler.stop();
    const auto afterStop = item.sampleCount();
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(item.sampleCount(), afterStop);
}

TEST(SamplingSchedulerTest, IndependentIntervalsAreHonoured)
{
    FakeItem fast(20);
    FakeItem medium(50);
    FakeItem slow(200);

    SamplingScheduler scheduler(nullptr);
    scheduler.start();
    scheduler.registerItem(&fast);
    scheduler.registerItem(&medium);
    scheduler.registerItem(&slow);

    std::this_thread::sleep_for(1s);
    scheduler.stop();

    // Generous bounds: the point is that every item keeps its own rate, not that timing is exact.
    EXPECT_GE(fast.sampleCount(), 35u);
    EXPECT_LE(fast.sampleCount(), 65u);
    EXPECT_GE(medium.sampleCount(), 14u);
    EXPECT_LE(medium.sampleCount(), 26u);
    EXPECT_GE(slow.sampleCount(), 3u);
    EXPECT_LE(slow.sampleCount(), 8u);
}

TEST(SamplingSchedulerTest, SingleSlowSampleCatchesUpByOneTickOnly)
{
    FakeItem item(50);
    SamplingScheduler scheduler(nullptr);
    scheduler.start();
    scheduler.registerItem(&item);

    ASSERT_TRUE(item.waitForSamples(1, 1s));
    item.sampleDelay = 200ms;  // stalls for four intervals, consumed by the next sample

    std::this_thread::sleep_for(1s);
    scheduler.stop();

    // The stall costs at most one catch-up read; without the backlog drop there would be four.
    EXPECT_LE(countBackToBack(item.takeSampleTimes(), 50ms), 1u);
}

TEST(SamplingSchedulerTest, ChangedIntervalTakesEffect)
{
    FakeItem item(200);
    SamplingScheduler scheduler(nullptr);
    scheduler.start();
    scheduler.registerItem(&item);

    ASSERT_TRUE(item.waitForSamples(1, 1s));
    item.setSamplingInterval(20);

    // The new interval applies from the next deadline shift, so within one old interval.
    EXPECT_TRUE(item.waitForSamples(10, 1s));
}

TEST(SamplingSchedulerTest, UnregisterWaitsForSampleInProgress)
{
    FakeItem item(20);
    SamplingScheduler scheduler(nullptr);
    scheduler.start();
    scheduler.registerItem(&item);

    ASSERT_TRUE(item.waitForSamples(1, 1s));
    item.sampleDelay = 300ms;
    ASSERT_TRUE(item.waitForSamples(2, 1s));  // the sample that sleeps has just started

    const auto start = Clock::now();
    scheduler.unregisterItem(&item);
    const auto elapsed = Clock::now() - start;

    // unregisterItem must not return while the item is still being sampled.
    EXPECT_GE(elapsed, 100ms);

    const auto afterUnregister = item.sampleCount();
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(item.sampleCount(), afterUnregister);

    scheduler.stop();
}

TEST(SamplingSchedulerTest, DoesNotSampleWhileDisconnected)
{
    std::atomic<bool> connected{false};
    FakeItem item(20);

    SamplingScheduler scheduler([&] { return connected.load(); });
    scheduler.start();
    scheduler.registerItem(&item);

    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(item.sampleCount(), 0u);

    connected = true;
    scheduler.onReconnected();  // also wakes the loop out of its disconnected wait

    EXPECT_TRUE(item.waitForSamples(3, 300ms));
    scheduler.stop();
}

TEST(SamplingSchedulerTest, ResumingAfterPauseDoesNotBurst)
{
    std::atomic<bool> connected{true};
    FakeItem item(20);

    SamplingScheduler scheduler([&] { return connected.load(); });
    scheduler.start();
    scheduler.registerItem(&item);

    ASSERT_TRUE(item.waitForSamples(2, 1s));
    connected = false;
    std::this_thread::sleep_for(600ms);  // ~30 missed ticks

    const auto beforeResume = item.sampleCount();
    connected = true;
    scheduler.onReconnected();

    ASSERT_TRUE(item.waitForSamples(beforeResume + 3, 2s));
    std::this_thread::sleep_for(200ms);
    scheduler.stop();

    // The backlog is dropped, so the resumed item samples at its normal rate instead of firing ~30
    // reads back to back.
    const auto times = item.takeSampleTimes();
    EXPECT_LE(countBackToBack(times, 20ms), 1u);
}

TEST(SamplingSchedulerTest, OnReconnectedRevalidatesEveryItem)
{
    std::atomic<bool> connected{true};
    FakeItem first(50);
    FakeItem second(50);

    SamplingScheduler scheduler([&] { return connected.load(); });
    scheduler.start();
    scheduler.registerItem(&first);
    scheduler.registerItem(&second);

    ASSERT_TRUE(first.waitForSamples(1, 1s));
    scheduler.onReconnected();

    for (int i = 0; i < 200 && (first.revalidations == 0 || second.revalidations == 0); ++i)
        std::this_thread::sleep_for(10ms);

    EXPECT_EQ(first.revalidations.load(), 1u);
    EXPECT_EQ(second.revalidations.load(), 1u);

    scheduler.stop();
}

TEST(SamplingSchedulerTest, StopIsIdempotentAndSafeWithoutStart)
{
    SamplingScheduler scheduler(nullptr);
    EXPECT_NO_THROW(scheduler.stop());

    scheduler.start();
    EXPECT_NO_THROW(scheduler.stop());
    EXPECT_NO_THROW(scheduler.stop());
}

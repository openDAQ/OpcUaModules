#include <gtest/gtest.h>
#include <opcuageneric_client/sampling_scheduler.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <limits>
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
                slowSampleReturnedAt = Clock::now();
            }
        }

        void onConnectionRestored() override
        {
            {
                std::scoped_lock lock(mutex);
                revalidations++;
            }
            cv.notify_all();
        }

        void onSchedulerDestroyed() override
        {
            schedulerDestroyedCalls++;
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

        // Blocks until the item was revalidated at least count times, or the timeout elapses.
        bool waitForRevalidations(size_t count, std::chrono::milliseconds timeout)
        {
            std::unique_lock lock(mutex);
            return cv.wait_for(lock, timeout, [&] { return revalidations.load() >= count; });
        }

        std::atomic<std::chrono::milliseconds> sampleDelay{0ms};
        // Set when a delayed sample returned: the catch-up window after the stall starts there.
        std::atomic<Clock::time_point> slowSampleReturnedAt{Clock::time_point{}};
        std::atomic<size_t> revalidations{0};
        std::atomic<size_t> schedulerDestroyedCalls{0};

    private:
        std::atomic<uint32_t> interval;
        mutable std::mutex mutex;
        std::condition_variable cv;
        std::vector<Clock::time_point> sampleTimes;
    };

    // Median gap between consecutive samples, ignoring the first `from` samples. A slow runner can
    // only stretch gaps, and the median absorbs the odd stalled wakeup, so bounding it from below
    // holds however loaded the machine is.
    int64_t medianGapMs(const std::vector<Clock::time_point>& times, size_t from = 0)
    {
        std::vector<int64_t> gaps;
        for (size_t i = std::max<size_t>(from, 1); i < times.size(); ++i)
            gaps.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(times[i] - times[i - 1]).count());

        if (gaps.empty())
            return std::numeric_limits<int64_t>::max();

        std::sort(gaps.begin(), gaps.end());
        return gaps[gaps.size() / 2];
    }

    // Parks the first caller of pass() until open() is called.
    class Gate
    {
    public:
        void pass()
        {
            if (used.exchange(true))
                return;

            entered.set_value();
            openedFuture.wait();
        }

        bool waitUntilEntered(std::chrono::milliseconds timeout)
        {
            return entered.get_future().wait_for(timeout) == std::future_status::ready;
        }

        void open()
        {
            if (!openRequested.exchange(true))
                opened.set_value();
        }

    private:
        std::atomic<bool> used{false};
        std::atomic<bool> openRequested{false};
        std::promise<void> entered;
        std::promise<void> opened;
        std::shared_future<void> openedFuture{opened.get_future().share()};
    };

    DECLARE_OPENDAQ_INTERFACE(IOwnedItem, daq::IBaseObject){};

    // An item that is an openDAQ object, so that the scheduler can be given an owner to hold on to.
    // Whatever a test wants to learn from it goes through the hooks, as the item itself may be gone by
    // the time the test looks.
    class OwnedItem : public daq::ImplementationOfWeak<IOwnedItem>, public ISampledItem
    {
    public:
        struct Hooks
        {
            std::function<void()> onSample;
            std::function<void()> onRevalidation;
            // Runs first thing in the destructor, before the item unregisters.
            std::function<void()> onDestroy;
        };

        OwnedItem(SamplingScheduler& scheduler, Hooks hooks)
            : scheduler(&scheduler)
            , hooks(std::move(hooks))
        {
        }

        ~OwnedItem() override
        {
            if (hooks.onDestroy)
                hooks.onDestroy();

            if (auto* sched = scheduler.exchange(nullptr); sched != nullptr)
                sched->unregisterItem(this);
        }

        uint32_t getSamplingInterval() const override
        {
            return 5;
        }

        void processSample() override
        {
            if (hooks.onSample)
                hooks.onSample();
        }

        void onConnectionRestored() override
        {
            if (hooks.onRevalidation)
                hooks.onRevalidation();
        }

        void onSchedulerDestroyed() override
        {
            scheduler = nullptr;
        }

    private:
        std::atomic<SamplingScheduler*> scheduler;
        Hooks hooks;
    };

    struct OwnedItemHandle
    {
        // The only reference to the item apart from those the scheduler takes for the duration of a call.
        daq::ObjectPtr<IOwnedItem> owner;
        OwnedItem* item;
    };

    OwnedItemHandle registerOwnedItem(SamplingScheduler& scheduler, OwnedItem::Hooks hooks)
    {
        OwnedItemHandle handle;
        handle.owner = daq::createWithImplementation<IOwnedItem, OwnedItem>(scheduler, std::move(hooks));
        handle.item = static_cast<OwnedItem*>(handle.owner.getObject());
        scheduler.registerItem(handle.item, handle.owner);
        return handle;
    }

    // Samples taken in [from, from + window). Bounding this from above is safe on any runner: being
    // slow can only move samples out of the window, never into it.
    size_t countWithin(const std::vector<Clock::time_point>& times, Clock::time_point from, std::chrono::milliseconds window)
    {
        return static_cast<size_t>(
            std::count_if(times.begin(), times.end(), [&](Clock::time_point t) { return t >= from && t < from + window; }));
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
    FakeItem slow(500);

    SamplingScheduler scheduler(nullptr);
    scheduler.start();
    scheduler.registerItem(&fast);
    scheduler.registerItem(&medium);
    scheduler.registerItem(&slow);

    // The slow item sets how long the run has to be; waiting for it beats a fixed window, which would
    // assume the runner keeps up with the fast one.
    ASSERT_TRUE(slow.waitForSamples(4, 10s));
    scheduler.stop();

    // Every item stays on its own deadline: none of them is sampled faster than its own interval. If
    // the items shared one deadline, the slow ones would run at the fast item's rate.
    EXPECT_GE(medianGapMs(fast.takeSampleTimes()), 15);
    EXPECT_GE(medianGapMs(medium.takeSampleTimes()), 40);
    EXPECT_GE(medianGapMs(slow.takeSampleTimes()), 400);

    // And the fast item really does get more turns, whatever pace the runner manages overall.
    EXPECT_GT(fast.sampleCount(), slow.sampleCount() + 2);
}

TEST(SamplingSchedulerTest, SingleSlowSampleCatchesUpByOneTickOnly)
{
    FakeItem item(50);
    SamplingScheduler scheduler(nullptr);
    scheduler.start();
    scheduler.registerItem(&item);

    ASSERT_TRUE(item.waitForSamples(1, 1s));
    item.sampleDelay = 200ms;  // stalls for four intervals, consumed by the next sample

    ASSERT_TRUE(item.waitForSamples(4, 10s));
    scheduler.stop();

    // The stall costs at most one catch-up read. Without the backlog drop the four missed reads would
    // all fire the moment the slow sample returns, i.e. within microseconds of that catch-up read, so
    // measuring the window from the read itself makes the bound independent of the runner's speed.
    const auto times = item.takeSampleTimes();
    const auto caughtUp =
        std::find_if(times.begin(), times.end(), [&](Clock::time_point t) { return t >= item.slowSampleReturnedAt.load(); });
    ASSERT_NE(caughtUp, times.end());
    EXPECT_EQ(countWithin(times, *caughtUp, 25ms), 1u);
}

TEST(SamplingSchedulerTest, ChangedIntervalTakesEffect)
{
    constexpr auto slowInterval = 500ms;

    FakeItem item(static_cast<uint32_t>(slowInterval.count()));
    SamplingScheduler scheduler(nullptr);
    scheduler.start();
    scheduler.registerItem(&item);

    ASSERT_TRUE(item.waitForSamples(1, 1s));
    const auto beforeChange = item.sampleCount();
    item.setSamplingInterval(20);

    // The timeout only has to be generous; it is the gaps that carry the assertion.
    ASSERT_TRUE(item.waitForSamples(beforeChange + 6, 10s));
    scheduler.stop();

    // The new interval applies from the next deadline shift, so the first gap still carries the
    // pending old deadline and is skipped. From there the gaps must sit well below the old interval -
    // a scheduler that ignored the change would still be a full 500 ms apart. This compares the runner
    // with itself instead of assuming it achieves the requested 20 ms.
    EXPECT_LT(medianGapMs(item.takeSampleTimes(), beforeChange + 1), slowInterval.count() / 2);
}

TEST(SamplingSchedulerTest, UnregisterDoesNotWaitForSampleInProgress)
{
    constexpr auto patience = 5s;
    Gate sample;
    std::atomic<size_t> samples{0};
    FakeItem witness(20);
    SamplingScheduler scheduler(nullptr);

    OwnedItem::Hooks hooks;
    hooks.onSample = [&]
    {
        samples++;
        sample.pass();
    };
    const auto handle = registerOwnedItem(scheduler, hooks);
    scheduler.start();
    ASSERT_TRUE(sample.waitUntilEntered(patience));

    auto unregistered = std::async(std::launch::async, [&] { scheduler.unregisterItem(handle.item); });
    EXPECT_EQ(unregistered.wait_for(patience), std::future_status::ready) << "unregisterItem() waits for the sample in progress";

    sample.open();
    unregistered.get();

    // The call that was in progress is the last one. The scheduler serves the earliest deadline first,
    // and the witness has a longer interval than the item: were the item still registered, it would
    // have been sampled again before the second sample of the witness.
    scheduler.registerItem(&witness);
    ASSERT_TRUE(witness.waitForSamples(2, patience));
    EXPECT_EQ(samples.load(), 1u);

    scheduler.stop();
}

TEST(SamplingSchedulerTest, OwnerIsKeptAliveDuringSampleInProgress)
{
    constexpr auto patience = 5s;
    Gate sample;
    std::promise<void> destroyed;
    auto destroyedFuture = destroyed.get_future();
    FakeItem other(20);
    SamplingScheduler scheduler(nullptr);

    OwnedItem::Hooks hooks;
    hooks.onSample = [&] { sample.pass(); };
    hooks.onDestroy = [&] { destroyed.set_value(); };
    auto handle = registerOwnedItem(scheduler, hooks);
    scheduler.start();
    ASSERT_TRUE(sample.waitUntilEntered(patience));

    // Without the reference of the scheduler the item would be destroyed right here, under the call.
    handle.owner.release();
    EXPECT_EQ(destroyedFuture.wait_for(0ms), std::future_status::timeout);

    sample.open();
    ASSERT_EQ(destroyedFuture.wait_for(patience), std::future_status::ready);

    // The item went away on the scheduler thread and unregistered itself from there. The scheduler has
    // to be in working order after that.
    scheduler.registerItem(&other);
    EXPECT_TRUE(other.waitForSamples(1, patience));

    scheduler.stop();
}

TEST(SamplingSchedulerTest, OwnerIsKeptAliveDuringRevalidationInProgress)
{
    constexpr auto patience = 5s;
    Gate revalidation;
    std::promise<void> destroyed;
    auto destroyedFuture = destroyed.get_future();
    FakeItem other(20);
    SamplingScheduler scheduler(nullptr);

    OwnedItem::Hooks hooks;
    hooks.onRevalidation = [&] { revalidation.pass(); };
    hooks.onDestroy = [&] { destroyed.set_value(); };
    auto handle = registerOwnedItem(scheduler, hooks);
    scheduler.start();
    scheduler.onReconnected();
    ASSERT_TRUE(revalidation.waitUntilEntered(patience));

    handle.owner.release();
    EXPECT_EQ(destroyedFuture.wait_for(0ms), std::future_status::timeout);

    revalidation.open();
    ASSERT_EQ(destroyedFuture.wait_for(patience), std::future_status::ready);

    scheduler.registerItem(&other);
    EXPECT_TRUE(other.waitForSamples(1, patience));

    scheduler.stop();
}

TEST(SamplingSchedulerTest, ItemWhoseOwnerIsBeingDestroyedIsNotCalled)
{
    constexpr auto patience = 5s;
    Gate destructor;
    std::atomic<size_t> calls{0};
    FakeItem witness(20);
    SamplingScheduler scheduler(nullptr);

    OwnedItem::Hooks hooks;
    hooks.onSample = [&] { calls++; };
    hooks.onRevalidation = [&] { calls++; };
    hooks.onDestroy = [&] { destructor.pass(); };
    auto handle = registerOwnedItem(scheduler, hooks);
    scheduler.registerItem(&witness);

    // The destructor is held back before it unregisters the item: the scheduler still has the item in
    // its list, but there is nothing left that it could hold on to.
    auto released = std::async(std::launch::async, [&] { handle.owner.release(); });
    ASSERT_TRUE(destructor.waitUntilEntered(patience));

    // Requested before the start, so that the revalidation is the first thing the scheduler does and
    // finds the item still in the list. The witness was registered after the item and comes after it
    // in both passes: once it has been revalidated and sampled, the scheduler has been past the item.
    scheduler.onReconnected();
    scheduler.start();
    ASSERT_TRUE(witness.waitForRevalidations(1, patience));
    ASSERT_TRUE(witness.waitForSamples(1, patience));
    EXPECT_EQ(calls.load(), 0u);

    destructor.open();
    released.get();
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

    // How long the three samples take is up to the runner; that they arrive at all is the assertion.
    EXPECT_TRUE(item.waitForSamples(3, 5s));
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
    const auto resumedAt = Clock::now();
    connected = true;
    scheduler.onReconnected();

    ASSERT_TRUE(item.waitForSamples(beforeResume + 3, 5s));
    scheduler.stop();

    // The backlog is dropped, so the resumed item samples at its normal rate instead of firing the ~30
    // missed reads at once. Two intervals' worth of window holds three samples at the normal rate and
    // all thirty of a burst; counting inside it cannot overcount on a slow runner.
    const auto times = item.takeSampleTimes();
    EXPECT_LE(countWithin(times, resumedAt, 40ms), 3u);
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

TEST(SamplingSchedulerTest, DestructorDetachesItemsThatAreStillRegistered)
{
    // Items outlive the scheduler whenever the owning device is destroyed without removed() running
    // first: the device destroys its scheduler member, and only afterwards does the base class release
    // the function blocks. Without this callback each of those blocks would unregister from a destroyed
    // scheduler, which on macOS surfaces as "mutex lock failed: Invalid argument" and terminates.
    FakeItem item(20);

    {
        SamplingScheduler scheduler(nullptr);
        scheduler.start();
        scheduler.registerItem(&item);
        ASSERT_TRUE(item.waitForSamples(1, 1s));
        EXPECT_EQ(item.schedulerDestroyedCalls.load(), 0u);
    }

    EXPECT_EQ(item.schedulerDestroyedCalls.load(), 1u);
}

TEST(SamplingSchedulerTest, DestructorDoesNotDetachItemsThatUnregisteredThemselves)
{
    FakeItem item(20);

    {
        SamplingScheduler scheduler(nullptr);
        scheduler.start();
        scheduler.registerItem(&item);
        ASSERT_TRUE(item.waitForSamples(1, 1s));
        scheduler.unregisterItem(&item);
    }

    EXPECT_EQ(item.schedulerDestroyedCalls.load(), 0u);
}

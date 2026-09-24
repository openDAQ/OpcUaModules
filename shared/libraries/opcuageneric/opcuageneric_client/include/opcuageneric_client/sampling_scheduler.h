#pragma once
#include <opcuageneric_client/opcuageneric.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

class ISampledItem
{
public:
    virtual ~ISampledItem() = default;

    // Sampling period in milliseconds.
    // It must not take any lock that could be held while processSample() runs.
    virtual uint32_t getSamplingInterval() const = 0;

    // Performs one read and publishes the result.
    virtual void processSample() = 0;

    // Called once per item after the connection has been re-established.
    virtual void onConnectionRestored() = 0;

    // Called once for every still-registered item while the scheduler is being destroyed. The item
    // must drop its back-pointer here: the scheduler is gone by the time the item itself is torn down.
    virtual void onSchedulerDestroyed() = 0;
};

// Drives all monitored items of a device from a single thread. Each item keeps its own deadline.
class SamplingScheduler
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit SamplingScheduler(std::function<bool()> isConnected);
    ~SamplingScheduler();

    SamplingScheduler(const SamplingScheduler&) = delete;
    SamplingScheduler& operator=(const SamplingScheduler&) = delete;

    void start();
    void stop();
    void registerItem(ISampledItem* item);

    // Removes the item and waits for an in-progress processSample() on it to return, so that the
    // caller can destroy the item afterwards.
    void unregisterItem(ISampledItem* item);

    // Requests onConnectionRestored() for every item.
    void onReconnected();

    static TimePoint advanceDeadline(TimePoint due, TimePoint now, std::chrono::milliseconds interval);

private:
    struct Entry
    {
        ISampledItem* item;
        TimePoint nextDue;
    };

    static constexpr std::chrono::milliseconds DISCONNECTED_POLL_INTERVAL{1000};

    void loop();
    void revalidateItems();

    // Runs fn on the item outside of the mutex while keeping unregisterItem() correct.
    void invokeUnlocked(std::unique_lock<std::mutex>& lock, ISampledItem* item, const std::function<void(ISampledItem*)>& fn);

    std::function<bool()> isConnected;

    std::thread thread;
    std::atomic<bool> running{false};
    std::atomic<bool> revalidatePending{false};

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<Entry> items;

    ISampledItem* inFlight{nullptr};
    std::condition_variable inFlightCv;
};

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC

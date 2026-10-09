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

    // The scheduler holds a reference to the owner for the duration of every call into the item, so the
    // item cannot be destroyed under a call in progress. Without an owner it is up to the caller to keep
    // the item alive until no call can be in progress any more.
    void registerItem(ISampledItem* item, const WeakRefPtr<IBaseObject>& owner = nullptr);

    // Stops further calls into the item. Does not wait for one that is in progress.
    void unregisterItem(ISampledItem* item);

    // Requests onConnectionRestored() for every item.
    void onReconnected();

    static TimePoint advanceDeadline(TimePoint due, TimePoint now, std::chrono::milliseconds interval);

private:
    struct Entry
    {
        ISampledItem* item;
        WeakRefPtr<IBaseObject> owner;
        TimePoint nextDue;
    };

    static constexpr std::chrono::milliseconds DISCONNECTED_POLL_INTERVAL{1000};

    void loop();
    void revalidateItems(std::unique_lock<std::mutex>& lock);

    // Takes the reference that keeps the item of the entry alive during a call into it; it stays
    // unassigned for an item without an owner. Returns false if the owner is already being destroyed,
    // in which case the item must not be touched.
    static bool acquireOwner(const Entry& entry, BaseObjectPtr& owner);

    // Runs fn on the item outside of the mutex and lets go of the owner there as well.
    void invokeUnlocked(std::unique_lock<std::mutex>& lock,
                        ISampledItem* item,
                        BaseObjectPtr owner,
                        const std::function<void(ISampledItem*)>& fn);

    std::function<bool()> isConnected;

    std::thread thread;
    std::atomic<bool> running{false};

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<Entry> items;
    bool revalidatePending{false};
};

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC

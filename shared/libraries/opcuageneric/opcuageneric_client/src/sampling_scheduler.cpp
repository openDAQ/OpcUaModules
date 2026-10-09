#include <opcuageneric_client/sampling_scheduler.h>
#include <algorithm>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

SamplingScheduler::SamplingScheduler(std::function<bool()> isConnected)
    : isConnected(std::move(isConnected))
{
}

SamplingScheduler::~SamplingScheduler()
{
    stop();

    // stop() has joined the thread, so nothing else can touch `items` from here on. Anything still in
    // the list belongs to a component that was never removed(), and it outlives this object - tell it
    // to forget us before its own destructor tries to unregister from freed memory.
    std::vector<Entry> leftover;
    {
        std::scoped_lock lock(mutex);
        leftover.swap(items);
    }

    for (const auto& entry : leftover)
    {
        BaseObjectPtr owner;
        if (acquireOwner(entry, owner))
            entry.item->onSchedulerDestroyed();
    }
}

SamplingScheduler::TimePoint SamplingScheduler::advanceDeadline(TimePoint due, TimePoint now, std::chrono::milliseconds interval)
{
    if (interval < std::chrono::milliseconds(1))
        interval = std::chrono::milliseconds(1);

    due += interval;
    return due > now ? due : now + interval;
}

void SamplingScheduler::start()
{
    if (thread.joinable())
        return;

    running = true;
    thread = std::thread([this] { loop(); });
}

void SamplingScheduler::stop()
{
    {
        std::scoped_lock lock(mutex);
        running = false;
    }
    cv.notify_all();
    if (thread.joinable())
        thread.join();
}

void SamplingScheduler::registerItem(ISampledItem* item, const WeakRefPtr<IBaseObject>& owner)
{
    if (item == nullptr)
        return;

    {
        std::scoped_lock lock(mutex);
        items.push_back({item, owner, Clock::now()});
    }
    cv.notify_all();
}

void SamplingScheduler::unregisterItem(ISampledItem* item)
{
    {
        std::scoped_lock lock(mutex);
        items.erase(std::remove_if(items.begin(), items.end(), [item](const Entry& e) { return e.item == item; }), items.end());
    }
    cv.notify_all();
}

void SamplingScheduler::onReconnected()
{
    {
        std::scoped_lock lock(mutex);
        revalidatePending = true;
    }
    cv.notify_all();
}

bool SamplingScheduler::acquireOwner(const Entry& entry, BaseObjectPtr& owner)
{
    if (!entry.owner.assigned())
        return true;

    owner = entry.owner.getRef();
    return owner.assigned();
}

void SamplingScheduler::invokeUnlocked(std::unique_lock<std::mutex>& lock,
                                       ISampledItem* item,
                                       BaseObjectPtr owner,
                                       const std::function<void(ISampledItem*)>& fn)
{
    lock.unlock();

    try
    {
        fn(item);
    }
    catch (...)
    {
        // Items report their own errors through the component status; swallow whatever still escapes
        // so that one misbehaving item cannot tear down sampling for the whole device.
    }

    // This can be the last reference, in which case the item is destroyed right here and unregisters
    // itself on the way.
    owner.release();
    lock.lock();
}

void SamplingScheduler::revalidateItems(std::unique_lock<std::mutex>& lock)
{
    // Snapshot: the list can change while an item is revalidated outside of the mutex.
    std::vector<ISampledItem*> pending;
    pending.reserve(items.size());
    for (const auto& entry : items)
        pending.push_back(entry.item);

    for (auto* item : pending)
    {
        if (!running)
            return;

        const auto entry = std::find_if(items.begin(), items.end(), [item](const Entry& e) { return e.item == item; });
        if (entry == items.end())
            continue;

        BaseObjectPtr owner;
        if (!acquireOwner(*entry, owner))
            continue;

        invokeUnlocked(lock, item, std::move(owner), [](ISampledItem* i) { i->onConnectionRestored(); });
    }
}

void SamplingScheduler::loop()
{
    while (running)
    {
        // Checked without holding the mutex: it takes the client lock, which the reconnect thread can
        // be holding for the duration of a connect().
        if (isConnected && !isConnected())
        {
            std::unique_lock lock(mutex);
            cv.wait_for(lock, DISCONNECTED_POLL_INTERVAL, [this] { return !running.load() || revalidatePending; });
            continue;
        }

        std::unique_lock lock(mutex);
        if (!running)
            break;

        if (revalidatePending)
        {
            revalidatePending = false;
            revalidateItems(lock);
            continue;
        }

        if (items.empty())
        {
            cv.wait(lock, [this] { return !running.load() || !items.empty(); });
            continue;
        }

        const auto next =
            std::min_element(items.begin(), items.end(), [](const Entry& a, const Entry& b) { return a.nextDue < b.nextDue; });

        const auto now = Clock::now();
        if (next->nextDue > now)
        {
            cv.wait_until(lock, next->nextDue);
            continue;
        }

        BaseObjectPtr owner;
        if (!acquireOwner(*next, owner))
        {
            // Being destroyed on another thread, which has not got as far as unregistering it yet.
            items.erase(next);
            continue;
        }

        next->nextDue = advanceDeadline(next->nextDue, now, std::chrono::milliseconds(next->item->getSamplingInterval()));

        invokeUnlocked(lock, next->item, std::move(owner), [](ISampledItem* i) { i->processSample(); });
    }
}

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC

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

void SamplingScheduler::registerItem(ISampledItem* item)
{
    if (item == nullptr)
        return;

    {
        std::scoped_lock lock(mutex);
        items.push_back({item, Clock::now()});
    }
    cv.notify_all();
}

void SamplingScheduler::unregisterItem(ISampledItem* item)
{
    {
        std::unique_lock lock(mutex);
        items.erase(std::remove_if(items.begin(), items.end(), [item](const Entry& e) { return e.item == item; }), items.end());
        inFlightCv.wait(lock, [this, item] { return inFlight != item; });
    }
    cv.notify_all();
}

void SamplingScheduler::onReconnected()
{
    revalidatePending = true;
    cv.notify_all();
}

void SamplingScheduler::invokeUnlocked(std::unique_lock<std::mutex>& lock, ISampledItem* item, const std::function<void(ISampledItem*)>& fn)
{
    inFlight = item;
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

    lock.lock();
    inFlight = nullptr;
    inFlightCv.notify_all();
}

void SamplingScheduler::revalidateItems()
{
    std::unique_lock lock(mutex);

    // Snapshot: the list can change while an item is revalidated outside of the mutex.
    std::vector<ISampledItem*> pending;
    pending.reserve(items.size());
    for (const auto& entry : items)
        pending.push_back(entry.item);

    for (auto* item : pending)
    {
        if (!running)
            return;

        const bool stillRegistered = std::any_of(items.begin(), items.end(), [item](const Entry& e) { return e.item == item; });
        if (!stillRegistered)
            continue;

        invokeUnlocked(lock, item, [](ISampledItem* i) { i->onConnectionRestored(); });
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
            cv.wait_for(lock, DISCONNECTED_POLL_INTERVAL, [this] { return !running.load(); });
            continue;
        }

        if (revalidatePending.exchange(false))
            revalidateItems();

        std::unique_lock lock(mutex);
        if (!running)
            break;

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

        next->nextDue = advanceDeadline(next->nextDue, now, std::chrono::milliseconds(next->item->getSamplingInterval()));

        invokeUnlocked(lock, next->item, [](ISampledItem* i) { i->processSample(); });
    }
}

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC

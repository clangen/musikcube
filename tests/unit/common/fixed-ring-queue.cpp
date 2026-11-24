#include <common/FixedRingQueue.h>

#include <cstdio>
#include <memory>

static bool require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "fixed ring queue: %s\n", message);
    }
    return condition;
}

int main() {
    musik::FixedRingQueue<std::unique_ptr<int>, 2> queue;
    if (!require(queue.Empty() && !queue.Front() && !queue.TryPop(), "empty queue")) {
        return 1;
    }

    auto first = std::make_unique<int>(1);
    auto second = std::make_unique<int>(2);
    auto third = std::make_unique<int>(3);
    if (!require(queue.TryPush(std::move(first)) && queue.TryPush(std::move(second)), "fill queue") ||
        !require(!queue.TryPush(std::move(third)) && third && *third == 3, "full queue retains caller ownership") ||
        !require(queue.Size() == 2 && **queue.Front() == 1, "oldest value remains at front"))
    {
        return 1;
    }

    if (!require(queue.TryPop() && queue.TryPush(std::move(third)), "wrap around") ||
        !require(**queue.Front() == 2 && queue.TryPop() && **queue.Front() == 3, "preserve FIFO order") ||
        !require(queue.TryPop() && queue.Empty() && !queue.Front(), "drain queue"))
    {
        return 1;
    }

    musik::FixedRingQueue<std::shared_ptr<int>, 1> owners;
    auto value = std::make_shared<int>(4);
    std::weak_ptr<int> observer = value;
    if (!require(owners.TryPush(value), "retain queued value")) {
        return 1;
    }
    value.reset();
    if (!require(!observer.expired() && owners.TryPop() && observer.expired(),
        "popping releases the queued value"))
    {
        return 1;
    }
    return 0;
}

#include <musikcore/plugin/fixed_ring_queue.hpp>

int main() {
    musik::FixedRingQueue<int, 1> queue;
    return queue.TryPush(7) && !queue.TryPush(8) && *queue.Front() == 7 && queue.TryPop() && queue.Empty()
        ? 0
        : 1;
}

#include <musikcore/debug.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/library/query/SearchTrackListQuery.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/runtime/Message.h>
#include <musikcore/runtime/MessageQueue.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include "CoreRuntime.h"

namespace {
    using namespace musik::core;
    using namespace musik::core::runtime;
    using namespace musik::core::library::query;
    using Clock = std::chrono::steady_clock;

    /* Delivers the indexer signal to the fixture's dispatcher. Neither copyable nor movable. */
    class IndexCompletion final : public IMessageTarget, public sigslot::has_slots<> {
    public:
        IndexCompletion(MessageQueue& queue, IIndexer& indexer, int expectedScanned)
        : queue(queue)
        , expectedScanned(expectedScanned) {
            this->queue.Register(this);
            indexer.Finished.connect(this, &IndexCompletion::OnFinished);
        }

        ~IndexCompletion() { this->queue.Unregister(this); }
        IndexCompletion(const IndexCompletion&) = delete;
        IndexCompletion& operator=(const IndexCompletion&) = delete;

        bool Wait() {
            const auto deadline = Clock::now() + std::chrono::minutes(5);
            while (!this->finished) {
                const auto remaining = deadline - Clock::now();
                if (remaining <= Clock::duration::zero()) return false;
                this->queue.WaitAndDispatch(std::chrono::ceil<std::chrono::milliseconds>(remaining).count());
            }
            return true;
        }

        void ProcessMessage(IMessage&) override { this->finished = true; }

    private:
        void OnFinished(int scanned) {
            /* A database upgrade may have already scheduled an empty scan. Wait
            for the scan that includes the fixture media instead of accepting
            the first generic Finished notification. */
            if (scanned >= this->expectedScanned) {
                this->queue.Post(Message::Create(this, 0, 0, 0));
            }
        }

        MessageQueue& queue;
        int expectedScanned;
        bool finished = false;
    };

    bool seed(MessageQueue& queue, const ILibraryPtr& library, const char* media, int64_t expected) {
        auto& indexer = *library->Indexer();
        IndexCompletion completion(queue, indexer, static_cast<int>(expected));
        indexer.AddPath(media);
        indexer.Schedule(IIndexer::SyncType::Local);

        /* Wait for the indexer to commit before inspecting its results. */
        const bool finished = completion.Wait();
        indexer.Shutdown();
        if (!finished) {
            std::fputs("Indexer did not finish\n", stderr);
            return false;
        }

        const auto query = std::make_shared<SearchTrackListQuery>(
            library, SearchTrackListQuery::MatchType::Substring, "", TrackSortType::Title);
        if (library->EnqueueAndWait(query) < 0 || query->GetStatus() != db::IQuery::Finished) {
            std::fputs("Could not verify indexed tracks\n", stderr);
            return false;
        }

        const auto count = query->GetResult()->Count();
        if (expected < 0 || count != static_cast<uint64_t>(expected)) {
            std::fprintf(stderr, "Expected %lld visible tracks, indexed %zu\n",
                static_cast<long long>(expected), count);
            return false;
        }
        return true;
    }
}

int main(int argc, char** argv) {
    if (argc != 3) return 1;

    CoreRuntime runtime;
    auto& queue = runtime.Queue();
    const auto library = LibraryFactory::Instance().DefaultLocalLibrary();
    plugin::Start(&queue, nullptr, library);
    const bool populated = seed(queue, library, argv[1], std::strtoll(argv[2], nullptr, 10));

    return populated ? 0 : 1;
}

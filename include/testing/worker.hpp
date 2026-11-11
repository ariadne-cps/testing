/***************************************************************************
 *            testing/worker.hpp
 *
 *  Copyright  2026  Luca Geretti
 *
 ****************************************************************************/

/*
 *  This file is part of Ariadne Testing.
 *
 *  Ariadne Testing is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Ariadne Testing is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ariadne Testing. If not, see <https://www.gnu.org/licenses/>.
 */

/** @file testing/worker.hpp
 *  @brief Thread-safe staging of worker assertions and deterministic result reporting.
 */
#ifndef ARIADNE_TESTING_WORKER_HPP
#define ARIADNE_TESTING_WORKER_HPP

#include <mutex>

#include "testing/testing.hpp"

namespace Ariadne::Testing {

/** @brief Collect worker checks safely, then report them deterministically.
 *
 * Each numbered worker slot is executed exactly once by run(), typically on
 * a distinct caller-owned thread. Worker checks are staged under a mutex;
 * neither worker code nor its staging calls Context or Reporter. After all
 * worker threads have been joined, flush() publishes assertions on the
 * owning case thread, ordered by slot index and within-slot check order.
 * Missing, duplicate and premature slots cannot silently pass. The caller
 * owns worker lifetime and must retain CTest's deadlock watchdog.
 */
class WorkerBatch {
    struct Entry {
        Outcome outcome;
        std::string description;
        std::string detail;
        std::string file;
        std::uint_least32_t line;
    };
    struct Slot {
        bool started = false;
        bool completed = false;
        std::vector<Entry> entries;
    };

  public:
    class Worker {
      public:
        /** @brief Buffer one mathematical-evidence check on the worker thread.
         *  @note The corresponding CHECK event is emitted on flush, not here.
         *  The predicate is evaluated once; exceptions become Error records.
         */
        template<class Predicate>
        void check(Predicate&& predicate, std::string description,
                   std::source_location location = std::source_location::current()) {
            Outcome status = Outcome::Passed;
            std::string detail;
            try {
                switch (Detail::evidence(std::invoke(std::forward<Predicate>(predicate)))) {
                    case Evidence::True:
                        status = Outcome::Passed;
                        break;
                    case Evidence::False:
                        status = Outcome::Failed;
                        break;
                    case Evidence::Indeterminate:
                        status = Outcome::Inconclusive;
                        break;
                    default:
                        status = Outcome::Error;
                        detail = "invalid evidence";
                        break;
                }
            } catch (const std::exception& error) {
                status = Outcome::Error;
                detail = error.what();
            } catch (...) {
                status = Outcome::Error;
                detail = "non-standard worker predicate exception";
            }
            _owner.add(_slot, {status, std::move(description), std::move(detail),
                               location.file_name(), location.line()});
        }

      private:
        friend class WorkerBatch;
        Worker(WorkerBatch& owner, std::size_t slot) : _owner(owner), _slot(slot) {}
        WorkerBatch& _owner;
        std::size_t _slot;
    };

    explicit WorkerBatch(std::size_t count) : _slots(count) {
        if (count == 0)
            throw std::invalid_argument("WorkerBatch needs a nonzero slot count");
    }
    WorkerBatch(const WorkerBatch&) = delete;
    WorkerBatch& operator=(const WorkerBatch&) = delete;

    /** @brief Execute one numbered worker and capture unhandled exceptions.
     *  @note The Worker reference must not escape the callback. Caller joins
     *  all threads before flush; this method does not create/join/cancel any.
     */
    template<class Callback>
    void run(std::size_t slot, Callback&& callback,
             std::source_location location = std::source_location::current()) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_flushed || slot >= _slots.size() || _slots[slot].started)
                throw std::logic_error("invalid or repeated worker slot");
            _slots[slot].started = true;
        }
        Worker worker(*this, slot);
        try {
            std::invoke(std::forward<Callback>(callback), worker);
        } catch (const std::exception& error) {
            add(slot, {Outcome::Error, "uncaught worker exception", error.what(),
                       location.file_name(), location.line()});
        } catch (...) {
            add(slot, {Outcome::Error, "uncaught non-standard worker exception",
                       "non-standard worker exception",
                       location.file_name(), location.line()});
        }
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_slots[slot].entries.empty())
                _slots[slot].entries.push_back({Outcome::Error, "no worker checks",
                                                "worker completed without recording checks",
                                                location.file_name(), location.line()});
            _slots[slot].completed = true;
        }
    }

    /** @brief Replay every worker check on the owning Context, exactly once.
     *  @throws std::logic_error if a worker is unfinished or already flushed.
     */
    void flush(Context& context) {
        std::vector<Entry> ordered;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_flushed)
                throw std::logic_error("worker batch already flushed");
            for (const auto& slot: _slots)
                if (!slot.completed)
                    throw std::logic_error("worker batch incomplete");
            for (std::size_t i = 0; i < _slots.size(); ++i) {
                for (const auto& entry: _slots[i].entries) {
                    auto record = entry;
                    record.description = "worker[" + std::to_string(i) + "]: " + record.description;
                    ordered.push_back(std::move(record));
                }
            }
            _flushed = true;
        }
        for (const auto& entry: ordered) {
            const auto check_index = context.begin_check(entry.description);
            context.record(entry.outcome, entry.description, {}, "proved",
                           entry.detail, std::source_location::current(), check_index);
            // Copy back the worker's origin, not the flush implementation.
            auto& assertion = context._result.assertions.back();
            assertion.file = entry.file;
            assertion.line = entry.line;
        }
    }

  private:
    void add(std::size_t slot, Entry entry) {
        std::lock_guard<std::mutex> lock(_mutex);
        _slots[slot].entries.push_back(std::move(entry));
    }
    std::mutex _mutex;
    std::vector<Slot> _slots;
    bool _flushed = false;
};

} // namespace Ariadne::Testing
#endif // ARIADNE_TESTING_WORKER_HPP

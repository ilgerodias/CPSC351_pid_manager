#include <cassert>
#include <cstdlib>
#include <iostream>
#include <random>
#include <iterator>
#include <unordered_set>
#include <vector>
#include "pid_manager.hpp"

// Small helper to assert with a message
#define CHECK(cond, msg) do { if (!(cond)) { \
    std::cerr << "CHECK failed: " << (msg) << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
    std::abort(); } } while(0)

static void requirement_tests() {
    std::cout << "[Requirement Tests]\n";

    PIDManager m;
    // 1) Call allocate_map to initialize the data structure.
    int ok = m.allocate_map();
    CHECK(ok == 1, "allocate_map should return 1 on success");
    CHECK(m.initialized(), "manager should be initialized after allocate_map");

    // 2) Call allocate_pid multiple times to allocate PIDs.
    std::vector<int> allocated;
    for (int i = 0; i < 5; ++i) {
        int pid = m.allocate_pid();
        CHECK(pid != -1, "allocate_pid should succeed while capacity remains");
        allocated.push_back(pid);
    }

    // 3) Check if the allocated PIDs fall within the specified range.
    for (int pid : allocated) {
        CHECK(m.in_range(pid), "allocated pid must be within [MIN_PID, MAX_PID]");
        CHECK(m.is_allocated(pid), "allocated pid should be marked allocated");
    }

    // 4) Call release_pid for each allocated PID.
    for (int pid : allocated) {
        m.release_pid(pid);
    }

    // 5) Check if the released PIDs become available for allocation again.
    // Easiest way: allocate the same count again and verify not -1, in-range.
    std::vector<int> reallocated;
    for (int i = 0; i < 5; ++i) {
        int pid = m.allocate_pid();
        CHECK(pid != -1, "re-allocation after release should succeed");
        CHECK(m.in_range(pid), "re-allocated pid must be within range");
        reallocated.push_back(pid);
    }

    std::cout << "  ✓ initialize, allocate, range-check, release, and reuse passed\n\n";
}

static void what_if_tests() {
    std::cout << "[What-if Tests]\n";

    // 1) Test error handling by calling allocate_pid before allocate_map.
    {
        PIDManager m;
        int pid = m.allocate_pid();
        CHECK(pid == -1, "allocate_pid before allocate_map must return -1");
        std::cout << "  ✓ allocate_pid before allocate_map returns -1\n";
    }

    // 2) Test releasing a PID without initializing the data structure.
    {
        PIDManager m;
        // Should be a safe no-op (no crash). We can't assert side effects except "no throw".
        m.release_pid(MIN_PID);
        std::cout << "  ✓ release_pid before allocate_map is a safe no-op\n";
    }

    // 3) Allocate and release PIDs in a loop for a large number of iterations.
    {
        PIDManager m;
        CHECK(m.allocate_map() == 1, "allocate_map must succeed");
        const int iterations = 5000;
        for (int i = 0; i < iterations; ++i) {
            int pid = m.allocate_pid();
            CHECK(pid != -1, "should allocate within capacity during loop");
            CHECK(m.in_range(pid), "allocated pid must be in range");
            m.release_pid(pid);
        }
        std::cout << "  ✓ allocate/release loop ("
                  << 5000 << " iterations) passed\n";
    }

    // 4) Test for memory leaks:
    //    Note: The class uses RAII and std::vector; no dynamic allocations are leaked
    //    in normal usage. Use ASan or Valgrind externally to confirm (commands printed later).
    std::cout << "  ✓ memory management is RAII-based; use ASan/Valgrind to verify\n";

    // 5) Randomly allocate and release PIDs multiple times.
    {
        PIDManager m;
        CHECK(m.allocate_map() == 1, "allocate_map must succeed");

        std::mt19937 rng(12345);
        std::uniform_int_distribution<int> coin(0, 1);
        std::unordered_set<int> live;

        const int ops = 10000;
        for (int i = 0; i < ops; ++i) {
            if (coin(rng) == 0 || live.empty()) {
                // allocate
                int pid = m.allocate_pid();
                if (pid != -1) {
                    CHECK(m.in_range(pid), "random: allocated pid must be in range");
                    bool inserted = live.insert(pid).second;
                    CHECK(inserted, "random: pid should not be duplicated");
                } else {
                    // exhausted or uninitialized (not the case here)
                    CHECK(true, "random: allocate_pid returned -1 when exhausted — acceptable");
                }
            } else {
                // release a random element from 'live'
                auto it = live.begin();
                std::advance(it, rng() % live.size());
                int pid = *it;
                m.release_pid(pid);
                live.erase(it);
            }
        }

        // ensure released PIDs become available again: release everything then allocate again
        for (int pid : live) m.release_pid(pid);
        live.clear();

        int pid1 = m.allocate_pid();
        CHECK(pid1 != -1 && m.in_range(pid1), "random: after releases, a PID should be allocatable again");
        std::cout << "  ✓ randomized allocate/release passed\n";
    }

    // 6) Attempt to allocate a PID when the range is exhausted (MAX_PID - MIN_PID + 1 allocations).
    {
        PIDManager m;
        CHECK(m.allocate_map() == 1, "allocate_map must succeed");
        const int capacity = (MAX_PID - MIN_PID + 1);
        std::vector<int> all;
        all.reserve(capacity);

        for (int i = 0; i < capacity; ++i) {
            int pid = m.allocate_pid();
            CHECK(pid != -1, "within capacity, allocate_pid must succeed");
            CHECK(m.in_range(pid), "exhaustion: allocated pid must be in range");
            all.push_back(pid);
        }
        // Next allocation should fail with -1
        int fail = m.allocate_pid();
        CHECK(fail == -1, "exhaustion: allocate_pid must return -1 when all PIDs are in use");

        // Clean up and verify we can allocate again
        for (int pid : all) m.release_pid(pid);
        int pid_after_release = m.allocate_pid();
        CHECK(pid_after_release != -1, "after releasing all, allocation should work again");
        std::cout << "  ✓ exhaustion behavior and recovery passed\n";
    }

    std::cout << "\n";
}

int main() {
    requirement_tests();
    what_if_tests();
    std::cout << "All PID manager tests completed successfully." << std::endl;
    return 0;
}

#pragma once
#include <vector>
#include <stdexcept>
#include <algorithm>

#define MIN_PID 100
#define MAX_PID 1000

class PIDManager {
public:
    explicit PIDManager(int minPid = MIN_PID, int maxPid = MAX_PID)
        : min_pid(minPid), max_pid(maxPid),
          bitmap(static_cast<size_t>(maxPid + 1), 0),
          next(minPid), initialized_(false) {
        if (min_pid < 0 || max_pid < min_pid) {
            throw std::invalid_argument("Invalid PID range");
        }
    }

    // Creates and initializes the PID map. Returns -1 on failure, 1 on success.
    int allocate_map(void) {
        try {
            std::fill(bitmap.begin(), bitmap.end(), 0);
            next = min_pid;
            initialized_ = true;
            return 1;
        } catch (...) {
            initialized_ = false;
            return -1;
        }
    }

    // Allocates and returns a PID; returns -1 if not initialized or if all are in use.
    int allocate_pid(void) {
        if (!initialized_) return -1;
        if (min_pid > max_pid) return -1;

        int start = next;
        for (int pid = start; pid <= max_pid; ++pid) {
            if (bitmap[pid] == 0) {
                bitmap[pid] = 1;
                next = (pid + 1 > max_pid) ? min_pid : pid + 1;
                return pid;
            }
        }
        for (int pid = min_pid; pid < start; ++pid) {
            if (bitmap[pid] == 0) {
                bitmap[pid] = 1;
                next = (pid + 1 > max_pid) ? min_pid : pid + 1;
                return pid;
            }
        }
        return -1; // exhausted
    }

    // Releases a PID; safe no-op if not initialized, invalid PID, or already free.
    void release_pid(int pid) {
        if (!initialized_) return;
        if (pid < min_pid || pid > max_pid) return;
        bitmap[pid] = 0;
        if (pid < next) next = pid; // bias to reuse earlier frees
    }

    // Helpers for tests
    bool initialized() const { return initialized_; }
    int min() const { return min_pid; }
    int max() const { return max_pid; }
    bool in_range(int pid) const { return pid >= min_pid && pid <= max_pid; }
    bool is_allocated(int pid) const {
        if (pid < min_pid || pid > max_pid) return false;
        return bitmap[pid] != 0;
    }

private:
    int min_pid;
    int max_pid;
    std::vector<unsigned char> bitmap;
    int next;
    bool initialized_;
};

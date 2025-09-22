// pid_manager.cpp
#include <iostream>
#include <vector>
#include <unistd.h>     // fork, getpid
#include <sys/wait.h>   // waitpid
#include "pid_manager.hpp"

// Pretty-print a list of PIDs
static void print_list(const char* tag, const std::vector<int>& pids) {
    std::cout << tag << " [";
    for (size_t i = 0; i < pids.size(); ++i) {
        std::cout << pids[i];
        if (i + 1 < pids.size()) std::cout << ", ";
    }
    std::cout << "]\n";
}

int main() {
    std::cout << "PID Manager Demo (OOP + bitmap). Process PID=" << getpid() << "\n";
    std::cout << "Global range: [" << MIN_PID << ", " << MAX_PID << "]\n\n";

    // ----- Parent process: create a manager and do some allocations -----
    PIDManager parentMgr;                 // uses MIN_PID..MAX_PID
    if (parentMgr.allocate_map() != 1) {
        std::cerr << "Failed to initialize PID map in parent.\n";
        return 1;
    }

    std::vector<int> parentPIDs;
    for (int i = 0; i < 3; ++i) {
        int pid = parentMgr.allocate_pid();
        parentPIDs.push_back(pid);
    }
    std::cout << "[Parent " << getpid() << "] Allocated initial PIDs: ";
    print_list("", parentPIDs);

    // Release the middle one to show reuse
    if (parentPIDs.size() >= 2) {
        parentMgr.release_pid(parentPIDs[1]);
        std::cout << "[Parent " << getpid() << "] Released PID " << parentPIDs[1] << "\n";
        int reused = parentMgr.allocate_pid();
        std::cout << "[Parent " << getpid() << "] Re-allocated PID (should be same or next available): " << reused << "\n\n";
        if (reused != -1) {
            parentPIDs[1] = reused;
        }
    }

    // ----- Optional: small-range unit test to demonstrate exhaustion behavior -----
    // NOTE: This uses a custom range just for testing. Your main manager still uses MIN..MAX.
    {
        PIDManager tiny(1, 3);
        tiny.allocate_map();
        std::vector<int> tinyPids;
        for (int i = 0; i < 4; ++i) { // 4th should fail
            tinyPids.push_back(tiny.allocate_pid());
        }
        std::cout << "[Parent " << getpid() << "] Tiny-range allocations (1..3, 4 requests): ";
        print_list("", tinyPids); // Expect something like: [1, 2, 3, -1]
        tiny.release_pid(2);
        int again = tiny.allocate_pid();  // should get 2 again
        std::cout << "[Parent " << getpid() << "] Tiny-range re-allocation after release: " << again << "\n\n";
    }

    // ----- fork() to demonstrate independent managers in parent vs child -----
    pid_t child = fork();
    if (child < 0) {
        std::perror("fork");
        return 1;
    } else if (child == 0) {
        // CHILD PROCESS: has its own address space; create its own manager
        PIDManager childMgr; // independent manager instance (fresh bitmap)
        if (childMgr.allocate_map() != 1) {
            std::cerr << "Failed to initialize PID map in child.\n";
            _exit(2);
        }

        std::vector<int> childPIDs;
        for (int i = 0; i < 5; ++i) {
            childPIDs.push_back(childMgr.allocate_pid());
        }

        std::cout << "[Child  " << getpid() << "] Allocated PIDs: ";
        print_list("", childPIDs);

        // Release a couple and allocate again to show reuse
        if (!childPIDs.empty()) {
            childMgr.release_pid(childPIDs.front());
            childMgr.release_pid(childPIDs.back());
            std::cout << "[Child  " << getpid() << "] Released PIDs " << childPIDs.front()
                      << " and " << childPIDs.back() << "\n";
            int a = childMgr.allocate_pid();
            int b = childMgr.allocate_pid();
            std::cout << "[Child  " << getpid() << "] Re-allocated PIDs: " << a << ", " << b << "\n";
            if (a != -1) childPIDs.push_back(a);
            if (b != -1) childPIDs.push_back(b);
        }

        // Child done
        std::cout << "[Child  " << getpid() << "] Done. Releasing all its PIDs...\n";
        for (int pid : childPIDs) {
            if (pid != -1) {
                childMgr.release_pid(pid);
            }
        }
        _exit(0);
    } else {
        // PARENT continues with its manager while child runs
        std::cout << "[Parent " << getpid() << "] Continuing allocations while child runs...\n";
        std::vector<int> moreParent;
        for (int i = 0; i < 2; ++i) {
            moreParent.push_back(parentMgr.allocate_pid());
        }
        std::cout << "[Parent " << getpid() << "] Additional PIDs: ";
        print_list("", moreParent);

        int status = 0;
        waitpid(child, &status, 0);
        std::cout << "\n[Parent " << getpid() << "] Child " << child << " exited with status " << status << "\n";

        // Wrap up parent: release everything it took
        for (int pid : parentPIDs) {
            if (pid != -1) {
                parentMgr.release_pid(pid);
            }
        }
        for (int pid : moreParent) {
            if (pid != -1) {
                parentMgr.release_pid(pid);
            }
        }
        std::cout << "[Parent " << getpid() << "] Done. Released all its PIDs.\n";
    }

    return 0;
}

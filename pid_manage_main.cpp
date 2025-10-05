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
    //Creating PIPE
    int pipe_child_to_parent[2];
    int pipe_parent_to_child[2];
    if (pipe(pipe_child_to_parent) == -1 || pipe(pipe_parent_to_child) == -1) {
        perror("pipe");
        return 1;
    }


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
        close(pipe_child_to_parent[0]); //CLOSING READEND OF CHILD to PARENT PIPE
        close(pipe_parent_to_child[1]); // CLOSING WRITE END OF PARENT TO CHILD PIPE

        for (int i =0; i < 5; ++i) {
            //Send a reuqest to bye to parent, "1" meaning allocate PID
            char request = 1;
            if (write(pipe_child_to_parent[1], &request, 1) != 1){
                perror("write child request");
                break;
            }
        }
        // Read allocated PID from parent (int)
        int allocated_pid = -1;
            ssize_t r = read(pipe_parent_to_child[0], &allocated_pid, sizeof(allocated_pid));
            if (r <= 0) {
                perror("child read pid");
                
            }
            std::cout << "[Child " << getpid() << "] Received PID: " << allocated_pid << "\n";

        
        close(pipe_child_to_parent[1]); //CLOSING WRITE END OF CHILD TO PARENT PIPE
        close(pipe_parent_to_child[0]); //CLOSING READ END OF PARENT TO CHILD PIPE
        _exit(0);
    } else {
        // PARENT continues with its manager while child runs

        close(pipe_child_to_parent[1]); // close write end of child's request pipe
        close(pipe_parent_to_child[0]); // close read end of parent's response pipe



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

        close(pipe_child_to_parent[0]); //CLOSING read end of child's request pipe
        close(pipe_parent_to_child[1]); //closing write end of parent's response pipe


    }

    return 0;
}

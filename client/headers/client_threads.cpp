#include "./thread_header.h"
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <algorithm>

using namespace std;

// Dynamically determine thread count based on hardware
const int MAX_THREADS = max(2u, min(10u, thread::hardware_concurrency()));
vector<std::thread> thread_pool(MAX_THREADS);
vector<bool> thread_available(MAX_THREADS, true);
mutex thread_mutex;
condition_variable thread_cv;

// Function to check if any thread is available
bool is_any_thread_available() {
    for (bool available : thread_available) {
        if (available) return true;
    }
    return false;
}

// Function to find an available thread index
int get_available_thread() {
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (thread_available[i]) {
            thread_available[i] = false;
            return i;
        }
    }
    return -1; // No thread available
}

// Function to mark thread as available
void release_thread(int idx) {
    {
        unique_lock<mutex> lock(thread_mutex);
        thread_available[idx] = true;
    }
    thread_cv.notify_one();
}


// Function to join all threads before exiting
void join_all_threads() {
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (thread_pool[i].joinable()) {
            thread_pool[i].join();
        }
    }
}

void clear_thread_pool() {
    thread_pool.clear();
    thread_available.clear();
    thread_pool.resize(MAX_THREADS);
    thread_available.resize(MAX_THREADS, true);
}
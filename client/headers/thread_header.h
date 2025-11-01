#pragma once
#ifndef THREAD_HEADER_H
#define THREAD_HEADER_H

#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>

using namespace std;

extern const int MAX_THREADS;
extern vector<thread> thread_pool;
extern vector<bool> thread_available;
extern mutex thread_mutex;
extern condition_variable thread_cv;

bool is_any_thread_available();

int get_available_thread();

void release_thread(int idx);

void join_all_threads();

void clear_thread_pool();

#endif
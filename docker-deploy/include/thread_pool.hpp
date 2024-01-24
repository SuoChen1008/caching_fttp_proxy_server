//
// Created by rs590 on 2/19/23.
//

#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#pragma once

#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    // Constructor, creates a specified number of worker threads.
    explicit ThreadPool(size_t numThreads);

    // Destructor, stops all worker threads.
    ~ThreadPool();

    // Adds a task to the task queue and waits for a worker thread to execute it.
    void enqueue(std::function<void()> task);

private:
    std::vector<std::thread> m_workers; // Array of worker threads.
    std::queue<std::function<void()>> m_tasks; // Task queue.
    std::mutex m_mutex; // Mutex to protect the task queue.
    std::condition_variable m_cv; // Condition variable for task queue synchronization.
    bool m_stop; // Stop flag to notify worker threads to stop running.
};


#endif


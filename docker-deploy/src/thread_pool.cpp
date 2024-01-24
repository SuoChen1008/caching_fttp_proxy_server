#include <thread_pool.hpp>

ThreadPool::ThreadPool(size_t numThreads)
        : m_stop(false) {
    for (size_t i = 0; i < numThreads; i++) {
        // Create a specified number of worker threads.
        m_workers.emplace_back(
                [this]() {
                    while (true) {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(m_mutex);
                            // Wait for a task to be added to the task queue or for the stop flag to be set.
                            m_cv.wait(lock, [this]() { return m_stop || !m_tasks.empty(); });
                            // If the stop flag is set and the task queue is empty, the thread exits.
                            if (m_stop && m_tasks.empty()) {
                                return;
                            }
                            // Take the first task in the task queue.
                            task = std::move(m_tasks.front());
                            m_tasks.pop();
                        }

                        // Execute the task.
                        task();
                    }
                }
        );
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // Set the stop flag and notify worker threads to exit.
        m_stop = true;
    }
    // Notify all worker threads.
    m_cv.notify_all();
    // Wait for all worker threads to exit.
    for (std::thread &worker: m_workers) {
        worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // Add the task to the task queue.
        m_tasks.emplace(std::move(task));
    }
    // Notify one worker thread to execute the task.
    m_cv.notify_one();
}


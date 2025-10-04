#ifndef SIMPLE_THREAD_POOL_H
#define SIMPLE_THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

// Forward declaration of the function our threads will execute
void handle_client(int client_socket);

class SimpleThreadPool {
public:
    // Constructor: Creates a pool with a specific number of threads
    SimpleThreadPool(int num_threads) : stop(false) {
        // Create the specified number of worker threads
        for (int i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                // This is the code each thread will run forever
                while (true) {
                    int client_socket;

                    // This block is for safely getting a task from the queue
                    {
                        // Lock the mutex so only one thread can access the queue at a time
                        std::unique_lock<std::mutex> lock(this->queue_mutex);

                        // Wait until there is a task in the queue OR the pool is stopped
                        this->condition.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });

                        // If the pool is stopped and the queue is empty, exit the thread
                        if (this->stop && this->tasks.empty()) {
                            return;
                        }

                        // Get the client socket from the front of the queue
                        client_socket = this->tasks.front();
                        this->tasks.pop();
                    } // The lock is automatically released here

                    // Now that we have a task (the client socket), handle it
                    handle_client(client_socket);
                }
            });
        }
    }

    // Destructor: Gracefully shuts down the threads
    ~SimpleThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true; // Signal threads to stop
        }
        condition.notify_all(); // Wake up all sleeping threads
        for (std::thread &worker : workers) {
            worker.join(); // Wait for each thread to finish
        }
    }

    // Adds a new client socket to the task queue
    void enqueue(int client_socket) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push(client_socket);
        }
        condition.notify_one(); // Wake up one sleeping thread to handle the new task
    }

    // Returns the number of tasks waiting in the queue
    size_t get_queue_size() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        return tasks.size();
    }

private:
    std::vector<std::thread> workers;
    std::queue<int> tasks;
    
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

#endif // SIMPLE_THREAD_POOL_H
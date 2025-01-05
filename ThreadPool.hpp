/*
 * Copyright 2025 University of Michigan Solar Car Team
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the “Software”), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall
 * be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY
 * KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SIMULATOR_THREADPOOL_H
#define SIMULATOR_THREADPOOL_H

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

/*
 * University of Michigan Solar Car Team
 *
 * Light weight C++20 thread pool implementation
 *
 */

class ThreadPool {
   public:
	ThreadPool() {
		reset(std::thread::hardware_concurrency());
	}

	~ThreadPool() {
		running = false;
		cv.notify_all();
		for (auto& thread : threads) {
			thread.join();
		}
	}

	/// @brief  Construct a thread pool with a specific number of threads
	/// @param num_threads Number of threads for the pool
	explicit ThreadPool(size_t num_threads) {
		reset(num_threads);
	}

	
    /// @brief  Reset the number of threads in the pool
    /// @param num_threads Number of threads for the pool
    /// @throws std::runtime_error if the number of threads is less than the current number of threads
	void reset(size_t num_threads) {
		if (num_threads >= threads.size()) {
			size_t i = threads.size();
			for (; i < num_threads; i++) {
				threads.emplace_back([this] { worker_loop(); });
			}
			return;
		}
		throw std::runtime_error("Cannot downscale thread pool");
	}

    /// @brief  Get the number of threads in the pool
	size_t size() {
		return threads.size();
	}

	/// @brief Add a task to the thread pool
	/// @param task The task to be added
	void detach_task(std::function<void()>&& task) {
		std::unique_lock<std::mutex> lock(data_lock);
		jobs.push({std::move(task)});
		cv.notify_one();
	}

    /// @brief Adds tasks to the thread pool
    /// @param tasks A vector of tasks to be added
	void detach_tasks(std::vector<std::function<void()>>&& tasks) {
		std::unique_lock<std::mutex> lock(data_lock);
		for (auto& task : tasks) {
			jobs.push({std::move(task)});
		}
		cv.notify_all();
	}

    /// @brief Adds tasks to the thread pool
    /// @param tasks An array of tasks to be added
    /// @param num_tasks The number of tasks in the array
	void detach_tasks(std::function<void()>* tasks, size_t num_tasks) {
		std::unique_lock<std::mutex> lock(data_lock);
		for (size_t i = 0; i < num_tasks; i++) {
			jobs.push({std::move(tasks[i])});
		}
		cv.notify_all();
	}

   private:
	struct Job {
		Job(std::function<void()>&& job) : job(std::move(job)) {}
		Job() = delete;
		std::function<void()> job;
	};

	void worker_loop() {
		while (running.load()) {
			std::unique_lock<std::mutex> lock(data_lock);
			cv.wait(lock, [this] { return !jobs.empty() || !running.load(); });
			if (!running.load()) {
				return;
			}
			Job job = std::move(jobs.front());
			jobs.pop();
			lock.unlock();
			job.job();
		}
	}

	std::atomic<bool> running = true;
	std::vector<std::thread> threads;
	std::queue<Job> jobs;
	std::mutex data_lock;
	std::condition_variable cv;
};

#endif  // SIMULATOR_THREADPOOL_H

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
#include <semaphore>
#include <span>
#include <thread>

/*
 * University of Michigan Solar Car Team
 *
 * Light weight C++20 thread pool implementation
 *
 *
 * High level API:
 *
 * There are two APIs for adding tasks to the thread pool: blocking and non-blocking.
 *
 * The non-blocking API is used when you want to add a task to the thread pool and continue execution.
 *
 * The blocking API is used when you want to add a task to the thread pool and wait for it to finish.
 *
 * Within the non-blocking API, there are two functions:
 *      detach_task - used to add a single task to the thread pool
 *      detach_tasks - used to add multiple tasks to the thread pool
 *
 * Within the blocking API, there are two functions:
 *      run_tasks - used to add multiple tasks to the thread pool and wait for them to finish
 *      run_loop - used to add a loop of tasks to the thread pool and wait for them to finish (syntax sugar)
 *
 */

template <typename>
struct extract_argument;

template <typename T>
struct extract_argument<std::function<void(T)>> {
	using type = T;
};

template <>
struct extract_argument<std::function<void()>> {
	using type = void;
};

template <typename Func>
using extract_argument_t = typename extract_argument<Func>::type;

class ThreadPool {
   public:
	ThreadPool() {
		reset(std::thread::hardware_concurrency());
	}

	/// @brief  Construct a thread pool with a specific number of threads
	/// @param num_threads Number of threads for the pool
	explicit ThreadPool(size_t num_threads) {
		reset(num_threads);
	}

	~ThreadPool() {
		running = false;
		cv.notify_all();
		for (auto& thread : threads) {
			thread.join();
		}
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
	size_t size() const {
		return threads.size();
	}

	// ********** Non-blocking API **********

	/// @brief Add a task to the thread pool
	/// @param task The task to be added
	void detach_task(std::function<void()>&& task) {
		{
			std::scoped_lock<std::mutex> lock(data_lock);
			jobs.emplace(std::move(task));
		}
		cv.notify_one();
	}

	/// @brief Adds tasks to the thread pool
	/// @param tasks A container of tasks to be added (not valid after this function returns)
	void detach_tasks(std::span<std::function<void()>> tasks) {
		{
			std::scoped_lock<std::mutex> lock(data_lock);
			for (auto& task : tasks) {
				jobs.emplace(std::move(task));
			}
		}
		cv.notify_all();
	}

	// ********** Blocking API **********

	/// @brief Adds tasks to the thread pool and waits for them to finish
	/// @param tasks A container of tasks to be added (not valid after this function returns)
	void run_tasks(std::span<std::function<void()>> tasks) {
		std::counting_semaphore sem(0);
		{
			std::scoped_lock<std::mutex> lock(data_lock);
			for (auto& task : tasks) {
				jobs.emplace([&]() {
					task();
					sem.release();
				});
			}
		}
		cv.notify_all();
		for (size_t i = 0; i < tasks.size(); i++) {
			sem.acquire();
		}
	}

	/// @brief Adds tasks to the thread pool and waits for them to finish
	/// @param start The start index of the loop
	/// @param end The end index of the loop
	/// @param loop_body The body of the loop
	///
	/// This is equivalent to:
	///
	/// for (size_t i = start; i < end; i++) {
	///		 loop_body();
	/// }
	template <typename Func, typename arg_type = extract_argument_t<Func>>
		requires(std::is_invocable_r_v<void, Func, arg_type> && std::is_integral_v<arg_type>) ||
				std::is_invocable_r_v<void, Func>
	void run_loop(size_t start, size_t end, Func&& loop_body) {
		using induction_type = std::conditional_t<std::is_invocable_v<Func>, size_t, arg_type>;
		std::counting_semaphore sem(0);
		{
			std::scoped_lock<std::mutex> lock(data_lock);
			for (induction_type i = start; i < end; ++i) {
				jobs.emplace([&, i]() {
					if constexpr (std::is_invocable_v<Func>) {
						loop_body();
					} else {
						loop_body(i);
					}
					sem.release();
				});
			}
		}
		cv.notify_all();
		for (size_t i = start; i < end; i++) {
			sem.acquire();
		}
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

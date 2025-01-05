# ThreadPool

UMSCT's in house C++20 thread pool library. This library is designed to be lightweight and easy to use. It is not meant to be a full-fledged thread pool library, but rather a simple and efficient way to detach tasks to a pool of threads.

## Basic Usage

```cpp
#include <vector>
#include <iostream>
#include <vector>
#include <functional>

#include "ThreadPool.hpp"


int main() {
    ThreadPool pool(4); // Create a thread pool with 4 threads

    // Detach a task
    pool.detach_task([]() {
        std::cout << "Hello, World!" << std::endl;
    });

    // Detach a batch of tasks
    std::vector<std::function<void()>> tasks;
    for (int i = 0; i < 10; i++) {
        tasks.push_back([i]() {
            std::cout << "Hello, World! " << i << std::endl;
        });
    }

    // This is more efficient than detaching each task individually
    pool.detach_tasks(std::move(tasks));

    // Detach a task with other containers
    std::function<void()> arr_tasks[] = {
        []() {
            std::cout << "Hello, World!" << std::endl;
        },
        []() {
            std::cout << "Hello, World!" << std::endl;
        }
    };

    pool.detach_tasks(arr_tasks, 2);
}
```

## Advanced Usage

This lightweight thread pool does not include any synchronization mechanisms. If you need to synchronize tasks or access shared resources, you will need to use your own synchronization mechanisms. Furthermore, if you need to wait for all tasks to finish, you will have to do so manually within your lambda function.

Here is an example of how to synchronize tasks:

```cpp
#include <semaphore>
#include <mutex>
#include <vector>
#include <iostream>

#include "ThreadPool.hpp"


int main() {
    ThreadPool pool(4); // Create a thread pool with 4 threads

    std::mutex mtx;
    std::counting_semaphore sem(0);

    // Generate a batch of tasks
    std::vector<std::function<void()>> tasks;
    for (int i = 0; i < 10; i++) {
        tasks.push_back([&]() {
            {
                std::lock_guard<std::mutex> lock(mtx);
                std::cout << "Hello, World! " << i << std::endl;
            }
            
            sem.release();
        });
    }

    // Detach the tasks
    pool.detach_tasks(std::move(tasks));

    // Wait for all tasks to finish
    for (int i = 0; i < 10; i++) {
        sem.acquire();
    }
}


```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. We highly encourage you to contribute to this project by submitting pull requests or creating issues.

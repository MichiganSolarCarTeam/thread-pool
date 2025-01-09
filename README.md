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
    // This is more efficient than detaching each task individually
    {
        std::vector<std::function<void()>> tasks;
        for (int i = 0; i < 10; i++) {
            tasks.push_back([i]() {
                std::cout << "Hello, World! " << i << std::endl;
            });
        }
        pool.detach_tasks(std::move(tasks));
    }
    
    // Wait for tasks to finish
    // Detach a batch of tasks
    // This is more efficient than detaching each task individually
    {
        std::vector<std::function<void()>> tasks;
        for (int i = 0; i < 10; i++) {
            tasks.push_back([i]() {
                std::cout << "Hello, World! " << i << std::endl;
            });
        }
        pool.run_tasks(std::move(tasks)); // block until all tasks are done

        // You could have also done this
        auto loop = tasks.push_back([](size_t i) {
            std::cout << "Hello, World! " << i << std::endl; 
        });
        pool.run_loop(0, 10, std::function(loop)); // block until all tasks are done
    }
    

}
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. We highly encourage you to contribute to this project by submitting pull requests or creating issues.

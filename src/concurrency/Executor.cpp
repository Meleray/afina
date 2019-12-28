#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {
  void perform(Executor *executor) {
    while (executor->state == Executor::State::kRun) {
      std::function<void()> exec;
      {
        std::unique_lock<std::mutex> lock(executor->mutex);
        auto border = std::chrono::system_clock::now() + std::chrono::milliseconds(executor->idle_time);
        while (executor->tasks.size() == 0) {
          ++executor->num_threads;
          if (executor->empty_condition.wait_until(lock, border) == std::cv_status::timeout) {
              if (executor->threads.size() > executor->low_watermark) {
                std::thread::id id = std::this_thread::get_id();
                std::vector<std::thread>::iterator it;
                while (it != executor->threads.end() && id != it->get_id()) {
                  ++it;
                }
                if (it != executor->threads.end()) {
                  --executor->num_threads;
                  it->detach();
                  executor->threads.erase(it);
                }
                return;
            }
          } else {
            executor->empty_condition.wait(lock);
          }
          --executor->num_threads;
        }
        exec = executor->tasks.front();
        executor->tasks.pop_front();
      }
      exec();
    }
    {
      std::unique_lock<std::mutex> lock(executor->mutex);
      std::thread::id id = std::this_thread::get_id();
      std::vector<std::thread>::iterator it;
      while (it != executor->threads.end() && id != it->get_id()) {
        ++it;
      }
      if (it != executor->threads.end()) {
        --executor->num_threads;
        it->detach();
        executor->threads.erase(it);
      }
      if (executor->threads.empty()) {
        executor->stopped.notify_all();
      }
    }
  }

  void Executor::Start() {
    state = State::kRun;
    std::unique_lock<std::mutex> lock(mutex);
    for (int i = 0; i < low_watermark; ++i) {
      threads.push_back(std::thread(perform, this));
    }
    num_threads = 0;
  }

  void Executor::Stop(bool await) {
    state = State::kStopping;
    empty_condition.notify_all();
    if (await) {
      std::unique_lock<std::mutex> lock(mutex);
        while (!threads.empty()) {
          stopped.wait(lock);
        }
    }
    state = State::kStopped;
  }
}
} // namespace Afina

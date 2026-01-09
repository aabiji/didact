#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <stop_token>
#include <vector>

template <typename T> class SampleQueue {
public:
  bool have_enough(int amount) {
    std::unique_lock<std::mutex> guard(m_mutex);
    return amount < m_data.size();
  }

  // Push to the queue, which doesn't have a maximum size
  void push_samples(T* samples, int num_samples) {
    std::unique_lock<std::mutex> guard(m_mutex);
    m_data.insert(m_data.end(), samples, samples + num_samples);
  }

  bool is_empty() {
    std::unique_lock<std::mutex> guard(m_mutex);
    return m_data.empty();
  };

  // Wait until there are enough samples in the queue, then pop
  std::vector<T> pop_samples(int num_samples, std::stop_token token = {}) {
    std::unique_lock<std::mutex> guard(m_mutex);
    std::vector<T> output(num_samples, T{});

    auto lambda = [&] { return num_samples <= m_data.size(); };
    if (!m_not_empty.wait(guard, token, lambda))
      return output; // Stopped waiting because a stop was requested

    int size = std::min(num_samples, (int)m_data.size());
    std::copy(m_data.begin(), m_data.begin() + size, output.begin());
    m_data.erase(m_data.begin(), m_data.begin() + size);
    return output;
  }

private:
  std::deque<T> m_data;
  std::mutex m_mutex;
  std::condition_variable_any m_not_empty;
};

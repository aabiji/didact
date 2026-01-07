#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <stop_token>
#include <vector>

template <typename T> class SampleQueue {
public:
  void init(int size) { m_max_samples = size; }

  bool is_full() {
    std::unique_lock<std::mutex> guard(m_mutex);
    return m_data.size() == m_max_samples;
  }

  bool have_enough(int amount) {
    std::unique_lock<std::mutex> guard(m_mutex);
    return amount < m_data.size();
  }

  void push_samples(T* samples, int num_samples, std::stop_token token = {}) {
    std::unique_lock<std::mutex> guard(m_mutex);

    auto lambda = [&] { return m_data.size() + num_samples <= m_max_samples; };
    if (!m_cond_not_full.wait(guard, token, lambda))
      return; // Stopped waiting because a stop was requested

    m_data.insert(m_data.end(), samples, samples + num_samples);
    m_cond_not_empty.notify_all();
  }

  std::vector<T> pop_samples(int num_samples, std::stop_token token = {}) {
    std::unique_lock<std::mutex> guard(m_mutex);

    std::vector<T> output(num_samples, T{});
    auto lambda = [&] { return num_samples <= m_data.size(); };
    if (!m_cond_not_empty.wait(guard, token, lambda))
      return output; // Stopped waiting because a stop was requested

    int size = std::min(num_samples, (int)m_data.size());
    std::copy(m_data.begin(), m_data.begin() + size, output.begin());
    m_data.erase(m_data.begin(), m_data.begin() + size);

    m_cond_not_full.notify_all();
    return output;
  }

private:
  int m_max_samples;
  std::deque<T> m_data;

  std::mutex m_mutex;
  std::condition_variable_any m_cond_not_full;
  std::condition_variable_any m_cond_not_empty;
};

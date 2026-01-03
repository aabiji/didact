#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <stdlib.h>
#include <vector>

using Lock = std::shared_mutex;
using WriteLock = std::unique_lock<Lock>;
using ReadLock = std::shared_lock<Lock>;
using ConditionVar = std::condition_variable_any;

template <typename T> class SampleQueue {
public:
  void init(int size) { m_max_samples = size; }

  bool is_full() {
    ReadLock read_lock(m_lock);
    return m_data.size() == m_max_samples;
  }

  bool have_enough(int amount) {
    ReadLock read_lock(m_lock);
    return amount < m_data.size();
  }

  void push_samples(T* samples, int num_samples, bool wait = false) {
    WriteLock write_lock(m_lock);
    if (wait) {
      auto lambda = [&] { return m_data.size() + num_samples < m_max_samples; };
      m_cond_not_full.wait(write_lock, lambda);
    }
    m_data.insert(m_data.end(), samples, samples + num_samples);
  }

  std::vector<T> pop_samples(int num_samples, bool wait = false) {
    ReadLock read_lock(m_lock);
    if (wait) {
      auto lambda = [&] { return num_samples < m_data.size(); };
      m_cond_not_empty.wait(read_lock, lambda);
    }

    int size = std::min(num_samples, int(m_data.size()));
    std::vector<T> output(size);
    std::copy(m_data.begin(), m_data.begin() + size, output.begin());
    m_data.erase(m_data.begin(), m_data.begin() + size);
    return output;
  }

private:
  int m_max_samples;
  std::deque<T> m_data;

  Lock m_lock;
  ConditionVar m_cond_not_full;
  ConditionVar m_cond_not_empty;
};

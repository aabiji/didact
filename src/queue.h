#pragma once

#include <deque>
#include <mutex>
#include <shared_mutex>
#include <stdlib.h>
#include <vector>

using Lock = std::shared_mutex;
using WriteLock = std::unique_lock<Lock>;
using ReadLock = std::shared_lock<Lock>;

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

  void push_samples(T *samples, int num_samples) {
    WriteLock write_lock(m_lock);
    if (m_data.size() == m_max_samples)
      return;
    m_data.insert(m_data.end(), samples, samples + num_samples);
  }

  std::vector<T> pop_samples(int num_samples) {
    ReadLock read_lock(m_lock);
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
};

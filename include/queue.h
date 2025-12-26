#pragma once

#include <deque>
#include <mutex>
#include <shared_mutex>
#include <stdlib.h>
#include <vector>

using Lock = std::shared_mutex;
using WriteLock = std::unique_lock<Lock>;
using ReadLock = std::shared_lock<Lock>;

class SampleQueue {
public:
  void init(int size) { m_max_samples = size; }

  bool is_full() {
    ReadLock read_lock(m_lock);
    return m_data.size() == m_max_samples;
  }

  bool empty() {
    ReadLock read_lock(m_lock);
    return m_data.empty();
  }

  void push_samples(int16_t *samples, int num_samples) {
    WriteLock write_lock(m_lock);
    if (m_data.size() == m_max_samples)
      return;
    m_data.insert(m_data.end(), samples, samples + num_samples);
  }

  std::vector<int16_t> pop_samples(int num_samples) {
    ReadLock read_lock(m_lock);
    std::vector<int16_t> output(num_samples, 0);
    std::copy(m_data.begin(), m_data.begin() + num_samples, output.begin());
    m_data.erase(m_data.begin(), m_data.begin() + num_samples);
    return output;
  }

private:
  int m_max_samples;
  std::deque<int16_t> m_data;
  Lock m_lock;
};

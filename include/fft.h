#pragma once

#include <mutex>
#include <shared_mutex>
#include <stdint.h>
#include <vector>

using Lock = std::shared_mutex;
using WriteLock = std::unique_lock<Lock>;
using ReadLock = std::shared_lock<Lock>;

class SpectrumAnalyzer {
public:
  SpectrumAnalyzer(int sample_rate);

  void process(int16_t *samples, int length);
  std::vector<float> get_bars();

private:
  void get_frequency_bins();
  void map_bins_to_bars();
  void fill_input_buffer(int16_t *data, int size);

  int m_num_bars;
  int m_sample_rate;
  int m_write_offset;
  int m_input_size;

  Lock m_lock; // protects m_input_buffer
  std::vector<int16_t> m_input_buffer;
  std::vector<float> m_frequency_bins;
  std::vector<float> m_bars;
};

#pragma once

#include <deque>
#include <mutex>
#include <shared_mutex>
#include <stdint.h>
#include <vector>

using Lock = std::shared_mutex;
using WriteLock = std::unique_lock<Lock>;
using ReadLock = std::shared_lock<Lock>;

struct FFTFrame {
  std::vector<float> bars;
  int num_samples_till_now;
};

class SpectrumAnalyzer {
public:
  SpectrumAnalyzer(int sample_rate);

  void process(int16_t *samples, int length, int total_samples);
  std::vector<float> get_bars(int start_sample_offset);

private:
  void get_frequency_bins();
  void map_bins_to_bars(int total_samples);
  void fill_input_buffer(int16_t *data, int size);

  int m_num_bars;
  int m_sample_rate;
  int m_write_offset;
  int m_input_size;

  Lock m_lock; // protects m_frames
  std::vector<int16_t> m_input_buffer;
  std::vector<float> m_frequency_bins;
  std::deque<FFTFrame> m_frames;
};

#pragma once

#include <stdint.h>
#include <vector>

using float_vec = std::vector<float>;

class SpectrumAnalyzer {
public:
  void init(int sample_rate);

  float_vec process(int16_t* samples, size_t length);

private:
  void fill_input_buffer(int16_t* data, size_t size);
  float_vec get_frequency_bins();
  float_vec map_bins_to_bars(float_vec bins);

  int m_sample_rate;
  size_t m_num_bars;
  size_t m_write_offset;
  size_t m_input_size;
  std::vector<int16_t> m_input_buffer;
};

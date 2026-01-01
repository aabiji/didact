#pragma once

#include <stdint.h>
#include <vector>

using float_vec = std::vector<float>;

class SpectrumAnalyzer {
public:
  SpectrumAnalyzer(int sample_rate);

  float_vec process(int16_t* samples, int length);

private:
  void fill_input_buffer(int16_t* data, int size);
  float_vec get_frequency_bins();
  float_vec map_bins_to_bars(float_vec bins);

  int m_num_bars;
  int m_sample_rate;
  int m_write_offset;
  int m_input_size;
  std::vector<int16_t> m_input_buffer;
};

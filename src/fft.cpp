#include <algorithm>
#include <assert.h>
#include <complex>

#include "fft.h"

using complex = std::complex<float>;
using cdata = std::vector<complex>;
const float pi = std::numbers::pi;

// The Fast Fourrier Transform algorithm.
// The implementation was derived from here:
// https://cp-algorithms.com/algebra/fft.html
cdata fft(cdata data, bool inverse) {
  int N = data.size();
  cdata points(N);

  // Number of times the input would have been split in half
  int stages = std::log2(N);

  // Reordering the input into a "bit-reversed" order pre-sorts
  // the data so that the input's recursive even/odd decomposition
  // can be done iteratively from the bottom up.
  for (int i = 0; i < N; i++) {
    int reversed = 0;
    for (int j = 0; j < stages; j++) {
      if ((i >> j) & 1)
        reversed |= (1 << (stages - j - 1));
    }
    points[reversed] = data[i];
  }

  // Process the data in stages...
  for (int len = 2; len <= N; len *= 2) {
    // Calculate the "twiddle factor" step for this stage.
    // Forward FFT uses e^(-i*2pi/len), inverse uses e^(i*2pi/len).
    float angle = (inverse ? -1 : 1) * (2.0 * pi / len);
    complex factor = std::polar(1.0f, angle);
    int mid = len / 2;

    // Step through each block of size "len"
    for (int i = 0; i < N; i += len) {
      complex w = complex(1.0, 0.0);

      // Pairwise combine elements from the lower and upper halves of the block
      for (int j = 0; j < mid; j++) {
        complex even = points[i + j];
        complex odd = w * points[i + j + mid];
        points[i + j] = even + odd;
        points[i + j + mid] = even - odd;
        w *= factor;
      }
    }
  }

  // The inverse fourrier transform requires dividing by N
  if (inverse) {
    for (int i = 0; i < N; i++)
      points[i] /= N;
  }

  return points;
}

SpectrumAnalyzer::SpectrumAnalyzer(int sample_rate) {
  m_num_bars = 32;
  m_sample_rate = sample_rate;
  m_write_offset = 0;
  m_input_size = 1024;

  m_input_buffer.resize(m_input_size, 0);
  m_frequency_bins.resize(m_input_size / 2, 0);
  m_bars.resize(m_num_bars, 0);
}

std::vector<float> SpectrumAnalyzer::get_bars() {
  ReadLock read_lock(m_lock);
  return m_bars;
}

void SpectrumAnalyzer::process(int16_t *samples, int length) {
  WriteLock write_lock(m_lock);
  fill_input_buffer(samples, length);
  get_frequency_bins();
  map_bins_to_bars();
}

void SpectrumAnalyzer::fill_input_buffer(int16_t *data, int size) {
  // Ensure the input buffer is big enough for the new data. Also ensure
  // that the new input size is a power of 2, so that the FFT works correctly.
  if (size > m_input_size) {
    double l = std::log(size) / std::log(2);
    m_input_size = std::pow(2, std::ceil(l));
    m_input_buffer.resize(m_input_size, 0);
    m_frequency_bins.resize(m_input_size / 2, 0);
  }

  int overflow = m_write_offset + size - m_input_size;
  if (overflow > 0) {
    // Shift the buffer to the left to make space for the new data
    std::move(m_input_buffer.begin() + overflow, m_input_buffer.end(),
              m_input_buffer.begin());
    m_write_offset -= overflow;
  }

  // Fill the sliding input buffer with new samples
  std::copy(data, data + size, m_input_buffer.begin() + m_write_offset);
  m_write_offset += size;
}

void SpectrumAnalyzer::get_frequency_bins() {
  // Apply a Hamming Window, which will reduce the noise in the FFT output
  // (spectral leakage) caused by sharp discontinuities in the input
  cdata preprocessed(m_input_size, complex(0, 0));
  float window_const = 2.0 * pi / (m_input_size - 1);
  for (int i = 0; i < m_input_size; i++) {
    float hamming = 0.54 - 0.46 * std::cos(window_const * i);
    preprocessed[i] = complex(float(m_input_buffer[i]) * hamming);
  }

  cdata output = fft(preprocessed, false);

  // Since the input is purely real, we can ignore the
  // upper half of the output, which is just a mirror image
  int mid = m_input_size / 2;
  for (int i = 0; i < mid; i++) {
    float scale =
        i == 0 ? 1.0 / float(m_input_size) : 2.0 / float(m_input_size);
    float magnitude = std::abs(output[i]) * scale;
    m_frequency_bins[i] = magnitude;
  }
}

void SpectrumAnalyzer::map_bins_to_bars() {
  float bin_frequency_spacing = float(m_sample_rate) / float(m_input_size);
  float min_freq = 40, max_freq = float(m_sample_rate) / 2.0;
  float ratio = max_freq / min_freq;

  // Logarithmically map peak magnitudes to the set of bars.
  // This is because human hearing is Logarithmic: going from 100 Hz
  // 1000 Hz sounds as if the frequency were going from 10 Hz to 20 Hz
  for (int i = 0; i < m_num_bars; i++) {
    // Each bar will cover a larger and larger frequency range
    float freq_start = min_freq * std::pow(ratio, float(i) / float(m_num_bars));
    float freq_end =
        min_freq * std::pow(ratio, float(i + 1) / float(m_num_bars));

    int bin_start = std::floor(freq_start / bin_frequency_spacing);
    int bin_end = std::floor(freq_end / bin_frequency_spacing);
    auto peak_it = std::max_element(m_frequency_bins.begin() + bin_start,
                                    m_frequency_bins.begin() + bin_end);
    m_bars[i] = *peak_it;
  }
}

#include <algorithm>
#include <complex>

#include "analyzer.h"

using complex = std::complex<float>;
using cdata = std::vector<complex>;
const float pi = std::numbers::pi_v<float>;

// The Fast Fourrier Transform algorithm.
// The implementation was derived from here:
// https://cp-algorithms.com/algebra/fft.html
cdata fft(cdata data, bool inverse) {
  size_t N = data.size();
  cdata points(N);

  // Number of times the input would have been split in half
  int stages = (int)std::floor(std::log2(N));

  // Reordering the input into a "bit-reversed" order pre-sorts
  // the data so that the input's recursive even/odd decomposition
  // can be done iteratively from the bottom up.
  for (size_t i = 0; i < N; i++) {
    size_t reversed = 0;
    for (int j = 0; j < stages; j++) {
      if ((i >> j) & 1)
        reversed |= (1 << (stages - j - 1));
    }
    points[reversed] = data[i];
  }

  // Process the data in stages...
  for (size_t len = 2; len <= N; len *= 2) {
    // Calculate the "twiddle factor" step for this stage.
    // Forward FFT uses e^(-i*2pi/len), inverse uses e^(i*2pi/len).
    float angle = (inverse ? -1 : 1) * (2.0f * pi / (float)len);
    complex factor = std::polar(1.0f, angle);
    size_t mid = len / 2;

    // Step through each block of size "len"
    for (size_t i = 0; i < N; i += len) {
      complex w = complex(1.0, 0.0);

      // Pairwise combine elements from the lower and upper halves of the block
      for (size_t j = 0; j < mid; j++) {
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
    for (size_t i = 0; i < N; i++)
      points[i] /= (float)N;
  }

  return points;
}

SpectrumAnalyzer::SpectrumAnalyzer(int sample_rate) {
  m_num_bars = 32;
  m_sample_rate = sample_rate;
  m_write_offset = 0;
  m_input_size = 1024;
}

float_vec SpectrumAnalyzer::process(int16_t* samples, size_t length) {
  // Apply a crude noise gate:
  // Clear the input buffer if the input only coantains "silence"
  bool silent = true;
  float threshold = 150.0f;
  for (size_t i = 0; i < length; i++) {
    float value = (float)std::abs(samples[i]);
    if (value > threshold) {
      silent = false;
      break;
    }
  }

  if (silent) {
    std::fill(m_input_buffer.begin(), m_input_buffer.end(), 0);
    m_write_offset = 0;
    return float_vec(m_num_bars, 0.0f);
  }

  fill_input_buffer(samples, length);
  auto bins = get_frequency_bins();
  return map_bins_to_bars(bins);
}

void SpectrumAnalyzer::fill_input_buffer(int16_t* data, size_t size) {
  // Ensure the input buffer is big enough for the new data. Also ensure
  // that the new input size is a power of 2, so that the FFT works correctly.
  if (size > m_input_size) {
    double l = std::log(size) / std::log(2);
    m_input_size = (size_t)std::floor(std::pow(2, std::ceil(l)));
  }

  if (m_input_buffer.size() != m_input_size)
    m_input_buffer.resize(m_input_size, 0);

  // Fill up to the end of the buffer
  size_t initial_size =
      m_write_offset + size < m_input_size ? size : m_input_size - m_write_offset;
  std::copy(data, data + initial_size, m_input_buffer.begin() + (long)m_write_offset);
  m_write_offset += initial_size;

  // Wrap back around and copy the reminaing data
  size_t remaining = size - initial_size;
  if (remaining > 0) {
    std::copy(data + initial_size, data + size, m_input_buffer.begin());
    m_write_offset = remaining;
  }
}

float_vec SpectrumAnalyzer::get_frequency_bins() {
  // Apply a Hamming Window, which will reduce the noise in the FFT output
  // (spectral leakage) caused by sharp discontinuities in the input
  cdata preprocessed(m_input_size, complex(0, 0));
  float window_const = 2.0f * pi / ((float)m_input_size - 1);
  for (size_t i = 0; i < m_input_size; i++) {
    size_t index = (m_write_offset + i) % m_input_size;
    float hamming = 0.54f - 0.46f * std::cos(window_const * (float)i);
    preprocessed[i] = complex((float)m_input_buffer[index] * hamming);
  }

  cdata output = fft(preprocessed, false);

  // Since the input is purely real, we can ignore the
  // upper half of the output, which is just a mirror image
  size_t mid = m_input_size / 2;
  float_vec frequency_bins(mid);
  for (size_t i = 0; i < mid; i++) {
    float scale = i == 0 ? 1.0f / (float)m_input_size : 2.0f / (float)m_input_size;
    float magnitude = std::abs(output[i]) * scale;
    // Apply a crude noise floor:
    frequency_bins[i] = magnitude < 15.0f ? 0.0f : magnitude;
  }

  return frequency_bins;
}

float_vec SpectrumAnalyzer::map_bins_to_bars(float_vec frequency_bins) {
  float bin_frequency_spacing = float(m_sample_rate) / float(m_input_size);
  float min_freq = 40, max_freq = (float)m_sample_rate / 2.0f;
  float ratio = max_freq / min_freq;

  float_vec bars(m_num_bars);

  // Logarithmically map peak magnitudes to the set of bars.
  // This is because human hearing is Logarithmic: going from 100 Hz
  // 1000 Hz sounds as if the frequency were going from 10 Hz to 20 Hz
  for (size_t i = 0; i < m_num_bars; i++) {
    // Each bar will cover a larger and larger frequency range
    float freq_start = min_freq * std::pow(ratio, float(i) / float(m_num_bars));
    float freq_end = min_freq * std::pow(ratio, float(i + 1) / float(m_num_bars));

    int bin_start = (int)std::floor(freq_start / bin_frequency_spacing);
    int bin_end = (int)std::floor(freq_end / bin_frequency_spacing);
    auto peak_it = std::max_element(frequency_bins.begin() + bin_start,
                                    frequency_bins.begin() + bin_end);
    bars[i] = *peak_it;
  }

  return bars;
}

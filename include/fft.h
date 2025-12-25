#pragma once

#include <stdint.h>
#include <vector>

#ifndef FFT_IMPLEMENTATION
// Get the amplitudes of each of the frequency components in the input signal
std::vector<double> analyze_spectrum(int16_t* samples, int num_samples);
#else
#include <algorithm>
#include <complex>
#include <numbers>

using complex = std::complex<double>;
using cdata = std::vector<complex>;
const double pi = std::numbers::pi;

// The Fast Fourrier Transform algorithm.
// The implementation was derived from here:
// https://cp-algorithms.com/algebra/fft.html
cdata FFT(cdata data, bool inverse) {
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
    double angle = (inverse ? -1 : 1) * (2.0 * pi / len);
    complex factor = std::polar(1.0, angle);
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

std::vector<double> analyze_spectrum(int16_t* samples, int num_samples) {
  // Pad the input with zeroes so that its size will be a power of two
  double l = std::log(num_samples) / std::log(2);
  int size = std::pow(2, std::ceil(l));
  cdata input(size, complex(0, 0));

  // Apply a Hamming Window, which will reduce the noise in the FFT output
  // (spectral leakage) caused by sharp discontinuities in the input
  double window_const = 2.0 * pi / (num_samples - 1);
  for (int i = 0; i < num_samples; i++) {
    double hamming = 0.54 - 0.46 * std::cos(window_const * i);
    input[i] = complex(double(samples[i]) * hamming);
  }

  cdata output = FFT(input, false);

  // Since the input is purely real, we can ignore the
  // upper half of the output, which is just a mirror image
  int mid = size / 2;
  std::vector<double> frequency_bins(mid);

  for (int i = 0; i < mid; i++) {
    double scale = i == 0 ? 1.0 / double(size) : 2.0 / double(size);
    double amplitude = std::abs(output[i]) * scale;
    frequency_bins[i] = amplitude;
  }
  return frequency_bins;
}
#endif
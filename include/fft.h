#pragma once

#include <stdint.h>
#include <vector>

#ifndef FFT_IMPLEMENTATION
// Get the amplitudes of each of the frequency components in the input signal
std::vector<double> seperate_frequencies(int16_t* samples, int num_samples);
#else
#include <complex>
#include <numbers>

using complex = std::complex<double>;
using cdata = std::vector<complex>;
const double PI = std::numbers::pi;
const complex I = complex(0.0, 1.0);

// The Fast Fourrier Transform algorithm
cdata FFT(cdata points, bool forward) {
  if (points.size() == 1)
    return points;

  int n = points.size();
  int mid = std::floor(double(n) / 2.0);

  cdata p_even(mid), p_odd(mid);
  for (int i = 0; i < n; i++) {
    int idx = std::floor(double(i) / 2.0);
    if (i % 2 == 0)
      p_even[idx] = points[i];
    else
      p_odd[idx] = points[i];
  }

  cdata y_even = FFT(p_even, forward);
  cdata y_odd = FFT(p_odd, forward);
  cdata output(n);

  int direction = forward ? -1 : 1;
  complex gamma = std::exp((2.0 * PI * I * direction) / double(n));
  complex w(1.0, 0.0);  // root of unity

  for (int i = 0; i < mid; i++) {
    output[i] = y_even[i] + w * y_odd[i];
    output[i + mid] = y_even[i] - w * y_odd[i];
    w *= gamma;
  }
  return output;
}

std::vector<double> seperate_frequencies(int16_t* samples, int num_samples) {
  // Pad the input with zeroes so that its size will be a power of two
  double l = std::log(num_samples) / std::log(2);
  int input_size = std::pow(2, std::ceil(l));
  int mid = floor(double(input_size) / 2.0);
  cdata input(input_size, complex(0, 0));

  // Convert the samples into the needed input format and apply a Hamming
  // Window. The Hamming Window will reduce the noise in the FFT output
  // (spectral leakage) caused by sharp discontinuities in the input
  for (int i = 0; i < input_size; i++) {
    double value = 0.0;
    if (i < num_samples) {
      double factor =
          0.54 - 0.46 * std::cos((2.0 * PI * i) / (num_samples - 1));
      value = double(samples[i]) * factor;
    }
    input[i] = complex(value, 0);
  }

  cdata output = FFT(input, true);

  // Get the amplitude of each of the frequency components.
  // The upper half of the output is just a mirror image of the
  // lower half of the output, so it can be ignored.
  // NOTE: Also ignoring the Nyquist frequency
  std::vector<double> frequency_bins(mid);
  for (int i = 0; i < mid - 1; i++) {
    complex c = output[i];
    double magnitude = sqrt(c.real() * c.real() + c.imag() * c.imag());

    // The magnitude is scaled by the number of points,
    // and power is split between the positive and negative components (for non
    // DC/Nyquist points), so scaling must be done.
    double amplitude = i == 0 ? magnitude / double(input_size)
                              : (magnitude * 2.0) / double(input_size);
    frequency_bins[i] = amplitude;
  }

  return frequency_bins;
}
#endif
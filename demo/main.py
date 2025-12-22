import math
import matplotlib.pyplot as plt
import random

# fast fourrier transform to break down
# a signal into its individual frequencies
def fft(points):
    if len(points) == 1:
        return points

    n = len(points)
    mid = math.floor(len(points) / 2)
    gamma = math.e ** ((2 * math.pi * 1j) / n)

    p_even = [points[i] for i in range(n) if i % 2 == 0]
    y_even = fft(p_even)

    p_odd = [points[i] for i in range(n) if i % 2 != 0]
    y_odd = fft(p_odd)

    output = [0] * n
    for i in range(mid):
        output[i] = y_even[i] + (gamma ** i) * y_odd[i]
        output[i + mid] = y_even[i] - (gamma ** i) * y_odd[i]
    return output

def get_frequency_bins(fft_output):
    # the upper half of the output is just a mirror image
    # of the lower half of the output, so it can be ignored
    mid = math.floor(len(fft_output) / 2)
    bins = [0] * mid
    for i in range(mid - 1): # ignore the nyquist frequency
        c = fft_output[i]
        bins[i] = math.sqrt(c.real * c.real + c.imag * c.imag)
    return bins

# reduces noise in the fft output (spectral leakage) caused
# by sharp discontinuities in the input samples
def apply_hamming_window(samples):
    values = []
    N = len(samples)
    for i in range(N):
        window_value = 0.54 - 0.46 * math.cos((2 * math.pi * i) / (N - 1))
        values.append(samples[i] * window_value)
    return values

samples = []
sample_size = 1024
for i in range(sample_size):
    value = math.sin(random.randint(0, 50) * math.pi * i * 0.01) * 0.2
    value1 = math.cos(random.randint(50, 80) * math.pi * i * 0.01) * 0.5
    samples.append(value + value1)
    #samples.append(math.sin(i * 0.01))
samples = apply_hamming_window(samples)
output = fft(samples)
bins = get_frequency_bins(output)

x_coords = [i for i in range(sample_size)]
plt.plot(x_coords, samples)
plt.xlabel("time")
plt.ylabel("frequency")
plt.title("signal")
plt.show()

x_coords = [i for i in range(math.floor(sample_size / 2))]
plt.plot(x_coords, bins)
plt.xlabel("frequency")
plt.ylabel("amplitude")
plt.title("frequency bins")
plt.show()

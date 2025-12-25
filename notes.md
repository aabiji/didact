# Notes on [fftviz](https://github.com/gursi26/fftviz)

- They're mirroring the frequency on both sides
  (high -> low, low -> high) to get a nice symmetric effect
- actual fps vs fft fps (which is smaller)
  trying to keep the fft in sync with the audio

Processing steps:
- FFT
- Amplitudes
- Logarithic binning
- Frequency normalization
- Intensity normalization
- Interpolation

# Normalization

Intensity normalization:
For each amplitude, if the amplitude falls below a treshold,
multiply it by a scaling factor.
The thresholds would look like so: [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]
The scaling factors would look like so: [0.4, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.6, 0.5]
On the low end, it eliminates amplitudes that are too low.
On the high end, it caps amplitudes that are too high.
It leaves the middle range the same.

Frequency normalization:
Apply a scaling factor to a frequency zone, depending on
which zone the frquency belongs to:

The scaling factors would look something like this:
[0.9, 1.2, 1.2, 1.2, 1.0]
Each index = 1 zone

Certain zones of the frequency domain will be more
emphasized than others. This is to remove "noise" from
the fft output. We want to reduce the strength of the lowest
and highest frequencies, since they're not very important to begin with.

# Interpolation
Moving average:
For each sample, get the average of the n samples around it:
new_fft_sample = (i - n, i + n - 1) / (n * 2)
Don't apply this smoothing to the first and last n frames

Both temporal and spatial interpolation are doing the same thing.
Getting the intermediate value between two values

Temporal interpolation -> Between the sample from 2 different frames:
new_value = prev * (1 - decay) + current * decay
(decay is the strength of the interpolation -> higher = smoother)

Spatial interpolation -> Between two samples in the same frame:
0 1 2 3 4 5 -> 0 a b c 1 a b c 2 a b c 3 a b c 4 a b c 5
6, 7        -> 6, 6.25 6.5 6.75 7

#pragma once

#include <miniaudio.h>
#include <renamenoise.h>

#include "queue.h"

using u64 = unsigned long long;
using u32 = unsigned int;

class AudioStream {
public:
  ~AudioStream();

  // Either stream audio from a file, or capture audio from the microphone
  void init(const char* path, bool is_capture);
  void start();

  u32 sample_rate();
  std::vector<float> get_chunk(std::stop_token token);

  u64 write_samples(const void* input, u32 size);
  void queue_samples(const void* input, void* output, u64 num_samples);

  void enable_resampler(u32 channels, u32 samplerate);
  std::vector<float> resample(float* samples, u64 length);

private:
  ma_device_config init_device_codec(const char* path);

  bool m_resampling;
  bool m_is_capture;
  bool m_started;

  ma_device m_device;
  ma_device_config m_dev_cfg;
  ma_encoder m_encoder;
  ma_decoder m_decoder;
  ma_data_converter m_converter;

  ReNameNoiseDenoiseState* m_denoiser;
  SampleQueue<float> m_samples;
};

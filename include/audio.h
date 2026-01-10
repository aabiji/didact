#pragma once

#include <functional>
#include <miniaudio.h>
#include <renamenoise.h>

#include "queue.h"

using u64 = unsigned long long;
using u32 = unsigned int;

// Callback to process audio samples supplied by miniaudio
using AudioCallback = std::function<void(void*, float*, u32)>;

class AudioStream {
public:
  // Either stream audio from a file, or capture audio from the microphone
  ~AudioStream();
  AudioStream(const char* path, bool is_capture);

  u32 sample_rate();
  void start(AudioCallback callback, void* user_data);
  std::vector<float> get_samples(std::stop_token token, int size);
  void queue_samples(const void* input, void* output, u64 num_samples);

  void enable_resampler(u32 samplerate);
  std::vector<float> resample(float* samples, u64 length);

private:
  ma_device_config init_device_codec(const char* path);

  void* m_user_data;
  AudioCallback m_user_callback;

  bool m_resampling;
  bool m_is_capture;
  bool m_started;
  SampleQueue m_samples;

  ma_device m_device;
  ma_device_config m_dev_cfg;
  ma_encoder m_encoder;
  ma_decoder m_decoder;
  ma_data_converter m_converter;
};

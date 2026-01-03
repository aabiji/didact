#pragma once

#include <miniaudio.h>
#include <renamenoise.h>

#include "queue.h"

class AudioStream {
public:
  ~AudioStream();

  // Either stream audio from a file, or capture audio from the microphone
  AudioStream(ma_device_data_proc callback, void* user_data, const char* path,
              bool is_capture);

  void start();
  ma_uint32 sample_rate();

  bool have_chunk(bool flush);
  std::vector<int16_t> get_chunk();

  ma_uint64 write_samples(const void* input, ma_uint32 size);
  void queue_samples(const void* input, void* output, ma_uint64 amount);

  void enable_resampler(ma_uint32 channels, ma_uint32 samplerate);
  std::vector<float> resample(int16_t* samples, ma_uint64 length, ma_uint64* read);

private:
  ma_device_config init_device_codec(const char* path);

  bool m_resampling;
  bool m_is_capture;

  ma_device m_device;
  ma_encoder m_encoder;
  ma_decoder m_decoder;
  ma_data_converter m_converter;

  ReNameNoiseDenoiseState* m_denoiser;
  SampleQueue<float> m_convert_queue;
  SampleQueue<int16_t> m_raw_queue;
};

#pragma once

#include <miniaudio.h>
#include <renamenoise.h>

#include "queue.h"

// Device for audio capture/playback
class AudioDevice {
public:
  ~AudioDevice();
  AudioDevice(ma_device_data_proc callback, void* user_data, const char* path,
              bool capturing);

  void start();
  int sample_rate(); // The actual sample rate, not the configured one
  ma_uint64 process_frame(const void* input, void* output,
                          ma_uint32 num_samples);

private:
  void init_codec(const char* path);

  ma_device m_device;
  ma_encoder m_encoder;
  ma_decoder m_decoder;
  bool m_capturing;
};

class Denoiser {
public:
  ~Denoiser();
  Denoiser();

  void add_samples(int16_t* input, int size);
  bool have_frame(bool flush);
  std::vector<int16_t> process();

private:
  ReNameNoiseDenoiseState* m_state;
  SampleQueue<float> m_queue;
};

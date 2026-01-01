#define MINIAUDIO_IMPLEMENTATION

#include "audio.h"
#include "error.h"

AudioDevice::~AudioDevice() {
  ma_device_uninit(&m_device);
  if (m_capturing)
    ma_encoder_uninit(&m_encoder);
  else
    ma_decoder_uninit(&m_decoder);
}

AudioDevice::AudioDevice(ma_device_data_proc callback, void* user_data,
                         const char* path, bool capturing) {
  m_capturing = capturing;
  init_codec(path);

  ma_device_type type =
      m_capturing ? ma_device_type_capture : ma_device_type_playback;
  ma_device_config device_config = ma_device_config_init(type);

  device_config.capture.format = m_encoder.config.format;
  device_config.capture.channels = m_encoder.config.channels;
  device_config.playback.format = m_decoder.outputFormat;
  device_config.playback.channels = m_decoder.outputChannels;
  device_config.sampleRate =
      m_capturing ? m_encoder.config.sampleRate : m_decoder.outputSampleRate;

  device_config.dataCallback = callback;
  device_config.pUserData = user_data;

  if (ma_device_init(nullptr, &device_config, &m_device) != MA_SUCCESS)
    throw Error("Failed to open the device");
}

void AudioDevice::init_codec(const char* path) {
  if (m_capturing) {
    // This is because RNNoise takes in 10 second frames at
    // 48000 Hz and it would be nicer to avoid resampling.
    int rate = 48000;
    ma_encoder_config encoder_config =
        ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, 1, rate);
    if (ma_encoder_init_file(path, &encoder_config, &m_encoder) != MA_SUCCESS)
      throw Error("Failed to initialize the encoder");
  } else {
    ma_decoder_config decoder_config =
        ma_decoder_config_init(ma_format_s16, 1, 44100);
    if (ma_decoder_init_file(path, &decoder_config, &m_decoder) != MA_SUCCESS)
      throw Error("Failed to initialize the decoder");
  }
}

int AudioDevice::sample_rate() { return m_device.sampleRate; }

void AudioDevice::start() {
  if (ma_device_start(&m_device) != MA_SUCCESS)
    throw Error("Failed to start the device");
}

ma_uint64 AudioDevice::process_frame(const void* input, void* output,
                                     ma_uint32 num_samples) {
  if (m_capturing) {
    ma_uint64 samples_written;
    ma_encoder_write_pcm_frames(&m_encoder, input, num_samples,
                                &samples_written);
    return samples_written;
  } else {
    ma_uint64 samples_read;
    ma_decoder_read_pcm_frames(&m_decoder, output, num_samples, &samples_read);
    return samples_read;
  }
}

Denoiser::~Denoiser() { renamenoise_destroy(m_state); }

Denoiser::Denoiser() {
  m_state = renamenoise_create(nullptr);
  m_queue.init(renamenoise_get_frame_size() * 5);
}

void Denoiser::add_samples(int16_t* input, int size) {
  std::vector<float> converted(size, 0.0);
  for (int i = 0; i < size; i++) {
    converted[i] = input[i];
  }
  m_queue.push_samples(converted.data(), size);
}

bool Denoiser::have_frame(bool flush) {
  int size = flush ? 0 : renamenoise_get_frame_size();
  return m_queue.have_enough(size);
}

std::vector<int16_t> Denoiser::process() {
  int size = renamenoise_get_frame_size();
  std::vector<float> input = m_queue.pop_samples(size);

  std::vector<int16_t> output(size);
  renamenoise_process_frame_clamped(m_state, output.data(), input.data());
  return output;
}

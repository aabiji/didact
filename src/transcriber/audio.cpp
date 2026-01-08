#define MINIAUDIO_IMPLEMENTATION

#include <algorithm>

#include "error.h"
#include "transcriber/audio.h"

AudioStream::~AudioStream() {
  if (m_started) {
    ma_device_stop(&m_device);
    ma_device_uninit(&m_device);
  }

  if (m_is_capture)
    ma_encoder_uninit(&m_encoder);
  else
    ma_decoder_uninit(&m_decoder);

  if (m_resampling)
    ma_data_converter_uninit(&m_converter, nullptr);

  renamenoise_destroy(m_denoiser);
}

void AudioStream::init(const char* path, bool is_capture) {
  m_is_capture = is_capture;
  m_started = false;
  m_resampling = false;

  init_device_codec(path);
  m_denoiser = renamenoise_create(nullptr);

  m_raw_queue.init(renamenoise_get_frame_size() * 6);
  m_convert_queue.init(renamenoise_get_frame_size() * 5);
}

void AudioStream::start(ma_device_data_proc callback, void* user_data) {
  m_dev_cfg.dataCallback = callback;
  m_dev_cfg.pUserData = user_data;
  m_started = true;

  if (ma_device_init(nullptr, &m_dev_cfg, &m_device) != MA_SUCCESS)
    throw Error("Failed to open the device");

  if (ma_device_start(&m_device) != MA_SUCCESS)
    throw Error("Failed to start the device");
}

ma_device_config AudioStream::init_device_codec(const char* path) {
  u32 channels = 1;
  ma_format fmt = ma_format_s16;
  // RNNoise requires a 48 kHz sampling rate
  u32 rate = m_is_capture ? 48000 : 44100;

  if (m_is_capture) {
    ma_encoder_config codec_cfg =
        ma_encoder_config_init(ma_encoding_format_wav, fmt, channels, rate);
    if (ma_encoder_init_file(path, &codec_cfg, &m_encoder) != MA_SUCCESS)
      throw Error("Failed to initialize the encoder");
  } else {
    ma_decoder_config codec_cfg = ma_decoder_config_init(fmt, channels, rate);
    if (ma_decoder_init_file(path, &codec_cfg, &m_decoder) != MA_SUCCESS)
      throw Error("Failed to initialize the decoder");
  }

  m_dev_cfg = ma_device_config_init(m_is_capture ? ma_device_type_capture
                                                 : ma_device_type_playback);
  m_dev_cfg.sampleRate = rate;
  m_dev_cfg.capture.format = fmt;
  m_dev_cfg.capture.channels = channels;
  m_dev_cfg.playback.format = fmt;
  m_dev_cfg.playback.channels = channels;
  return m_dev_cfg;
}

u32 AudioStream::sample_rate() {
  return m_is_capture ? m_encoder.config.sampleRate : m_decoder.outputSampleRate;
}

u64 AudioStream::write_samples(const void* input, u32 size) {
  // Only writing to an output file while capturing
  if (!m_is_capture)
    return 0;
  u64 written = 0;
  ma_encoder_write_pcm_frames(&m_encoder, input, size, &written);
  return written;
}

void AudioStream::queue_samples(const void* input, void* output, u64 amount) {
  if (!m_is_capture) {
    // Read the frames into the output buffer
    u64 read = 0;
    ma_decoder_read_pcm_frames(&m_decoder, output, amount, &read);
    m_raw_queue.push_samples((int16_t*)output, (int)read);
    return;
  }

  // Queue the frames for denoising
  int16_t* ptr = (int16_t*)input;
  std::vector<float> converted(amount);
  std::transform(ptr, ptr + amount, converted.begin(), [](int16_t a) { return a; });
  m_convert_queue.push_samples(converted.data(), (int)amount);
}

bool AudioStream::have_chunk(bool flush) {
  if (!m_is_capture) {
    int size = (int)m_device.playback.internalPeriodSizeInFrames;
    return m_raw_queue.have_enough(flush ? 0 : size);
  }

  int size = renamenoise_get_frame_size();
  return m_convert_queue.have_enough(flush ? 0 : size);
}

std::vector<int16_t> AudioStream::get_chunk() {
  if (!m_is_capture) {
    int size = (int)m_device.playback.internalPeriodSizeInFrames;
    return m_raw_queue.pop_samples(size);
  }

  unsigned int size = renamenoise_get_frame_size();
  std::vector<float> input = m_convert_queue.pop_samples(size);
  std::vector<int16_t> output(size);
  renamenoise_process_frame_clamped(m_denoiser, output.data(), input.data());
  return output;
}

void AudioStream::enable_resampler(u32 channels, u32 samplerate) {
  ma_format in_fmt = m_is_capture ? m_encoder.config.format : m_decoder.outputFormat;
  u32 in_channels = m_is_capture ? m_encoder.config.channels : m_decoder.outputChannels;
  u32 in_rate = m_is_capture ? m_encoder.config.sampleRate : m_decoder.outputSampleRate;

  ma_data_converter_config config = ma_data_converter_config_init(
      in_fmt, ma_format_f32, in_channels, channels, in_rate, samplerate);

  if (ma_data_converter_init(&config, nullptr, &m_converter) != MA_SUCCESS)
    throw Error("Failed to create the resampler");
  m_resampling = true;
}

std::vector<float> AudioStream::resample(int16_t* samples, u64 length) {
  if (!m_resampling)
    return {};

  u64 read = 0;
  ma_data_converter_get_expected_output_frame_count(&m_converter, length, &read);

  std::vector<float> output(read, 0.0);
  ma_result result = ma_data_converter_process_pcm_frames(
      &m_converter, (const void*)samples, &length, (void*)output.data(), &read);
  if (result != MA_SUCCESS)
    throw Error("Failed to convert samples");

  output.resize(read);
  return output;
}

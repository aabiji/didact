#define MINIAUDIO_IMPLEMENTATION

#include "transcriber/audio.h"
#include "error.h"

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
}

void AudioStream::start() {
  auto callback = [](ma_device* stream, void* output, const void* input, u32 size) {
    AudioStream* s = (AudioStream*)stream->pUserData;
    s->queue_samples(input, output, size);
  };

  m_dev_cfg.dataCallback = callback;
  m_dev_cfg.pUserData = this;
  m_started = true;

  if (ma_device_init(nullptr, &m_dev_cfg, &m_device) != MA_SUCCESS)
    throw Error("Failed to open the device");

  if (ma_device_start(&m_device) != MA_SUCCESS)
    throw Error("Failed to start the device");
}

ma_device_config AudioStream::init_device_codec(const char* path) {
  u32 channels = 1;
  ma_format fmt = ma_format_f32;
  u32 rate = m_is_capture ? 48000 : 44100; // RNNoise requires a 48 kHz sampling rate

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

void AudioStream::queue_samples(const void* input, void* output, u64 num_samples) {
  u64 amount = num_samples;
  if (!m_is_capture) // Read the frames into the output buffer
    ma_decoder_read_pcm_frames(&m_decoder, output, num_samples, &amount);

  float* ptr = (float*)(m_is_capture ? output : input);
  m_samples.push_samples(ptr, amount);
}

std::vector<float> AudioStream::get_chunk(std::stop_token token) {
  if (m_samples.is_empty())
    return {};

  // Only denoise captured audio
  if (!m_is_capture) {
    int size = m_device.playback.internalPeriodSizeInFrames;
    return m_samples.pop_samples(size, token);
  }

  unsigned int size = renamenoise_get_frame_size();
  std::vector<float> input = m_samples.pop_samples(size, token);

  std::vector<float> output(size);
  renamenoise_process_frame(m_denoiser, output.data(), input.data());
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

std::vector<float> AudioStream::resample(float* samples, u64 length) {
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

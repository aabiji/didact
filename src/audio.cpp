#define MINIAUDIO_IMPLEMENTATION

#include "audio.h"
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
}

void AudioStream::init(const char* path, bool is_capture) {
  m_is_capture = is_capture;
  m_started = false;
  m_resampling = false;
  init_device_codec(path);
}

void AudioStream::start(AudioCallback user_callback, void* user_data) {
  m_user_data = user_data;
  m_user_callback = user_callback;

  auto callback = [](ma_device* stream, void* output, const void* input, u32 size) {
    AudioStream* s = (AudioStream*)stream->pUserData;
    float* ptr = (float*)(stream->type == ma_device_type_capture ? input : output);

    s->queue_samples(input, output, size);
    s->m_user_callback(s->m_user_data, ptr, size);
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

std::vector<float> AudioStream::get_samples(std::stop_token token, int size) {
  return m_samples.is_empty() ? std::vector<float>{} : m_samples.pop_samples(size, token);
}

void AudioStream::queue_samples(const void* input, void* output, u64 num_samples) {
  u64 amount = num_samples;
  if (m_is_capture) // Write the captured audio to the output file
    ma_encoder_write_pcm_frames(&m_encoder, input, num_samples, nullptr);
  else // Read the frames into the output buffer
    ma_decoder_read_pcm_frames(&m_decoder, output, num_samples, &amount);

  float* ptr = (float*)(m_device.type == ma_device_type_capture ? input : output);
  m_samples.push_samples(ptr, amount);
}

void AudioStream::enable_resampler(u32 samplerate) {
  ma_format in_fmt = m_is_capture ? m_encoder.config.format : m_decoder.outputFormat;
  u32 in_channels = m_is_capture ? m_encoder.config.channels : m_decoder.outputChannels;
  u32 in_rate = m_is_capture ? m_encoder.config.sampleRate : m_decoder.outputSampleRate;

  ma_data_converter_config config = ma_data_converter_config_init(
      in_fmt, in_fmt, in_channels, in_channels, in_rate, samplerate);

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

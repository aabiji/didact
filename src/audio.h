#define MINIAUDIO_IMPLEMENTATION
#define MA_DEBUG_OUTPUT
#include <miniaudio.h>

#include "error.h"

class Codec {
public:
  Codec(bool encoding, const char *path) {
    m_encoding = encoding;
    if (encoding) {
      ma_encoder_config encoder_config = ma_encoder_config_init(
          ma_encoding_format_wav, ma_format_s16, 1, 44100);
      if (ma_encoder_init_file(path, &encoder_config, &m_encoder) != MA_SUCCESS)
        throw Error("Failed to initialize the encoder");
    } else {
      ma_decoder_config decoder_config =
          ma_decoder_config_init(ma_format_s16, 1, 44100);
      if (ma_decoder_init_file(path, &decoder_config, &m_decoder) != MA_SUCCESS)
        throw Error("Failed to initialize the decoder");
    }
  }

  ~Codec() {
    if (m_encoding)
      ma_encoder_uninit(&m_encoder);
    else
      ma_decoder_uninit(&m_decoder);
  }

  // return the number of samples written or read
  ma_uint64 process_frame(const void *input, void *output,
                          ma_uint32 num_samples) {
    if (m_encoding) {
      ma_uint64 samples_written;
      ma_encoder_write_pcm_frames(&m_encoder, input, num_samples,
                                  &samples_written);
      return samples_written;
    } else {
      ma_uint64 samples_read;
      ma_decoder_read_pcm_frames(&m_decoder, output, num_samples,
                                 &samples_read);
      return samples_read;
    }
  }

  bool encoding() { return m_encoding; }

  ma_format format() {
    return m_encoding ? m_encoder.config.format : m_decoder.outputFormat;
  }

  int channels() {
    return m_encoding ? m_encoder.config.channels : m_decoder.outputChannels;
  }

  int sample_rate() {
    return m_encoding ? m_encoder.config.sampleRate
                      : m_decoder.outputSampleRate;
  }

private:
  bool m_encoding;
  ma_encoder m_encoder;
  ma_decoder m_decoder;
};

class Device {
public:
  Device(Codec &codec, ma_device_data_proc callback,
         ma_log_callback_proc logger, void *user_data) {
    ma_log_init(nullptr, &m_log);
    ma_log_register_callback(&m_log, ma_log_callback_init(logger, nullptr));

    ma_context_config context_config = ma_context_config_init();
    context_config.pLog = &m_log;
    if (ma_context_init(nullptr, 0, &context_config, &m_context) != MA_SUCCESS)
      throw Error("Failed to initialize the context");

    ma_device_type type =
        codec.encoding() ? ma_device_type_capture : ma_device_type_playback;
    ma_device_config device_config = ma_device_config_init(type);
    device_config.playback.format = codec.format();
    device_config.playback.channels = codec.channels();
    device_config.sampleRate = codec.sample_rate();
    device_config.dataCallback = callback;
    device_config.pUserData = user_data;

    if (ma_device_init(&m_context, &device_config, &m_device) != MA_SUCCESS)
      throw Error("Failed to open the device");

    if (ma_device_start(&m_device) != MA_SUCCESS)
      throw Error("Failed to start the device");
  }

  ~Device() {
    ma_device_uninit(&m_device);
    ma_context_uninit(&m_context);
    ma_log_uninit(&m_log);
  }

private:
  ma_device m_device;
  ma_context m_context;
  ma_log m_log;
};

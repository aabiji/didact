#define MINIAUDIO_IMPLEMENTATION
#define MA_DEBUG_OUTPUT
#include <miniaudio.h>
#include <renamenoise.h>
#include <vector>

#include "error.h"

class MicrophoneCapture {
public:
  MicrophoneCapture(const char *outpath, ma_log_callback_proc logger,
                    ma_device_data_proc callback, void *user_data) {
    ma_log_init(nullptr, &m_log);
    ma_log_register_callback(&m_log, ma_log_callback_init(logger, nullptr));

    ma_context_config context_config = ma_context_config_init();
    context_config.pLog = &m_log;
    if (ma_context_init(nullptr, 0, &context_config, &m_context) != MA_SUCCESS)
      throw Error("Failed to initialize the context");

    ma_encoder_config encoder_config = ma_encoder_config_init(
        ma_encoding_format_wav, ma_format_s16, 1, 48000); // 44100);
    if (ma_encoder_init_file(outpath, &encoder_config, &m_encoder) !=
        MA_SUCCESS)
      throw Error("Failed to initialize the encoder");

    ma_device_config device_config =
        ma_device_config_init(ma_device_type_capture);
    device_config.capture.format = m_encoder.config.format;
    device_config.capture.channels = m_encoder.config.channels;
    device_config.sampleRate = m_encoder.config.sampleRate;
    device_config.dataCallback = callback;
    device_config.pUserData = user_data;

    if (ma_device_init(&m_context, &device_config, &m_device) != MA_SUCCESS)
      throw Error("Failed to open the device");

    if (ma_device_start(&m_device) != MA_SUCCESS)
      throw Error("Failed to start the device");
  }

  ma_uint64 save_frame(const void *input, ma_uint32 num_samples) {
    ma_uint64 samples_written;
    ma_encoder_write_pcm_frames(&m_encoder, input, num_samples,
                                &samples_written);
    return samples_written;
  }

  int sample_rate() { return m_device.sampleRate; }

  ~MicrophoneCapture() {
    ma_encoder_uninit(&m_encoder);
    ma_device_uninit(&m_device);
    ma_context_uninit(&m_context);
    ma_log_uninit(&m_log);
  }

private:
  ma_device m_device;
  ma_encoder m_encoder;
  ma_context m_context;
  ma_log m_log;
};

class Denoiser {
public:
  Denoiser() { m_state = renamenoise_create(nullptr); }

  ~Denoiser() { renamenoise_destroy(m_state); }

  std::vector<int16_t> process(int16_t *input, int size) {
    int chunk_size = renamenoise_get_frame_size();
    int num_chunks = std::ceil(float(size) / float(chunk_size));
    int overflow = num_chunks * chunk_size - size;
    std::vector<int16_t> output(num_chunks * chunk_size);

    for (int i = 0; i < num_chunks; i++) {
      int start = i * chunk_size;
      int end = std::min(start + chunk_size, size);

      std::vector<float> converted(end - start, 0.0);
      std::copy(input + start, input + end, converted.begin());
      renamenoise_process_frame_clamped(m_state, output.data() + start,
                                        converted.data());
    }

    // Remove the padding used for ReNameNoise
    output.resize(output.size() - overflow);
    return output;
  }

private:
  ReNameNoiseDenoiseState *m_state;
};

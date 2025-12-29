#include <algorithm>
#include <iostream>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <raylib.h>

#include "analyzer.h"
#include "error.h"

struct CallbackData {
  ma_decoder *decoder;
  SpectrumAnalyzer *analyzer;
  float_vec fft_bars;
  bool done;
};

void data_callback(ma_device *device, void *output, const void *input,
                   ma_uint32 num_samples) {
  CallbackData *data = (CallbackData *)device->pUserData;

  unsigned long long samples_read = 0;
  ma_result result = ma_decoder_read_pcm_frames(data->decoder, output,
                                                num_samples, &samples_read);
  if (samples_read < num_samples) {
    data->done = true;
    return;
  }

  data->fft_bars = data->analyzer->process((int16_t *)output, samples_read);
}

void draw_bars(std::vector<float> bars, Vector2 window_size) {
  int num_bars = bars.size();
  float min = *(std::min_element(bars.begin(), bars.end()));
  float max = *(std::max_element(bars.begin(), bars.end()));

  Vector2 bar_size = {5, 100};
  float start_x = window_size.x / 2.0f;

  for (int i = -(num_bars - 1); i < num_bars; i++) {
    float value = bars[abs(i)];
    float value_percent = (value - min) / (max - min);
    float height = value_percent * bar_size.y;
    float x = start_x + 2 * (i * bar_size.x);
    float center_y = window_size.y / 2.0f - height / 2.0f;
    DrawRectangle(x, center_y, bar_size.x, height, WHITE);
  }
}

int main() {
  try {
    const char *path = "../assets/fly-me-to-the-moon.mp3";

    ma_decoder decoder;
    ma_decoder_config decoder_config =
        ma_decoder_config_init(ma_format_s16, 1, 44100);
    if (ma_decoder_init_file(path, &decoder_config, &decoder) != MA_SUCCESS)
      throw Error("Failed to initialize the decoder");

    SpectrumAnalyzer analyzer(decoder.outputSampleRate);
    CallbackData data = {&decoder, &analyzer, {}, false};

    ma_device device;
    ma_device_config device_config =
        ma_device_config_init(ma_device_type_playback);
    device_config.playback.format = decoder.outputFormat;
    device_config.playback.channels = decoder.outputChannels;
    device_config.sampleRate = decoder.outputSampleRate;
    device_config.dataCallback = data_callback;
    device_config.pUserData = &data;

    if (ma_device_init(nullptr, &device_config, &device) != MA_SUCCESS)
      throw Error("Failed to open the playback device");

    if (ma_device_start(&device) != MA_SUCCESS) {
      ma_device_uninit(&device);
      throw Error("Failed to start the playback device");
    }

    Vector2 window_size = {900, 700};
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(window_size.x, window_size.y, "didact");
    SetTargetFPS(60);

    float_vec prev_bars;

    while (!WindowShouldClose()) {
      ClearBackground(BLACK);
      BeginDrawing();

      if (data.done)
        break;

      if (data.fft_bars.size() > 0 && prev_bars.size() > 0) {
        // Interpolate between the 2 frames
        float decay = 0.4;
        float_vec bars(data.fft_bars.size());
        for (int i = 0; i < bars.size(); i++) {
          bars[i] = prev_bars[i] * (1 - decay) + data.fft_bars[i] * decay;
        }
        draw_bars(bars, window_size);
      }
      prev_bars = data.fft_bars;

      EndDrawing();
    }

    ma_decoder_uninit(&decoder);
    ma_device_uninit(&device);

    CloseWindow();
  } catch (const std::runtime_error &error) {
    std::cout << error.what() << "\n";
    return -1;
  }

  return 0;
}

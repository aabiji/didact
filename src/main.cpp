#include <algorithm>
#include <iostream>

#include <raylib.h>

#include "analyzer.h"
#include "audio.h"

struct CallbackData {
  Codec *codec;
  SpectrumAnalyzer *analyzer;
  float_vec fft_bars;
  bool done;
};

void data_callback(ma_device *device, void *output, const void *input,
                   ma_uint32 num_samples) {
  CallbackData *data = (CallbackData *)device->pUserData;

  ma_uint64 size = data->codec->process_frame(input, output, num_samples);
  if (size < num_samples) {
    data->done = true;
    return;
  }

  int16_t *ptr = (int16_t *)(data->codec->encoding() ? input : output);
  data->fft_bars = data->analyzer->process(ptr, size);
}

void logger(void *data, ma_uint32 level, const char *msg) {
  std::cout << "[miniaudio]: " << msg << "\n";
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

int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw Error("Usage: didact [ --microphone | --file ] <PATH>");

    if (strcmp(argv[1], "--microphone") != 0 && strcmp(argv[1], "--file") != 0)
      throw Error("Usage: didact [ --microphone | --file ] <PATH>");

    Codec codec(strcmp(argv[1], "--microphone") == 0, argv[2]);
    SpectrumAnalyzer analyzer(codec.sample_rate());
    CallbackData data = {&codec, &analyzer, {}, false};
    Device device(codec, data_callback, logger, (void *)&data);

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

    CloseWindow();
  } catch (const std::runtime_error &error) {
    std::cout << error.what() << "\n";
    return -1;
  }

  return 0;
}

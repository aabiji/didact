#include <algorithm>
#include <iostream>
#include <raylib.h>
#include <thread>

#include "decoder.h"
#include "fft.h"

void process_audio_frame(void *user_data, int16_t *samples, int num_samples) {
  SpectrumAnalyzer *analyzer = (SpectrumAnalyzer *)user_data;
  analyzer->process(samples, num_samples);
}

int main() {
  try {
    std::stop_source stopper;
    std::stop_token token = stopper.get_token();

    AudioDecoder decoder("../assets/music.mp3", token);
    SpectrumAnalyzer analyzer(decoder.sample_rate());

    SampleHandler handler = {.callback = process_audio_frame,
                             .user_data = (void *)&analyzer};
    std::thread t1([&] { decoder.process_file(handler); });

    int window_width = 900, window_height = 700;
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(window_width, window_height, "didact");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
      ClearBackground(BLACK);
      BeginDrawing();

      auto bars = analyzer.get_bars();
      int num_bars = bars.size();
      float min = *(std::min_element(bars.begin(), bars.end()));
      float max = *(std::max_element(bars.begin(), bars.end()));

      Vector2 bar_size = {10, 200};
      float start_x = float(window_width) / 2.0f;

      for (int i = -(num_bars - 1); i < num_bars; i++) {
        float x = start_x + i * bar_size.x;
        float value_percent = (bars[abs(i)] - min) / (max - min);
        float height = value_percent * bar_size.y;
        float center_y = float(window_height) / 2.0f - height / 2.0f;
        DrawRectangle(x, center_y, bar_size.x, height, WHITE);
      }

      EndDrawing();
    }

    CloseWindow();

    stopper.request_stop();
    t1.join();
  } catch (const std::runtime_error &error) {
    std::cout << error.what() << "\n";
  }
}

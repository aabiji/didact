#include <raylib.h>
#include <algorithm>
#include <iostream>
#include <thread>

#include "decoder.h"

#define FFT_IMPLEMENTATION
#include "fft.h"

std::vector<double> frequency_bins;
double max_amplitude = 0;

void process_audio_frame(void* data, SampleChunk chunk) {
  frequency_bins = analyze_spectrum(chunk.samples, chunk.num_samples);

  max_amplitude =
      *(std::max_element(frequency_bins.begin(), frequency_bins.end()));
}

int main() {
  try {
    std::stop_source stopper;
    std::stop_token token = stopper.get_token();

    AudioDecoder decoder("../assets/music.mp3", token);
    std::thread t1([&] { decoder.process_file(process_audio_frame, nullptr); });

    int window_width = 900, window_height = 700;
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(window_width, window_height, "didact");
    SetTargetFPS(60);

    Vector2 bar_size = {25, 250};
    int max_bins = double(window_width) / bar_size.x;

    while (!WindowShouldClose()) {
      ClearBackground(BLACK);
      BeginDrawing();

      int min_y = double(window_height) / 2.0 + (bar_size.y / 2.0);
      for (int i = 0; i < frequency_bins.size(); i++) {
        int height = (frequency_bins[i] / max_amplitude) * bar_size.y;
        double scale =
            std::pow(double(i) / double(frequency_bins.size()), 1.0 / 2.0);
        int bin_index = scale * max_bins;
        Color color = ColorFromHSV(scale * 360, 1.0, 1.0);
        DrawRectangle(bin_index * bar_size.x, min_y - height, bar_size.x,
                      height, color);
      }

      EndDrawing();
    }

    CloseWindow();

    stopper.request_stop();
    t1.join();
  } catch (const std::runtime_error& error) {
    std::cout << error.what() << "\n";
  }
}

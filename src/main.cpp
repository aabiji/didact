#include <raylib.h>
#include <iostream>
#include <thread>

#include "decoder.h"

#define FFT_IMPLEMENTATION
#include "fft.h"

std::vector<double> frequency_bins;

void process_audio_frame(void* data, SampleChunk chunk) {
  frequency_bins = analyze_spectrum(chunk.samples, chunk.num_samples);
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

    while (!WindowShouldClose()) {
      BeginDrawing();

      double ratio =
          double(decoder.sample_rate()) / double(frequency_bins.size());
      for (int i = 0; i < frequency_bins.size(); i++) {
        double amplitude = frequency_bins[i];
        int width = 10;
        int height = amplitude * 250;
        int x = i * ratio * width;
        int y = window_height;
        DrawRectangle(x, y, 10, height, GREEN);
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

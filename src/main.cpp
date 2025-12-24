#include <raylib.h>

#include <atomic>
#include <iostream>
#include <thread>

#include "decoder.h"

int main() {
  try {
    std::stop_source stopper;
    std::stop_token token = stopper.get_token();

    SampleQueue queue(1024 * 3, 1024, 2, token);
    AudioDecoder reader("../assets/music.mp3", &queue, token);

    std::thread t1([&] { reader.process_file(); });

    int width = 900, height = 700;
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(width, height, "didact");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
      BeginDrawing();
      DrawRectangle(100, 200, 50, 50, GREEN);
      EndDrawing();
    }

    CloseWindow();

    stopper.request_stop();
    t1.join();
  } catch (const std::runtime_error& error) {
    std::cout << error.what() << "\n";
  }
}

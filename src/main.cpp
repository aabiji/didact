#include <raylib.h>

#include <atomic>
#include <iostream>
#include <thread>

#include "decoder.h"

int main() {
  try {
    std::stop_source stopper;

    SampleQueue queue(stopper.get_token(), 1024 * 3, 1024, 2);
    SampleQueue* queue_ptr = &queue;

    // TODO: give this the stop token as well
    AudioDecoder reader("../assets/music.mp3");
    std::thread t1([&] { reader.process_file(queue_ptr); });

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

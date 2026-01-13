#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>

#include "transcriber.h"
#include "ui.h"

int main() {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;

  try {
    // TODO: download these automatically if they aren't present
    // (do the same for the Roboto font)
    ModelPaths paths = {
        "../assets/sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06/tokens.txt",
        "../assets/sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06/encoder.onnx",
        "../assets/sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06/decoder.onnx",
        "../assets/sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06/joiner.onnx",
    };
    Transcriber engine(paths, "test.wav", true);
    engine.start();

    if (!SDL_Init(SDL_INIT_VIDEO))
      throw Error(SDL_GetError());

    int window_width = 900;
    int window_height = 700;
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags flags =
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window = SDL_CreateWindow("didact", window_width, window_height, flags);
    if (!window)
      throw Error(SDL_GetError());

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer)
      throw Error(SDL_GetError());
    SDL_SetRenderVSync(renderer, 1);

    FontCache font(renderer, "../assets/Roboto-Regular.ttf", 18, {255, 255, 255, 255});

    Button copy(renderer, &font);
    copy.set_icon("../assets/icons/copy.svg", {28, 28});

    Button save(renderer, &font);
    save.set_icon("../assets/icons/save.svg", {28, 28});

    Cursor cursor;

    SDL_Event event;
    bool running = true;

    float area_height = 100.0f;
    float area_width = (float)window_width / 1.5f;
    float area_padding = (window_width - area_width) / 2.0f;
    float bottom_padding = 50;
    SDL_FRect bar_rect = {.x = window_width - area_padding,
                          .y = window_height - (area_height / 2.0f) - bottom_padding,
                          .w = 3,
                          .h = 80};

    while (running) {
      while (SDL_PollEvent(&event)) {
        bool done = event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                     event.window.windowID == SDL_GetWindowID(window));
        if (done) {
          running = false;
          break;
        }

        if (event.type == SDL_EVENT_WINDOW_RESIZED) {
          window_width = event.window.data1;
          window_height = event.window.data2;
        }
      }

      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);

      copy.render({window_width - copy.size().x, 50}, cursor);
      save.render({window_width - save.size().x, 95}, cursor);

      SDL_FRect rect = {.x = 50,
                        .y = 50,
                        .w = (float)window_width - 100,
                        .h = (float)window_height / 1.5f};
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      SDL_RenderRect(renderer, &rect);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

      // TODO: define a text editing area (that can be read only toggled)
      auto lines = engine.get_transcript();
      if (lines.size() == 0)
        font.render("Capturing...", {65.0f, 65.0f}); // TODO: add nice cetered animation
      for (int i = 0; i < lines.size(); i++) {
        font.render(lines[i],
                    {65.0f, 65.0f + 20.0f * i}); // TODO: how to determine the proper y?
      }

      std::vector<float> amplitudes = engine.get_normalized_waveform();
      for (int i = amplitudes.size() - 1; i >= 0; i--) {
        SDL_FRect rect;
        rect.w = bar_rect.w;
        rect.h = amplitudes[i] * bar_rect.h;
        rect.x = bar_rect.x - ((amplitudes.size() - i) * (rect.w * 2));
        rect.y = bar_rect.y - rect.h / 2.0f;
        if (rect.x < area_padding)
          break;

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &rect);
      }

      SDL_RenderPresent(renderer);
    }

  } catch (const std::runtime_error& error) {
    SDL_Log(error.what(), "\n");
    return -1;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

#include <algorithm>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "error.h"
#include "transcriber/transcriber.h"

void draw_bars(SDL_Renderer* renderer, SDL_FRect bar_rect, std::vector<float> bars) {
  int num_bars = (int)bars.size();
  float min = *(std::min_element(bars.begin(), bars.end()));
  float max = *(std::max_element(bars.begin(), bars.end()));

  for (int i = -(num_bars - 1); i < num_bars; i++) {
    float value = bars[(size_t)abs(i)];
    float value_percent = (value - min) / (max - min);

    SDL_FRect rect;
    rect.w = bar_rect.w;
    rect.h = value_percent * bar_rect.y;
    rect.x = bar_rect.x + 2 * ((float)i * rect.w);
    rect.y = bar_rect.y - rect.h / 2.0f;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &rect);
  }
}

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
    Transcription transcript;
    transcript.init(paths, "test.wav", true);
    transcript.start();

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

    int icon_size = 64;
    const char* icon_path = "../assets/icons/copy.svg";
    SDL_IOStream* ops = SDL_IOFromFile(icon_path, "rb");
    if (!ops)
      throw Error("Failed to load {}", icon_path);
    SDL_Surface* surf = IMG_LoadSizedSVG_IO(ops, icon_size, icon_size);
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureColorMod(tex, 255, 0, 0); // assuming that the original svg is white
    SDL_DestroySurface(surf);
    SDL_FRect icon_rect = {.x = 0, .y = 0, .w = (float)icon_size, .h = (float)icon_size};
    //SDL_DestroyTexture(tex);

    TTF_Font* font = TTF_OpenFont("../assets/Roboto-Regular.ttf", 25);
    if (font == nullptr)
      throw Error("Failed to open the font");
    const char* text = "hello :)";
    SDL_Color text_color = {255, 255, 255, 255};
    SDL_Surface* text_surf = TTF_RenderText_Solid(font, text, 8, text_color);
    SDL_Texture* text_tex = SDL_CreateTextureFromSurface(renderer, text_surf);
    SDL_FRect text_rect = {
        .x = 0, .y = 500, .w = (float)text_surf->w, .h = (float)text_surf->h};
    SDL_DestroySurface(text_surf);
    //SDL_DestroyTexture(text_tex);

    SDL_Event event;
    bool running = true;
    float_vec prev_bars;

    SDL_FRect bar_rect = {.x = (float)window_width / 2.0f, .y = 50, .w = 5, .h = 100};
    int ui_offset_y = bar_rect.y + 25;

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

      SDL_RenderTexture(renderer, tex, nullptr, &icon_rect);
      SDL_RenderTexture(renderer, text_tex, nullptr, &text_rect);

      auto fft_bars = transcript.get_visualization_data();
      if (fft_bars.size() > 0 && prev_bars.size() > 0) {
        // Peek decay between frames (rise sharply then fall slowly)
        float_vec bars(fft_bars.size());
        for (size_t i = 0; i < bars.size(); i++) {
          bars[i] = std::max(fft_bars[i], prev_bars[i] * 0.85f);
        }
        draw_bars(renderer, bar_rect, bars);
      }
      prev_bars = fft_bars;

      SDL_RenderPresent(renderer);
    }

    transcript.stop();
  } catch (const std::runtime_error& error) {
    SDL_Log(error.what(), "\n");
    return -1;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

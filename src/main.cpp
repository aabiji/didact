#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "error.h"
#include "transcriber.h"

class Icon {
public:
  ~Icon() { SDL_DestroyTexture(m_tex); }

  Icon(SDL_Renderer* renderer, const char* path, float size,
       SDL_Color color = {255, 255, 255}) {
    SDL_IOStream* ops = SDL_IOFromFile(path, "rb");
    if (!ops)
      throw Error(SDL_GetError());

    SDL_Surface* surf = IMG_LoadSizedSVG_IO(ops, size, size);
    m_tex = SDL_CreateTextureFromSurface(renderer, surf);
    m_size = size;
    SDL_DestroySurface(surf);

    // NOTE: This only works if the original svg is white
    SDL_SetTextureColorMod(m_tex, color.r, color.g, color.b);
  }

  void render(SDL_Renderer* renderer, float x, float y) {
    SDL_FRect icon_rect = {.x = x, .y = y, .w = m_size, .h = m_size};
    SDL_RenderTexture(renderer, m_tex, nullptr, &icon_rect);
  }

private:
  float m_size;
  SDL_Texture* m_tex;
};

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

    TTF_Init();
    TTF_Font* font = TTF_OpenFont("../assets/Roboto-Regular.ttf", 25);
    if (font == nullptr)
      throw Error(SDL_GetError());
    const char* text = "hello :)";
    SDL_Color text_color = {255, 255, 255, 255};
    SDL_Surface* text_surf = TTF_RenderText_Solid(font, text, 8, text_color);
    SDL_Texture* text_tex = SDL_CreateTextureFromSurface(renderer, text_surf);
    SDL_FRect text_rect = {
        .x = 0, .y = 500, .w = (float)text_surf->w, .h = (float)text_surf->h};
    SDL_DestroySurface(text_surf);

    Icon copy_icon(renderer, "../assets/icons/copy.svg", 32);
    Icon save_icon(renderer, "../assets/icons/save.svg", 32);

    SDL_Event event;
    bool running = true;

    float area_height = 100.0f;
    float area_width = (float)window_width / 1.5f;
    float area_padding = (window_width - area_width) / 2.0f;
    SDL_FRect bar_rect = {.x = window_width - area_padding,
                          .y = window_height - (area_height / 2.0f),
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

      SDL_RenderTexture(renderer, text_tex, nullptr, &text_rect);

      copy_icon.render(renderer, 0, 0);
      save_icon.render(renderer, 32, 0);

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

    SDL_DestroyTexture(text_tex);
  } catch (const std::runtime_error& error) {
    SDL_Log(error.what(), "\n");
    return -1;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "clay_renderer_SDL3.c"

#include <iostream>
#include <utility>

#include "error.h"
#include "transcriber.h"

// TODO: write our own custom clay renderer (so that we can use the font atlas)
// TODO: refactor this main file, start building the main ui

void draw_waveform_visualization(SDL_Renderer* renderer, std::vector<float>& amplitudes,
                                 float window_width, float window_height) {
  float area_height = 100.0f;
  float area_width = window_width / 1.5f;
  float area_padding = (window_width - area_width) / 2.0f;
  float bottom_padding = 50;
  SDL_FRect bar_rect = {.x = window_width - area_padding,
                        .y = window_height - (area_height / 2.0f) - bottom_padding,
                        .w = 3,
                        .h = 80};

  for (int i = amplitudes.size() - 1; i >= 0; i--) { // from newest to oldest data
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
}

struct Vec2 {
  float x, y;
};

class Cursor {
public:
  Cursor() {
    m_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    m_pointer = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
  }

  ~Cursor() {
    SDL_DestroyCursor(m_default);
    SDL_DestroyCursor(m_pointer);
  }

  void update(Vec2 pos, Vec2 size) {
    float mouse_x = 0, mouse_y = 0;
    SDL_GetMouseState(&mouse_x, &mouse_y);

    bool x_inside = mouse_x >= pos.x && mouse_x <= pos.x + size.x;
    bool y_inside = mouse_y >= pos.y && mouse_y <= pos.y + size.y;
    SDL_SetCursor(x_inside && y_inside ? m_pointer : m_default);
  }

private:
  SDL_Cursor* m_default;
  SDL_Cursor* m_pointer;
};

class SVG {
public:
  ~SVG() {
    if (m_initialized && m_tex)
      SDL_DestroyTexture(m_tex);
  }

  SVG() : m_initialized(false) {}

  SVG(SDL_Renderer* renderer, const char* path, Vec2 size, SDL_Color color) {
    SDL_IOStream* ops = SDL_IOFromFile(path, "rb");
    if (!ops)
      throw Error(SDL_GetError());

    SDL_Surface* surf = IMG_LoadSizedSVG_IO(ops, size.x, size.y);
    m_tex = SDL_CreateTextureFromSurface(renderer, surf);
    m_size = size;
    m_initialized = true;
    SDL_DestroySurface(surf);

    // NOTE: This only works if the original svg is white
    SDL_SetTextureColorMod(m_tex, color.r, color.g, color.b);
  }

  SVG(const SVG&) = delete;
  SVG& operator=(const SVG&) = delete;

  SVG(SVG&& other) noexcept {
    m_tex = std::exchange(other.m_tex, nullptr);
    m_size = std::exchange(other.m_size, {0, 0});
    m_initialized = std::exchange(other.m_initialized, false);
  }

  SVG& operator=(SVG&& other) noexcept {
    if (this != &other) {
      if (m_initialized)
        SDL_DestroyTexture(m_tex);

      m_tex = std::exchange(other.m_tex, nullptr);
      m_size = std::exchange(other.m_size, {0, 0});
      m_initialized = std::exchange(other.m_initialized, false);
    }
    return *this;
  }

  Vec2 size() { return m_size; }

  void render(SDL_Renderer* renderer, Vec2 p) {
    if (!m_initialized)
      return;
    SDL_FRect icon_rect = {.x = p.x, .y = p.y, .w = m_size.x, .h = m_size.y};
    SDL_RenderTexture(renderer, m_tex, nullptr, &icon_rect);
  }

private:
  bool m_initialized;
  Vec2 m_size;
  SDL_Texture* m_tex;
};

void clay_error_handler(Clay_ErrorData data) {
  std::cout << data.errorText.chars << "\n";
}

static inline Clay_Dimensions
SDL_MeasureText(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) {
  TTF_Font** fonts = (TTF_Font**)userData;
  TTF_Font* font = fonts[config->fontId];
  int width, height;

  TTF_SetFontSize(font, config->fontSize);
  if (!TTF_GetStringSize(font, text.chars, text.length, &width, &height)) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to measure text: %s", SDL_GetError());
  }

  return (Clay_Dimensions){(float)width, (float)height};
}

// clang-format off
Clay_RenderCommandArray create_layout() {
  Clay_BeginLayout();

  CLAY(CLAY_ID("List container"), {
      .layout = {
        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
        .padding = CLAY_PADDING_ALL(16),
        .childGap = 16,
        .layoutDirection = CLAY_TOP_TO_BOTTOM
      }
    }) {
      for (int i = 0; i < 5; i++) {
        CLAY(CLAY_ID("List item"), {
          .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(50)},
          },
          .backgroundColor = {255, i * 50.0f, 255, 255}
        }) {}
      }
  }

  return Clay_EndLayout();
}
// clang-format on

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
    Clay_SDL3RendererData render_data;
    render_data.textEngine = TTF_CreateRendererTextEngine(render_data.renderer);
    if (!render_data.textEngine) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                   "Failed to create text engine from renderer: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    render_data.fonts = (TTF_Font**)SDL_calloc(1, sizeof(TTF_Font*));
    if (!render_data.fonts) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                   "Failed to allocate memory for the font array: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    TTF_Font* font = TTF_OpenFont("../assets/Roboto-Regular.ttf", 24);
    if (!font) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load font: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    render_data.fonts[0] = font;
    render_data.renderer = renderer;

    unsigned int memsize = Clay_MinMemorySize();
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(memsize, malloc(memsize));
    Clay_Initialize(arena, {(float)window_width, (float)window_height},
                    {clay_error_handler, nullptr});
    Clay_SetMeasureTextFunction(SDL_MeasureText, render_data.fonts);

    Cursor cursor;
    SDL_Event event;
    bool running = true;

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
          Clay_SetLayoutDimensions({(float)window_width, (float)window_height});
        }

        if (event.type == SDL_EVENT_MOUSE_MOTION) {
          Clay_SetPointerState({event.motion.x, event.motion.y},
                               event.motion.state & SDL_BUTTON_LMASK);
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
          Clay_SetPointerState({event.button.x, event.button.y},
                               event.button.button == SDL_BUTTON_LEFT);
        }

        if (event.type == SDL_EVENT_MOUSE_WHEEL) {
          Clay_UpdateScrollContainers(true, {event.wheel.x, event.wheel.y}, 0.01f);
        }
      }

      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);

      auto render_commands = create_layout();
      SDL_Clay_RenderClayCommands(&render_data, &render_commands);

      std::vector<float> amplitudes = engine.get_normalized_waveform();
      draw_waveform_visualization(renderer, amplitudes, window_width, window_height);

      SDL_RenderPresent(renderer);
    }

    TTF_CloseFont(render_data.fonts[0]);
    SDL_free(render_data.fonts);
    TTF_DestroyRendererTextEngine(render_data.textEngine);

  } catch (const std::runtime_error& error) {
    SDL_Log(error.what(), "\n");
    return -1;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

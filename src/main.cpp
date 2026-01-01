#include <algorithm>
#include <iostream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>

#include <whisper.h>

#include "analyzer.h"
#include "microphone.h"

struct CallbackData {
  MicrophoneCapture *device;
  SpectrumAnalyzer *analyzer;
  Denoiser *denoiser;
  float_vec fft_bars;
  bool done;
};

void data_callback(ma_device *device, void *output, const void *input,
                   ma_uint32 num_samples) {
  CallbackData *data = (CallbackData *)device->pUserData;

  auto processed = data->denoiser->process((int16_t *)input, num_samples);
  data->fft_bars = data->analyzer->process(processed.data(), num_samples);

  ma_uint64 size = data->device->save_frame(processed.data(), num_samples);
  if (size < num_samples) {
    data->done = true;
    return;
  }
}

void logger(void *data, ma_uint32 level, const char *msg) {
  std::cout << "[miniaudio]: " << msg << "\n";
}

void draw_bars(SDL_Renderer *renderer, std::vector<float> bars,
               float window_width, float window_height) {
  int num_bars = bars.size();
  float min = *(std::min_element(bars.begin(), bars.end()));
  float max = *(std::max_element(bars.begin(), bars.end()));

  int bar_width = 5, bar_height = 100;
  float start_x = window_width / 2.0f;

  for (int i = -(num_bars - 1); i < num_bars; i++) {
    float value = bars[abs(i)];
    float value_percent = (value - min) / (max - min);

    SDL_FRect rect;
    rect.w = bar_width;
    rect.h = value_percent * bar_height;
    rect.x = start_x + 2 * (i * rect.w);
    rect.y = window_height / 2.0f - rect.h / 2.0f;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &rect);
  }
}

int main() {
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;

  try {
    CallbackData data;
    MicrophoneCapture capture("test.wav", logger, data_callback, (void *)&data);
    SpectrumAnalyzer analyzer(capture.sample_rate());
    Denoiser denoiser;
    data = {.device = &capture,
            .analyzer = &analyzer,
            .denoiser = &denoiser,
            .fft_bars = {},
            .done = false};

    if (!SDL_Init(SDL_INIT_VIDEO))
      throw Error(SDL_GetError());

    int window_width = 900;
    int window_height = 700;
    int flags = SDL_WINDOW_RESIZABLE;
    window = SDL_CreateWindow("didact", window_width, window_height, flags);
    if (!window)
      throw Error(SDL_GetError());
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);

    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer)
      throw Error(SDL_GetError());
    SDL_SetRenderVSync(renderer, 1);

    SDL_Event event;
    bool running = true;
    float_vec prev_bars;

    while (running) {
      while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
          running = false;
          break;
        }
      }

      if (data.done)
        break;

      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);

      if (data.fft_bars.size() > 0 && prev_bars.size() > 0) {
        // Interpolate between the 2 frames
        float decay = 0.4;
        float_vec bars(data.fft_bars.size());
        for (int i = 0; i < bars.size(); i++) {
          bars[i] = prev_bars[i] * (1 - decay) + data.fft_bars[i] * decay;
        }
        draw_bars(renderer, bars, window_width, window_height);
      }
      prev_bars = data.fft_bars;

      SDL_RenderPresent(renderer);
    }

  } catch (const std::runtime_error &error) {
    SDL_Log(error.what(), "\n");
    return -1;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

#include <algorithm>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>

#include <whisper.h>

#include "analyzer.h"
#include "audio.h"
#include "error.h"

struct AudioProcessor {
  AudioDevice *device;
  Denoiser *denoiser;
  SpectrumAnalyzer *analyzer;
  float_vec fft_bars;
  bool capturing;

  void process_frames(bool flush) {
    while (denoiser->have_frame(flush)) {
      auto samples = denoiser->process();
      if (!flush)
        fft_bars = analyzer->process(samples.data(), samples.size());

      device->process_frame(samples.data(), nullptr, samples.size());
    }
  }
};

void data_callback(ma_device *device, void *output, const void *input,
                   ma_uint32 input_size) {
  AudioProcessor *ap = (AudioProcessor *)device->pUserData;

  // Don't denoise the audio that is being streamed from a file
  if (!ap->capturing) {
    ma_uint64 read = ap->device->process_frame(nullptr, output, input_size);
    ap->fft_bars = ap->analyzer->process((int16_t *)output, read);
    return;
  }

  // Only process denoised samples when capturing audio from the micrpphone
  ap->denoiser->add_samples((int16_t *)input, input_size);
  ap->process_frames(false);
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
    Denoiser denoiser;
    AudioProcessor data;
    AudioDevice device(data_callback, &data, "test.wav", true);
    SpectrumAnalyzer analyzer(device.sample_rate());

    data.device = &device;
    data.denoiser = &denoiser;
    data.analyzer = &analyzer;
    data.fft_bars = {};
    data.capturing = true; // hardcoding for now
    device.start();

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

    // Flush any remaining audio frames in the denoiser's queue
    data.process_frames(true);

  } catch (const std::runtime_error &error) {
    SDL_Log(error.what(), "\n");
    return -1;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

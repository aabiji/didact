#include <algorithm>
#include <iostream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>

#include "analyzer.h"
#include "audio.h"
#include "error.h"
#include "speech.h"

struct CallbackData {
  AudioStream* stream;
  SpectrumAnalyzer* analyzer;
  SpeechToText* stt;
  float_vec fft_bars;
  std::stop_token token;
};

void data_callback(ma_device* stream, void* output, const void* input, u32 size) {
  CallbackData* d = (CallbackData*)stream->pUserData;
  d->stream->queue_samples(input, output, size);

  while (d->stream->have_chunk(false) && !d->token.stop_requested()) {
    auto samples = d->stream->get_chunk();
    d->fft_bars = d->analyzer->process(samples.data(), (int)samples.size());
    d->stream->write_samples(samples.data(), (u32)samples.size());

    auto resampled = d->stream->resample(samples.data(), samples.size());
    d->stt->process(resampled.data(), resampled.size());
  }
}

void text_handler(std::string text) { std::cout << text << "\n"; }

void draw_bars(SDL_Renderer* renderer, std::vector<float> bars, float window_width,
               float window_height) {
  int num_bars = (int)bars.size();
  float min = *(std::min_element(bars.begin(), bars.end()));
  float max = *(std::max_element(bars.begin(), bars.end()));

  float bar_width = 5, bar_height = 100;
  float start_x = window_width / 2.0f;

  for (int i = -(num_bars - 1); i < num_bars; i++) {
    float value = bars[(size_t)abs(i)];
    float value_percent = (value - min) / (max - min);

    SDL_FRect rect;
    rect.w = bar_width;
    rect.h = value_percent * bar_height;
    rect.x = start_x + 2 * ((float)i * rect.w);
    rect.y = window_height / 2.0f - rect.h / 2.0f;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &rect);
  }
}

int main() {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;

  try {
    std::stop_source stopper;

    ModelPaths paths = {
        .tokens = "../assets/sherpa-onnx-streaming-zipformer-en-2023-06-26/tokens.txt",
        .encoder = "../assets/sherpa-onnx-streaming-zipformer-en-2023-06-26/"
                   "encoder-epoch-99-avg-1-chunk-16-left-128.onnx",
        .decoder = "../assets/sherpa-onnx-streaming-zipformer-en-2023-06-26/"
                   "decoder-epoch-99-avg-1-chunk-16-left-128.onnx",
        .joiner = "../assets/sherpa-onnx-streaming-zipformer-en-2023-06-26/"
                  "joiner-epoch-99-avg-1-chunk-16-left-128.onnx",
    };
    SpeechToText stt(paths, text_handler);

    AudioStream stream("test.wav", true);
    SpectrumAnalyzer analyzer((int)stream.sample_rate());
    CallbackData data = {&stream, &analyzer, &stt, {}, stopper.get_token()};

    stream.start(data_callback, &data);
    stream.enable_resampler(1, 16000);
    std::thread thread([&] { stt.run_inference(stopper.get_token()); });

    if (!SDL_Init(SDL_INIT_VIDEO))
      throw Error(SDL_GetError());

    int window_width = 900;
    int window_height = 700;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
    window = SDL_CreateWindow("didact", window_width, window_height, flags);
    if (!window)
      throw Error(SDL_GetError());
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

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
        // Peek decay between frames (rise sharply then fall slowly)
        float_vec bars(data.fft_bars.size());
        for (size_t i = 0; i < bars.size(); i++) {
          bars[i] = std::max(data.fft_bars[i], prev_bars[i] * 0.85f);
        }
        draw_bars(renderer, bars, (float)window_width, (float)window_height);
      }
      prev_bars = data.fft_bars;

      SDL_RenderPresent(renderer);
    }

    // Flush any remaining audio frames in the denoiser's queue
    while (stream.have_chunk(true)) {
      auto samples = stream.get_chunk();
      stream.write_samples(samples.data(), (u32)samples.size());
    }

    stopper.request_stop();
    thread.join();

  } catch (const std::runtime_error& error) {
    SDL_Log(error.what(), "\n");
    return -1;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

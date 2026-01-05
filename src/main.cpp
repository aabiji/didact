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
  std::vector<float> fft_bars;
  std::vector<std::string> recognized_text;
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

void text_handler(void* user_data, std::string text) {
  CallbackData* d = (CallbackData*)user_data;
  d->recognized_text.push_back(text);
}

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

    // TODO: download these automatically if they aren't present
    // (do the same for the Roboto font)
    ModelPaths paths = {
        "../assets/sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06/tokens.txt",
        "../assets/sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06/encoder.onnx",
        "../assets/sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06/decoder.onnx",
        "../assets/sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06/joiner.onnx",
    };
    SpeechToText stt(paths);

    AudioStream stream("test.wav", true);
    SpectrumAnalyzer analyzer((int)stream.sample_rate());
    CallbackData data = {&stream, &analyzer, &stt, {}, {}, stopper.get_token()};

    stream.start(data_callback, &data);
    stream.enable_resampler(1, 16000);
    std::thread thread(
        [&] { stt.run_inference(stopper.get_token(), text_handler, (void*)&data); });

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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.Fonts->AddFontFromFileTTF("../assets/Roboto-Regular.ttf");

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;
    style.FontSizeBase = 20.0f;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    SDL_Event event;
    bool running = true;
    float_vec prev_bars;

    while (running) {
      while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
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

      ImGui_ImplSDLRenderer3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();

      ImGuiWindowFlags window_flags = 0;
      window_flags |= ImGuiWindowFlags_NoBackground;
      window_flags |= ImGuiWindowFlags_NoTitleBar;
      window_flags |= ImGuiWindowFlags_NoMove;
      window_flags |= ImGuiWindowFlags_NoResize;
      window_flags |= ImGuiWindowFlags_NoCollapse;

      // Fix the window at the bottom
      ImGuiViewport* viewport = ImGui::GetMainViewport();
      float width = viewport->WorkSize.x, height = 175;
      ImVec2 pos = ImVec2(viewport->WorkPos.x,
                          viewport->WorkPos.y + viewport->WorkSize.y - height);
      ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

      bool window_open = true;
      ImGui::Begin("UI", &window_open, window_flags);

      int start = std::max(0, (int)(data.recognized_text.size() - 6));
      for (int i = start; i < data.recognized_text.size(); i++) {
        float text_width = ImGui::CalcTextSize(data.recognized_text[i].c_str()).x;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - text_width) * 0.5);
        ImGui::Text(data.recognized_text[i].c_str());
      }

      ImGui::End();
      ImGui::Render();

      SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x,
                         io.DisplayFramebufferScale.y);
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

      ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
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

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

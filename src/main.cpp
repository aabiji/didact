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

struct Transcript {
  std::string current_line;
  std::string text;

  void save_to_clipboard() {
    // TODO: why does copying this text take so long??
    std::string total_text = text + "\n" + current_line;
    if (!SDL_SetClipboardText(total_text.c_str()))
      throw Error(SDL_GetError());
  }
};

int text_input_callback(ImGuiInputTextCallbackData* data) {
  Transcript* t = (Transcript*)data->UserData;
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
    t->text.resize(data->BufTextLen);
    data->Buf = t->text.data();
  }

  // Sync ImGui's internal state with our own
  // TODO: improve
  bool out_of_sync = strcmp(data->Buf, t->text.data()) != 0;
  if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && out_of_sync &&
      !ImGui::IsAnyItemActive()) {
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, t->text.data(), t->text.data() + t->text.size());
    data->CursorPos = data->BufTextLen;
  }

  return 0;
}

void text_handler(void* user_data, std::string text, bool endpoint) {
  if (text.size() == 0)
    return;

  Transcript* t = (Transcript*)user_data;
  t->current_line = text;
  if (endpoint) {
    t->text += t->current_line + "\n";
    t->current_line = "";
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
    Transcript transcript;

    AudioStream stream("test.wav", true);
    SpectrumAnalyzer analyzer((int)stream.sample_rate());
    CallbackData data = {&stream, &analyzer, &stt, {}, stopper.get_token()};

    stream.start(data_callback, &data);
    stream.enable_resampler(1, 16000);
    std::thread thread([&] {
      stt.run_inference(stopper.get_token(), text_handler, (void*)&transcript);
    });

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

    SDL_FRect bar_rect = {.x = (float)window_width / 2.0f, .y = 50, .w = 5, .h = 100};
    int ui_offset_y = bar_rect.y + 25;

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

      // Position the ui window
      ImGuiViewport* viewport = ImGui::GetMainViewport();
      ImVec2 pos = ImVec2(0, ui_offset_y);
      ImVec2 size = ImVec2(window_width, window_height - ui_offset_y);
      ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
      ImGui::SetNextWindowSize(size, ImGuiCond_Always);

      bool window_open = true;
      ImGui::Begin("UI", &window_open, window_flags);

      if (ImGui::Button("Copy"))
        transcript.save_to_clipboard();

      ImVec2 widget_size = ImVec2(window_width, window_height - ui_offset_y - 100);
      ImGuiInputTextFlags input_flags =
          ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_NoUndoRedo |
          ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways |
          ImGuiInputTextFlags_CallbackResize;
      ImGui::InputTextMultiline("Transcript", transcript.text.data(),
                                (size_t)transcript.text.capacity(), widget_size,
                                input_flags, text_input_callback, (void*)&transcript);
      ImGui::Text("%s", transcript.current_line.c_str());

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
        draw_bars(renderer, bar_rect, bars);
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

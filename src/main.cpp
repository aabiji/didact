#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include <miniaudio.h>
#include <raylib.h>

#include "decoder.h"
#include "fft.h"

void process_audio_frame(void *user_data, int16_t *samples, int num_samples,
                         int total_samples) {
  SpectrumAnalyzer *analyzer = (SpectrumAnalyzer *)user_data;
  analyzer->process(samples, num_samples, total_samples);
}

int playback_sample_index = 0;

// TODO: get microphone input
// TODO: pull in whispercpp as a dependency (if real time tts is actually fast
// enough, then we could think about pulling in qt for a real time voice app, if
// not, start thinking about the next project
void data_callback(ma_device *device, void *output, const void *input,
                   ma_uint32 num_samples) {
  AudioDecoder *decoder = (AudioDecoder *)device->pUserData;
  if (decoder == nullptr || decoder->empty())
    return;

  auto samples = decoder->get_samples(num_samples);
  std::copy(samples.begin(), samples.begin() + num_samples, (int16_t *)output);
  playback_sample_index += num_samples;
  std::cout << num_samples << "\n";
}

void draw_bars(std::vector<float> bars, Vector2 window_size) {
  int num_bars = bars.size();
  float min = *(std::min_element(bars.begin(), bars.end()));
  float max = *(std::max_element(bars.begin(), bars.end()));

  Vector2 bar_size = {5, 200};
  float start_x = window_size.x / 2.0f;

  for (int i = -(num_bars - 1); i < num_bars; i++) {
    float value = bars[abs(i)];
    float value_percent = (value - min) / (max - min);
    float height = value_percent * bar_size.y;
    float x = start_x + 2 * (i * bar_size.x);
    float center_y = window_size.y / 2.0f - height / 2.0f;
    DrawRectangle(x, center_y, bar_size.x, height, WHITE);
  }
}

int main() {
  try {
    std::stop_source stopper;
    std::stop_token token = stopper.get_token();

    const char *path = "../assets/fly-me-to-the-moon.mp3";
    AudioDecoder decoder(path, 4096, token);
    SpectrumAnalyzer analyzer(decoder.sample_rate());

    SampleHandler handler = {.callback = process_audio_frame,
                             .user_data = (void *)&analyzer};
    std::thread t1([&] { decoder.process_file(handler); });

    ma_device device;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = 1;
    config.sampleRate = decoder.sample_rate();
    config.dataCallback = data_callback;
    config.pUserData = &decoder;

    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
      std::cout << "Failed to open the playback device\n";
      return -1;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
      ma_device_uninit(&device);
      return -1;
    }

    int buffer_size = device.playback.internalPeriodSizeInFrames;
    std::cout << "Buffer size: " << buffer_size << "\n";

    Vector2 window_size = {900, 700};
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(window_size.x, window_size.y, "didact");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
      ClearBackground(BLACK);
      BeginDrawing();

      auto bars = analyzer.get_bars(playback_sample_index);
      if (bars.size() > 0)
        draw_bars(bars, window_size);

      EndDrawing();
    }

    ma_device_uninit(&device);

    CloseWindow();
    stopper.request_stop();
    t1.join();
  } catch (const std::runtime_error &error) {
    std::cout << error.what() << "\n";
    return -1;
  }

  return 0;
}

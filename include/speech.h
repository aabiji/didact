#pragma once

#include <c-api.h> // sherpa-onnx
#include <renamenoise.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>

using TextHandler = std::function<void(void*, std::string, bool)>;

struct ModelPaths {
  const char* tokens;
  const char* encoder;
  const char* decoder;
  const char* joiner;
};

class SpeechToText {
public:
  ~SpeechToText();
  SpeechToText(ModelPaths paths);

  int expected_chunk_size();
  bool initialized();

  void process(float* samples, int num_samples);
  void run_inference(std::stop_token token, TextHandler handler, void* user_data);
  std::vector<float> denoise(float* samples, int num_samples);

private:
  void init();

  bool m_initialized;
  ModelPaths m_model_paths;
  std::mutex m_mutex;
  std::condition_variable_any m_have_enough_data;

  ReNameNoiseDenoiseState* m_denoiser;
  const SherpaOnnxOnlineRecognizer* m_recognizer;
  const SherpaOnnxOnlineStream* m_stream;
};

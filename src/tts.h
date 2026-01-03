#pragma once

#include <functional>
#include <stop_token>
#include <string>
#include <whisper.h>

#include "queue.h"

using HandleText = std::function<void(std::string)>;

class TTS {
public:
  ~TTS();
  TTS(std::string model_path, std::stop_token token);

  void run_inference(HandleText handler);
  void queue_samples(float* samples, int num_samples);

private:
  SampleQueue<float> m_queue;
  std::stop_token m_token;

  struct whisper_full_params m_fparams;
  struct whisper_context* m_ctx;
};

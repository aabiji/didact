#include <thread>

#include "error.h"
#include "tts.h"

// Whisper.cpp takes 30 seconds worth of samples at 16 kHz
const int CHUNK_SAMPLES = 16000 * 30;

TTS::~TTS() { whisper_free(m_ctx); }

TTS::TTS(std::string model_path, std::stop_token token) {
  ggml_backend_load_all();

  struct whisper_context_params params = whisper_context_default_params();
  // TODO: find a clean way to load ship the model with the app
  m_ctx = whisper_init_from_file_with_params(model_path.c_str(), params);
  if (m_ctx == nullptr)
    throw Error("Failed to initialize whisper's context");

  m_fparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  m_fparams.print_progress = false;
  m_fparams.n_threads = std::min(4, (int32_t)std::thread::hardware_concurrency());

  m_queue.init(CHUNK_SAMPLES * 10);
  m_token = token;
}

void TTS::queue_samples(float* samples, int num_samples) {
  m_queue.push_samples(samples, num_samples);
}

void TTS::run_inference(HandleText handler) {
  while (!m_token.stop_requested()) {
    auto samples = m_queue.pop_samples(CHUNK_SAMPLES, m_token);
    if (whisper_full(m_ctx, m_fparams, samples.data(), (int)samples.size()) != 0)
      throw Error("Failed to process audio");

    int segments = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < segments; i++) {
      std::string text = whisper_full_get_segment_text(m_ctx, i);
      handler(text);
    }
  }
}

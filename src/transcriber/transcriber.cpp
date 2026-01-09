#include "transcriber/transcriber.h"

Transcription::~Transcription() {
  if (m_inference_thread.joinable() && m_audio_thread.joinable())
    stop();
}

void Transcription::init(ModelPaths paths, const char* audio_path, bool capture) {
  m_stt.init(paths);
  m_stream.init(audio_path, capture);
  m_analyzer.init(m_stream.sample_rate());
}

void Transcription::start() {
  m_stream.start();
  m_stream.enable_resampler(1, 16000);
  m_audio_thread =
      std::jthread([this](std::stop_token token) { this->handle_audio(token); });

  auto speech_callback = [](void* user_data, std::string text, bool endpoint) {
    if (text.size() > 0) {
      Transcription* t = (Transcription*)user_data;
      t->handle_speech(text, endpoint);
    }
  };

  m_inference_thread = std::jthread([this, speech_callback](std::stop_token token) {
    m_stt.run_inference(token, speech_callback, this);
  });
}

void Transcription::stop() {
  m_inference_thread.request_stop();
  m_audio_thread.request_stop();

  // Flush any remaining audio frames in the denoiser's queue
  while (true) {
    auto samples = m_stream.get_chunk({});
    if (samples.size() == 0)
      break;
    m_stream.write_samples(samples.data(), samples.size());
  }
}

void Transcription::handle_audio(std::stop_token token) {
  while (!token.stop_requested()) {
    auto samples = m_stream.get_chunk(token);
    if (token.stop_requested())
      break;

    m_fft_bars = m_analyzer.process(samples.data(), samples.size());
    m_stream.write_samples(samples.data(), samples.size());

    auto resampled = m_stream.resample(samples.data(), samples.size());
    m_stt.process(resampled.data(), resampled.size());
  }
}

void Transcription::handle_speech(std::string text, bool endpoint) {
  m_current_line = text;
  if (endpoint) {
    m_lines.push_back(m_current_line);
    m_current_line = "";
  }
}

std::vector<float>& Transcription::get_visualization_data() { return m_fft_bars; }

std::string Transcription::get_text() {
  std::string text = "";
  for (const auto& line : m_lines)
    text += line + "\n";
  text += m_current_line;
  return text;
}

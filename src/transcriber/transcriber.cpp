#include "transcriber/transcriber.h"

Transcription::~Transcription() {
  if (m_thread.joinable())
    stop();
}

void Transcription::init(ModelPaths paths, const char* audio_path, bool capture) {
  m_stt.init(paths);
  m_stream.init(audio_path, capture);
  m_analyzer.init(m_stream.sample_rate());
}

void Transcription::start() {
  auto audio_callback = [](ma_device* stream, void* output, const void* input, u32 size) {
    Transcription* t = (Transcription*)stream->pUserData;
    t->handle_audio(output, input, size);
  };

  auto speech_callback = [](void* user_data, std::string text, bool endpoint) {
    if (text.size() > 0) {
      Transcription* t = (Transcription*)user_data;
      t->handle_speech(text, endpoint);
    }
  };

  std::stop_token token = m_stopper.get_token();
  m_stream.start(audio_callback, this);
  m_stream.enable_resampler(1, 16000);
  m_thread = std::thread([this, token, speech_callback] {
    m_stt.run_inference(token, speech_callback, this);
  });
}

void Transcription::stop() {
  m_stopper.request_stop();
  m_thread.join();

  // Flush any remaining audio frames in the denoiser's queue
  while (m_stream.have_chunk(true)) {
    auto samples = m_stream.get_chunk();
    m_stream.write_samples(samples.data(), (u32)samples.size());
  }
}

void Transcription::handle_audio(void* output, const void* input, u32 size) {
  m_stream.queue_samples(input, output, size);
  auto token = m_stopper.get_token();

  while (m_stream.have_chunk(false) && !token.stop_requested()) {
    auto samples = m_stream.get_chunk();
    m_fft_bars = m_analyzer.process(samples.data(), (int)samples.size());
    m_stream.write_samples(samples.data(), (u32)samples.size());

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

#include <algorithm>
#include <cmath>

#include "transcriber.h"

Transcriber::Transcriber(ModelPaths paths, const char* audio_path, bool capture)
    : m_stt(paths), m_stream(audio_path, capture) {}

Transcriber::~Transcriber() {
  m_inference_thread.request_stop();
  m_stt_thread.request_stop();
}

void Transcriber::start() {
  auto audio_callback = [](void* user_data, float* samples, u32 num_samples) {
    Transcriber* t = (Transcriber*)user_data;
    t->calculate_amplitude(samples, num_samples);
  };

  auto speech_callback = [](void* user_data, std::string text, bool endpoint) {
    if (text.size() > 0) {
      Transcriber* t = (Transcriber*)user_data;
      t->update_transcript(text, endpoint);
    }
  };

  m_stream.start(audio_callback, this);
  m_stream.enable_resampler(16000);

  m_stt_thread =
      std::jthread([this](std::stop_token token) { this->process_audio_stream(token); });
  m_inference_thread = std::jthread([this, speech_callback](std::stop_token token) {
    m_stt.run_inference(token, speech_callback, this);
  });
}

void Transcriber::calculate_amplitude(float* samples, int num_samples) {
  if (m_amp_buffer.size() == 0) {
    m_write_offset = 0;
    m_amp_buffer_size = 1024;
    m_max_amplitude = 0;
    m_amp_buffer.resize(m_amp_buffer_size);
  }

  // Use the Root Mean Square algorithm to get an amplitude from the samplse
  float square_sum = 0;
  for (int i = 0; i < num_samples; i++)
    square_sum += samples[i] * samples[i];
  float rms_value = std::sqrt(square_sum / (float)num_samples);

  if (rms_value > m_max_amplitude)
    m_max_amplitude = rms_value;
  float normalized = rms_value / m_max_amplitude;

  // Write to the circular buffer
  m_amp_buffer[m_write_offset] = normalized;
  m_write_offset = (m_write_offset + 1) % m_amp_buffer_size;
}

void Transcriber::process_audio_stream(std::stop_token token) {
  while (!token.stop_requested()) {
    if (!m_stt.initialized())
      continue; // Wait for the speech-to-text model to load

    auto samples = m_stream.get_samples(token, m_stt.expected_chunk_size());
    if (token.stop_requested())
      break;

    if (samples.size() > 0) {
      auto denoised = m_stt.denoise(samples.data(), samples.size());
      auto resampled = m_stream.resample(denoised.data(), denoised.size());
      m_stt.process(resampled.data(), resampled.size());
    }
  }
}

void Transcriber::update_transcript(std::string text, bool endpoint) {
  m_current_line = text;
  if (endpoint) {
    m_lines.push_back(m_current_line);
    m_current_line = "";
  }
}

std::vector<float> Transcriber::get_normalized_waveform() {
  // Copy the oldest data first
  std::vector<float> amplitudes(m_amp_buffer_size);
  std::copy(m_amp_buffer.begin() + m_write_offset,
            m_amp_buffer.begin() + m_amp_buffer_size, amplitudes.begin());
  std::copy(m_amp_buffer.begin(), m_amp_buffer.begin() + m_write_offset,
            amplitudes.begin() + (m_amp_buffer_size - m_write_offset));
  return amplitudes;
}

std::string Transcriber::get_transcript() {
  std::string text = "";
  for (const auto& line : m_lines)
    text += line + "\n";
  text += m_current_line;
  return text;
}

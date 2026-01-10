#include <algorithm>
#include <cmath>

#include "transcriber.h"

TranscriptEngine::TranscriptEngine(ModelPaths paths, const char* audio_path,
                                   bool capture) {
  m_stt.init(paths);
  m_stream.init(audio_path, capture);
}

void TranscriptEngine::start() {
  auto audio_callback = [](void* user_data, float* samples, u32 num_samples) {
    TranscriptEngine* t = (TranscriptEngine*)user_data;
    t->calculate_amplitude(samples, num_samples);
  };

  auto speech_callback = [](void* user_data, std::string text, bool endpoint) {
    if (text.size() > 0) {
      TranscriptEngine* t = (TranscriptEngine*)user_data;
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

void TranscriptEngine::stop() {
  m_inference_thread.request_stop();
  m_stt_thread.request_stop();
}

float linear_to_db(float rms) {
  if (rms < 0.0001f)
    return 0.0f; // Floor to avoid log(0)
  // Map RMS to a 0.0 -> 1.0 range based on a 60dB dynamic range
  float db = 20.0f * std::log10(rms);

  // Normalize -60dB (quiet) to 0dB (loud) into a 0.0 to 1.0 range
  float normalized = (db + 60.0f) / 60.0f;
  return std::clamp(normalized, 0.0f, 1.0f);
}

void TranscriptEngine::calculate_amplitude(float* samples, int num_samples) {
  if (m_amp_buffer.size() == 0) {
    m_write_offset = 0;
    m_amp_buffer_size = 1024;
    m_amp_buffer.resize(m_amp_buffer_size);
  }

  // Use the Root Mean Square algorithm to get an amplitude from the samplse
  float square_sum = 0;
  for (int i = 0; i < num_samples; i++)
    square_sum += samples[i] * samples[i];
  float rms_value = linear_to_db(std::sqrt(square_sum / (float)num_samples));

  // Write to the circular buffer
  m_amp_buffer[m_write_offset] = rms_value;
  m_write_offset = (m_write_offset + 1) % m_amp_buffer_size;
}

void TranscriptEngine::process_audio_stream(std::stop_token token) {
  while (!token.stop_requested()) {
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

void TranscriptEngine::update_transcript(std::string text, bool endpoint) {
  m_current_line = text;
  if (endpoint) {
    m_lines.push_back(m_current_line);
    m_current_line = "";
  }
}

std::tuple<std::vector<float>, float, float> TranscriptEngine::get_waveform() {
  // Copy the oldest data first
  std::vector<float> amplitudes(m_amp_buffer_size);
  std::copy(m_amp_buffer.begin() + m_write_offset,
            m_amp_buffer.begin() + m_amp_buffer_size, amplitudes.begin());
  std::copy(m_amp_buffer.begin(), m_amp_buffer.begin() + m_write_offset,
            amplitudes.begin() + (m_amp_buffer_size - m_write_offset));

  auto min = std::min_element(amplitudes.begin(), amplitudes.end());
  auto max = std::max_element(amplitudes.begin(), amplitudes.end());
  return {amplitudes, *min, *max};
}

std::string TranscriptEngine::get_transcript() {
  std::string text = "";
  for (const auto& line : m_lines)
    text += line + "\n";
  text += m_current_line;
  return text;
}

#pragma once

#include <thread>

#include "audio.h"
#include "speech.h"

class TranscriptEngine {
public:
  TranscriptEngine(ModelPaths paths, const char* audio_path, bool capture);
  void start();
  void stop();

  void update_transcript(std::string text, bool endpoint);
  void calculate_amplitude(float* samples, int num_samples);
  void process_audio_stream(std::stop_token token);

  std::string get_transcript();
  // Get the amplitudes, and the min & max amplitude
  // NOTE: returns amplitudes from oldest to newest
  std::tuple<std::vector<float>, float, float> get_waveform();

private:
  std::string m_current_line;
  std::vector<std::string> m_lines;

  // Circular buffer of amplitudes
  std::vector<float> m_amp_buffer;
  int m_write_offset;
  int m_amp_buffer_size;

  SpeechToText m_stt;
  std::jthread m_stt_thread;

  AudioStream m_stream;
  std::jthread m_inference_thread;
};

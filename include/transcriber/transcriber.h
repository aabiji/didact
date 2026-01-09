#pragma once

#include "transcriber/analyzer.h"
#include "transcriber/audio.h"
#include "transcriber/speech.h"

class Transcription {
public:
  ~Transcription();

  void init(ModelPaths paths, const char* audio_path, bool capture);
  void start();
  void stop();

  void handle_audio(std::stop_token token);
  void handle_speech(std::string text, bool endpoint);

  std::string get_text();
  std::vector<float>& get_visualization_data();

private:
  std::string m_current_line;
  std::vector<std::string> m_lines;
  std::vector<float> m_fft_bars;

  std::jthread m_audio_thread;
  std::jthread m_inference_thread;

  SpectrumAnalyzer m_analyzer;
  SpeechToText m_stt;
  AudioStream m_stream;
};

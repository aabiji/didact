#pragma once

#include "transcriber/analyzer.h"
#include "transcriber/audio.h"
#include "transcriber/speech.h"

class Transcription {
public:
  void init(ModelPaths paths, const char* audio_path, bool capture);
  void start();
  void stop();

  void handle_audio(void* output, const void* input, u32 size);
  void handle_speech(std::string text, bool endpoint);

  std::string get_text();
  std::vector<float> get_visualization_data();

private:
  std::string m_current_line;
  std::vector<std::string> m_lines;
  std::mutex m_txt_mutex;

  std::mutex m_fft_mutex;
  std::vector<float> m_fft_bars;

  std::thread m_thread;
  std::stop_source m_stopper;

  SpectrumAnalyzer m_analyzer;
  SpeechToText m_stt;
  AudioStream m_stream;
};

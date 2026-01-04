#pragma once

#include <c-api.h> // sherpa-onnx
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

using TextHandler = std::function<void(std::string)>;

struct ModelPaths {
  const char* tokens;
  const char* encoder;
  const char* decoder;
  const char* joiner;
};

class SpeechToText {
public:
  ~SpeechToText() {
    SherpaOnnxOnlineStreamInputFinished(m_stream);
    SherpaOnnxDestroyOnlineStream(m_stream);
    SherpaOnnxDestroyOnlineRecognizer(m_recognizer);
  }

  SpeechToText(ModelPaths paths, TextHandler handler) {
    SherpaOnnxOnlineRecognizerConfig config = {0};

    config.model_config.debug = 1;
    config.model_config.num_threads =
        std::min(2, (int)std::thread::hardware_concurrency());
    config.model_config.provider = "cpu";
    config.model_config.tokens = paths.tokens;
    config.model_config.transducer.encoder = paths.encoder;
    config.model_config.transducer.decoder = paths.decoder;
    config.model_config.transducer.joiner = paths.joiner;

    config.max_active_paths = 4;
    config.decoding_method = "modified_beam_search";
    config.feat_config.sample_rate = 16000;
    config.feat_config.feature_dim = 80;

    config.enable_endpoint = true;
    config.rule1_min_trailing_silence = 2.4;
    config.rule2_min_trailing_silence = 1.2;
    config.rule3_min_utterance_length = 300;

    m_recognizer = SherpaOnnxCreateOnlineRecognizer(&config);
    m_stream = SherpaOnnxCreateOnlineStream(m_recognizer);
    m_handler = handler;
    m_segment = "";
  }

  // NOTE: The samples must be normalized to a range of [-1, 1]
  void process(float* samples, int num_samples) {
    {
      std::lock_guard<std::mutex> guard(m_mutex);
      SherpaOnnxOnlineStreamAcceptWaveform(m_stream, 16000, samples, num_samples);
    }
    m_have_enough_data.notify_one();
  }

  void run_inference(std::stop_token token) {
    while (!token.stop_requested()) {
      // Wait until there's enough samples to run inference on
      std::unique_lock<std::mutex> guard(m_mutex);
      auto lambda = [&] { return SherpaOnnxIsOnlineStreamReady(m_recognizer, m_stream); };
      if (!m_have_enough_data.wait(guard, token, lambda))
        break;

      while (SherpaOnnxIsOnlineStreamReady(m_recognizer, m_stream))
        SherpaOnnxDecodeOnlineStream(m_recognizer, m_stream);

      const SherpaOnnxOnlineRecognizerResult* r =
          SherpaOnnxGetOnlineStreamResult(m_recognizer, m_stream);
      m_segment = std::string(r->text);

      if (SherpaOnnxOnlineStreamIsEndpoint(m_recognizer, m_stream)) {
        SherpaOnnxOnlineStreamReset(m_recognizer, m_stream);
        if (m_segment.size() > 0) {
          m_handler(m_segment);
          m_segment = "";
        }
      }

      SherpaOnnxDestroyOnlineRecognizerResult(r);
    }
  }

private:
  std::string m_segment;
  TextHandler m_handler;
  std::mutex m_mutex;
  std::condition_variable_any m_have_enough_data;

  const SherpaOnnxOnlineRecognizer* m_recognizer;
  const SherpaOnnxOnlineStream* m_stream;
};

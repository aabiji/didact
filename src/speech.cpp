#include "speech.h"

SpeechToText::SpeechToText(ModelPaths paths) {
  m_model_paths = paths;
  m_initialized = false;
}

SpeechToText::~SpeechToText() {
  if (m_initialized) {
    SherpaOnnxOnlineStreamInputFinished(m_stream);
    SherpaOnnxDestroyOnlineStream(m_stream);
    SherpaOnnxDestroyOnlineRecognizer(m_recognizer);
    renamenoise_destroy(m_denoiser);
  }
}

int SpeechToText::expected_chunk_size() { return renamenoise_get_frame_size(); }

bool SpeechToText::initialized() { return m_initialized; }

void SpeechToText::init() {
  SherpaOnnxOnlineRecognizerConfig config = {0};
  config.model_config.debug = 0;
  config.model_config.num_threads = std::min(2, (int)std::thread::hardware_concurrency());
  config.model_config.provider = "cpu";
  config.model_config.tokens = m_model_paths.tokens;
  config.model_config.transducer.encoder = m_model_paths.encoder;
  config.model_config.transducer.decoder = m_model_paths.decoder;
  config.model_config.transducer.joiner = m_model_paths.joiner;

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
  m_denoiser = renamenoise_create(nullptr);
  m_initialized = true;
}

std::vector<float> SpeechToText::denoise(float* samples, int num_samples) {
  std::vector<float> output(num_samples);
  renamenoise_process_frame(m_denoiser, output.data(), samples);
  return output;
}

// NOTE: The samples must be normalized to a range of [-1, 1]
void SpeechToText::process(float* samples, int num_samples) {
  std::lock_guard<std::mutex> guard(m_mutex);
  SherpaOnnxOnlineStreamAcceptWaveform(m_stream, 16000, samples, num_samples);
  m_have_enough_data.notify_one();
}

void SpeechToText::run_inference(std::stop_token token, TextHandler handler,
                                 void* user_data) {
  if (!m_initialized)
    init();

  while (!token.stop_requested()) {
    // Wait until there's enough samples to run inference on
    std::unique_lock<std::mutex> guard(m_mutex);
    auto lambda = [&] {
      return SherpaOnnxIsOnlineStreamReady(m_recognizer, m_stream) ||
             token.stop_requested();
    };
    if (!m_have_enough_data.wait(guard, token, lambda))
      break; // A stop was requested

    while (SherpaOnnxIsOnlineStreamReady(m_recognizer, m_stream)) {
      if (token.stop_requested())
        break;
      SherpaOnnxDecodeOnlineStream(m_recognizer, m_stream);
    }

    const SherpaOnnxOnlineRecognizerResult* r =
        SherpaOnnxGetOnlineStreamResult(m_recognizer, m_stream);

    bool endpoint = false;
    if (SherpaOnnxOnlineStreamIsEndpoint(m_recognizer, m_stream)) {
      SherpaOnnxOnlineStreamReset(m_recognizer, m_stream);
      endpoint = true;
    }

    handler(user_data, r->text, endpoint);
    SherpaOnnxDestroyOnlineRecognizerResult(r);
  }
}
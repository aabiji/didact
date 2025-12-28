#pragma once

#include <functional>
#include <stop_token>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include "queue.h"

struct SampleHandler {
  std::function<void(void *, int16_t *, int, int)> callback;
  void *user_data;
};

class AudioDecoder {
public:
  AudioDecoder(const char *file_path, int max_queue_size,
               std::stop_token stop_token);
  ~AudioDecoder();

  void process_file(SampleHandler handler);

  bool empty();
  int sample_rate();
  std::vector<int16_t> get_samples(int amount);

private:
  void init_codec_ctx(const char *audio_path);
  void init_resampler();

  void decode_packet(SampleHandler handle, AVPacket *packet);
  void resample_audio(AVFrame *frame, int *dst_num_samples);

  std::stop_token m_token;

  struct OutputConfig {
    AVSampleFormat sample_format;
    AVChannelLayout channel_layout;
    int frame_samples;
  } m_output;

  int m_audio_stream;
  int m_num_total_samples;
  int m_max_frame_samples;
  uint8_t **m_pcm_buffer;
  SampleQueue m_queue;

  SwrContext *m_resampler;
  AVCodecContext *m_codec_ctx;
  AVFormatContext *m_format_ctx;
};

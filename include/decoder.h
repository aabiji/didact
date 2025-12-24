#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include "queue.h"

class AudioDecoder {
 public:
  AudioDecoder(const char* file_path,
               SampleQueue* queue,
               std::stop_token stop_token);
  ~AudioDecoder();

  void process_file();
  int sample_rate();

 private:
  void init_codec_ctx(const char* audio_path);
  void init_resampler();

  void decode_packet(AVPacket* packet);
  void resample_audio(AVFrame* frame, int* dst_num_samples);

  std::stop_token m_token;
  SampleQueue* m_queue;

  struct OutputConfig {
    AVSampleFormat sample_format;
    AVChannelLayout channel_layout;
    int frame_samples;
  } m_output;

  int m_audio_stream;
  int m_max_num_samples;
  uint8_t** m_pcm_buffer;

  SwrContext* m_resampler;
  AVCodecContext* m_codec_ctx;
  AVFormatContext* m_format_ctx;
};
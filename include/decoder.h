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
  AudioDecoder(const char* file_path);
  ~AudioDecoder();

  void process_file(SampleQueue* queue);

 private:
  void init_codec_ctx(const char* audio_path);
  void init_resampler();

  void decode_packet(SampleQueue* queue, AVPacket* packet);
  void resample_audio(AVFrame* frame, uint8_t*** buffer, int* dst_num_samples);

  struct OutputConfig {
    AVSampleFormat sample_format;
    AVChannelLayout channel_layout;
    int frame_samples;
  } m_output;
  int m_audio_stream;
  SwrContext* m_resampler;
  AVCodecContext* m_codec_ctx;
  AVFormatContext* m_format_ctx;
};
#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
#include <raylib.h>

#include "queue.h"
#include "reader.h"

const AVSampleFormat SAMPLE_FORMAT = AV_SAMPLE_FMT_S16;
const AVChannelLayout CHANNEL_LAYOUT = AV_CHANNEL_LAYOUT_MONO;
// each audio chunk will have a fixed amount of samples
const int NUM_SAMPLES = 1024;

void panic(const char *message) {
  std::cout << message << "\n";
  exit(-1);
}

void decode_audio_packet(AVCodecContext *decoder_ctx, SwrContext *swr_ctx,
                         SampleQueue &queue, AVPacket *packet, AVFrame *frame) {
  int ret = avcodec_send_packet(decoder_ctx, packet);
  if (ret < 0)
    panic("Failed to submit the packet to the decoder");

  while (true) {
    AVFrame *frame = av_frame_alloc();

    ret = avcodec_receive_frame(decoder_ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      return;
    else if (ret < 0)
      panic("Failed to decode frame");

    int dst_linesize = 0;
    int dst_num_samples = swr_get_out_samples(swr_ctx, frame->nb_samples);

    uint8_t **buffer = nullptr;
    ret = av_samples_alloc_array_and_samples(&buffer, &dst_linesize,
                                             CHANNEL_LAYOUT.nb_channels,
                                             dst_num_samples, SAMPLE_FORMAT, 0);
    if (ret < 0)
      panic("Failed to allocate output buffer");

    ret = swr_convert(swr_ctx, buffer, dst_num_samples, frame->extended_data,
                      frame->nb_samples);
    if (ret < 0)
      panic("Failed to convert the samples");

    queue.push(buffer[0], dst_num_samples);
    av_freep(buffer);
  }
}

int main() {
  AVFormatContext *input_ctx = nullptr;
  const char *audio_path = "../assets/music.mp3";

  if (avformat_open_input(&input_ctx, audio_path, nullptr, nullptr) < 0)
    panic("Failed to read the input file");

  if (avformat_find_stream_info(input_ctx, nullptr) < 0)
    panic("Failed to find stream info");

  const AVCodec *codec = nullptr;
  int audio_stream =
      av_find_best_stream(input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
  if (audio_stream < 0)
    panic("Failed to find an audio stream");

  AVCodecContext *decoder_ctx = avcodec_alloc_context3(codec);
  AVStream *audio = input_ctx->streams[audio_stream];

  decoder_ctx->request_sample_fmt = SAMPLE_FORMAT;
  avcodec_parameters_to_context(decoder_ctx, audio->codecpar);
  if (avcodec_open2(decoder_ctx, codec, nullptr) < 0)
    panic("Failed to open codec for stream");

  SwrContext *swr_ctx = nullptr;
  int ret = swr_alloc_set_opts2(
      &swr_ctx, &CHANNEL_LAYOUT, SAMPLE_FORMAT, decoder_ctx->sample_rate,
      &decoder_ctx->ch_layout, decoder_ctx->sample_fmt,
      decoder_ctx->sample_rate, 0, nullptr);
  if (ret < 0)
    panic(av_err2str(ret));

  if ((ret = swr_init(swr_ctx)) < 0)
    panic("Failed to initialize SwrContext");

  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = nullptr;

  SampleQueue queue(NUM_SAMPLES * 3, NUM_SAMPLES, 2);

  while (ret >= 0) {
    if ((ret = av_read_frame(input_ctx, packet)) < 0)
      break;
    if (queue.is_full())
      break;
    if (packet->stream_index == audio_stream)
      decode_audio_packet(decoder_ctx, swr_ctx, queue, packet, frame);
    av_packet_unref(packet);
  }

  /*
  int width = 900, height = 700;
  InitWindow(width, height, "dj");
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    BeginDrawing();
    DrawRectangle(100, 200, 50, 50, GREEN);
    EndDrawing();
  }

  CloseWindow();
  */

  av_packet_free(&packet);
  av_frame_free(&frame);
  swr_free(&swr_ctx);
  avcodec_free_context(&decoder_ctx);
  avformat_close_input(&input_ctx);
}

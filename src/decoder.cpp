#include "decoder.h"
#include "error.h"

AudioDecoder::~AudioDecoder() {
  swr_free(&m_resampler);
  avcodec_free_context(&m_codec_ctx);
  avformat_close_input(&m_format_ctx);
}

AudioDecoder::AudioDecoder(const char* file_path) {
  m_audio_stream = -1;
  m_resampler = nullptr;
  m_codec_ctx = nullptr;
  m_format_ctx = nullptr;
  m_output = {.sample_format = AV_SAMPLE_FMT_S16,
              .channel_layout = AV_CHANNEL_LAYOUT_MONO,
              .frame_samples = 1024};
  init_codec_ctx(file_path);
  init_resampler();
}

void AudioDecoder::init_codec_ctx(const char* file_path) {
  int ret = avformat_open_input(&m_format_ctx, file_path, nullptr, nullptr);
  if (ret < 0)
    throw Error(av_err2str(ret));

  ret = avformat_find_stream_info(m_format_ctx, nullptr);
  if (ret < 0)
    throw Error(av_err2str(ret));

  const AVCodec* codec = nullptr;
  m_audio_stream =
      av_find_best_stream(m_format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
  if (m_audio_stream < 0)
    throw Error(av_err2str(m_audio_stream));

  m_codec_ctx = avcodec_alloc_context3(codec);
  m_codec_ctx->request_sample_fmt = m_output.sample_format;

  AVStream* audio = m_format_ctx->streams[m_audio_stream];
  avcodec_parameters_to_context(m_codec_ctx, audio->codecpar);

  ret = avcodec_open2(m_codec_ctx, codec, nullptr);
  if (ret < 0)
    throw Error(av_err2str(ret));
}

void AudioDecoder::init_resampler() {
  int ret = swr_alloc_set_opts2(
      &m_resampler, &m_output.channel_layout, m_output.sample_format,
      m_codec_ctx->sample_rate, &m_codec_ctx->ch_layout,
      m_codec_ctx->sample_fmt, m_codec_ctx->sample_rate, 0, nullptr);
  if (ret < 0)
    throw Error(av_err2str(ret));

  ret = swr_init(m_resampler);
  if (ret < 0)
    throw Error(av_err2str(ret));
}

void AudioDecoder::resample_audio(AVFrame* frame,
                                  uint8_t*** buffer,
                                  int* dst_num_samples) {
  int dst_linesize = 0;
  *dst_num_samples = swr_get_out_samples(m_resampler, frame->nb_samples);

  int ret = av_samples_alloc_array_and_samples(
      buffer, &dst_linesize, m_output.channel_layout.nb_channels,
      *dst_num_samples, m_output.sample_format, 0);
  if (ret < 0)
    throw Error(av_err2str(ret));

  ret = swr_convert(m_resampler, *buffer, *dst_num_samples,
                    frame->extended_data, frame->nb_samples);
  if (ret < 0)
    throw Error(av_err2str(ret));
}

void AudioDecoder::decode_packet(SampleQueue* queue, AVPacket* packet) {
  int ret = avcodec_send_packet(m_codec_ctx, packet);
  if (ret < 0)
    throw Error(av_err2str(ret));

  while (true) {
    AVFrame* frame = av_frame_alloc();
    ret = avcodec_receive_frame(m_codec_ctx, frame);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      return;
    else if (ret < 0)
      throw Error(av_err2str(ret));

    int num_samples = 0;
    uint8_t** buffer = nullptr;

    resample_audio(frame, &buffer, &num_samples);
    queue->push(buffer[0], num_samples);

    av_freep(buffer);
    av_frame_free(&frame);
  }
}

void AudioDecoder::process_file(SampleQueue* queue) {
  int ret = 0;
  AVPacket* packet = av_packet_alloc();

  while (ret >= 0) {
    if ((ret = av_read_frame(m_format_ctx, packet)) < 0)
      break;

    if (packet->stream_index == m_audio_stream)
      decode_packet(queue, packet);

    av_packet_unref(packet);
  }

  av_packet_free(&packet);
}
#include <cstdlib>

#include "decoder.h"
#include "error.h"

AudioDecoder::~AudioDecoder() {
  if (m_pcm_buffer) {
    av_freep(&m_pcm_buffer[0]);
  }
  av_freep(&m_pcm_buffer);

  swr_free(&m_resampler);
  avcodec_free_context(&m_codec_ctx);
  avformat_close_input(&m_format_ctx);
}

AudioDecoder::AudioDecoder(const char *file_path, int max_queue_size,
                           std::stop_token stop_token) {
  m_token = stop_token;

  m_output = {.sample_format = AV_SAMPLE_FMT_S16,
              .channel_layout = AV_CHANNEL_LAYOUT_MONO,
              .frame_samples = 1024};

  m_audio_stream = -1;
  m_max_num_samples = 0;
  m_pcm_buffer = nullptr;
  m_queue.init(max_queue_size);

  m_resampler = nullptr;
  m_codec_ctx = nullptr;
  m_format_ctx = nullptr;

  init_codec_ctx(file_path);
  init_resampler();
}

std::string err2str(int errnum) {
  static char buffer[AV_ERROR_MAX_STRING_SIZE] = {0};
  std::string str =
      av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, errnum);
  return str;
}

void AudioDecoder::init_codec_ctx(const char *file_path) {
  int ret = avformat_open_input(&m_format_ctx, file_path, nullptr, nullptr);
  if (ret < 0)
    throw Error(err2str(ret));

  ret = avformat_find_stream_info(m_format_ctx, nullptr);
  if (ret < 0)
    throw Error(err2str(ret));

  const AVCodec *codec = nullptr;
  m_audio_stream =
      av_find_best_stream(m_format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
  if (m_audio_stream < 0)
    throw Error(err2str(m_audio_stream));

  m_codec_ctx = avcodec_alloc_context3(codec);
  m_codec_ctx->request_sample_fmt = m_output.sample_format;

  AVStream *audio = m_format_ctx->streams[m_audio_stream];
  avcodec_parameters_to_context(m_codec_ctx, audio->codecpar);

  ret = avcodec_open2(m_codec_ctx, codec, nullptr);
  if (ret < 0)
    throw Error(err2str(ret));
}

void AudioDecoder::init_resampler() {
  int ret = swr_alloc_set_opts2(
      &m_resampler, &m_output.channel_layout, m_output.sample_format,
      m_codec_ctx->sample_rate, &m_codec_ctx->ch_layout,
      m_codec_ctx->sample_fmt, m_codec_ctx->sample_rate, 0, nullptr);
  if (ret < 0)
    throw Error(err2str(ret));

  ret = swr_init(m_resampler);
  if (ret < 0)
    throw Error(err2str(ret));
}

void AudioDecoder::resample_audio(AVFrame *frame, int *dst_num_samples) {
  *dst_num_samples = swr_get_out_samples(m_resampler, frame->nb_samples);

  // Resize the pcm buffer when needed
  if (*dst_num_samples > m_max_num_samples) {
    if (m_pcm_buffer) {
      av_freep(&m_pcm_buffer[0]);
    }
    av_freep(&m_pcm_buffer);

    int dst_linesize = 0;
    int ret = av_samples_alloc_array_and_samples(
        &m_pcm_buffer, &dst_linesize, m_output.channel_layout.nb_channels,
        *dst_num_samples, m_output.sample_format, 0);
    if (ret < 0)
      throw Error(err2str(ret));

    m_max_num_samples = *dst_num_samples;
  }

  int ret =
      swr_convert(m_resampler, m_pcm_buffer, *dst_num_samples,
                  (const uint8_t **)frame->extended_data, frame->nb_samples);
  if (ret < 0)
    throw Error(err2str(ret));
}

void AudioDecoder::decode_packet(SampleHandler handler, AVPacket *packet) {
  int ret = avcodec_send_packet(m_codec_ctx, packet);
  if (ret < 0)
    throw Error(err2str(ret));

  while (!m_token.stop_requested()) {
    AVFrame *frame = av_frame_alloc();
    ret = avcodec_receive_frame(m_codec_ctx, frame);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      return;
    else if (ret < 0)
      throw Error(err2str(ret));

    int num_samples = 0;
    resample_audio(frame, &num_samples);

    int16_t *ptr = (int16_t *)(m_pcm_buffer[0]);
    handler.callback(handler.user_data, ptr, num_samples);
    m_queue.push_samples(ptr, num_samples);

    av_frame_free(&frame);
  }
}

int AudioDecoder::sample_rate() { return m_codec_ctx->sample_rate; }

bool AudioDecoder::empty() { return m_queue.empty(); }

std::vector<int16_t> AudioDecoder::get_samples(int amount) {
  return m_queue.pop_samples(amount);
}

void AudioDecoder::process_file(SampleHandler handler) {
  int ret = 0;
  AVPacket *packet = av_packet_alloc();

  while (ret >= 0 && !m_token.stop_requested()) {
    if (m_queue.is_full())
      continue;

    if ((ret = av_read_frame(m_format_ctx, packet)) < 0)
      break;

    if (packet->stream_index == m_audio_stream)
      decode_packet(handler, packet);

    av_packet_unref(packet);
  }

  av_packet_free(&packet);
}

#include "queue.h"

SampleQueue::~SampleQueue() {
  while (!m_chunks.empty()) {
    SampleChunk chunk = m_chunks.front();
    m_chunks.pop_front();
    free(chunk.samples);
  }
}

void SampleQueue::init(int max_chunks, int chunk_samples) {
  m_chunk_samples = chunk_samples;
  m_max_chunks = max_chunks;
}

bool SampleQueue::is_full() {
  return m_chunks.size() == m_max_chunks;
}

bool SampleQueue::has_chunk() {
  return !m_chunks.empty();
}

bool SampleQueue::partial_chunk_remaining() {
  return m_chunks.size() == 1 && m_chunks.front().num_samples < m_chunk_samples;
}

SampleChunk SampleQueue::pop_chunk(bool allow_partial_chunk) {
  SampleChunk front = m_chunks.front();
  m_chunks.pop_front();
  return front;
}

void SampleQueue::push_samples(int16_t* samples, int num_samples) {
  // Fill a potential partially filled chunk
  int samples_read = 0;
  if (!m_chunks.empty()) {
    SampleChunk& last = m_chunks.back();
    if (last.num_samples < m_chunk_samples) {
      int remaining = std::min(m_chunk_samples - last.num_samples, num_samples);
      std::copy(samples, samples + remaining, last.samples + last.num_samples);
      samples_read = remaining;
      last.num_samples = m_chunk_samples;
    }
  }

  // Split the samples up into chunks and add those
  // chunks (which can be partially filled) to the linked list
  while (samples_read < num_samples) {
    int remaining = std::min(num_samples - samples_read, m_chunk_samples);

    SampleChunk chunk;
    chunk.num_samples = remaining;
    chunk.samples = (int16_t*)calloc(m_chunk_samples, sizeof(int16_t));
    std::copy(samples + samples_read, samples + samples_read + remaining,
              chunk.samples);

    m_chunks.push_back(chunk);
    samples_read += remaining;
  }
}

#include "queue.h"
#include <cmath>

SampleQueue::SampleQueue(int max_samples, int samples_per_chunk,
                         int bytes_per_sample) {
  m_bytes_per_sample = bytes_per_sample;
  m_samples_per_chunk = samples_per_chunk;
  m_max_chunks = std::ceil(max_samples / samples_per_chunk);
}

bool SampleQueue::is_full() { return m_chunks.size() == m_max_chunks; }

bool SampleQueue::partial_chunk_remaining() {
  return m_chunks.size() == 1 &&
         m_chunks.front().num_samples < m_samples_per_chunk;
}

void SampleQueue::push(uint8_t *samples, int num_samples) {
  if (is_full())
    return;
  int samples_read = 0;

  // Fill a partially filled chunk if one exists
  if (!m_chunks.empty()) {
    SampleChunk front = m_chunks.front();
    if (front.num_samples < m_samples_per_chunk) {
      int remaining =
          std::min(m_samples_per_chunk - front.num_samples, num_samples);
      int dst_start = front.num_samples * m_bytes_per_sample;
      int src_end = remaining * m_bytes_per_sample;
      std::copy(samples, samples + src_end, front.data + dst_start);
      samples_read = remaining;
    }
  }

  // Split the samples up into chunks and add those
  // chunks (which can be partially filled) to the linked list
  while (samples_read < num_samples) {
    int remaining = std::min(num_samples - samples_read, m_samples_per_chunk);
    int src_start = samples_read * m_bytes_per_sample;
    int src_end = (samples_read + remaining) * m_bytes_per_sample;

    SampleChunk chunk;
    chunk.num_samples = remaining;
    chunk.data = (uint8_t *)calloc(m_samples_per_chunk, m_bytes_per_sample);
    std::copy(samples + src_start, samples + src_end, chunk.data);

    m_chunks.push_back(chunk);
    samples_read += remaining;
  }
}

std::optional<SampleChunk> SampleQueue::pop() {
  if (m_chunks.empty())
    return std::nullopt;

  SampleChunk front = m_chunks.front();
  if (front.num_samples < m_samples_per_chunk)
    return std::nullopt; // don't have a full chunk

  m_chunks.pop_front();
  return front;
}

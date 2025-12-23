#include "queue.h"
#include <cmath>

SampleQueue::SampleQueue(
  int max_samples, int chunk_samples, int sample_bytes) {
  m_sample_bytes = sample_bytes;
  m_chunk_samples = chunk_samples;
  m_max_chunks = std::ceil(max_samples / chunk_samples);
}

bool SampleQueue::is_full() {
  ReadLock read_lock(m_lock);
  return m_chunks.size() == m_max_chunks;
}

bool SampleQueue::partial_chunk_remaining() {
  ReadLock read_lock(m_lock);
  return m_chunks.size() == 1 &&
         m_chunks.front().num_samples < m_chunk_samples;
}

bool SampleQueue::can_read(bool allow_partial) {
  bool has_front = !m_chunks.empty();
  bool partial =
    has_front && m_chunks.front().num_samples < m_chunk_samples;
  return allow_partial ? has_front : !partial;
}

bool SampleQueue::can_write(int samples_to_write) {
  if (!m_chunks.empty()) {
    int remaining = m_chunk_samples - m_chunks.front().num_samples;
    samples_to_write = std::max(0, samples_to_write - remaining);
  }

  int chunks_needed = std::ceil(samples_to_write / m_chunk_samples);
  return m_chunks.size() < (m_max_chunks - chunks_needed);
}

SampleChunk SampleQueue::pop(bool allow_partial_chunk) {
  ReadLock read_lock(m_lock);
  m_cond_not_empty.wait(read_lock, [&]{
    return can_read(allow_partial_chunk);
  });

  SampleChunk front = m_chunks.front();
  m_chunks.pop_front();

  read_lock.unlock();
  m_cond_not_full.notify_all();
  return front;
}

void SampleQueue::push(uint8_t *samples, int num_samples) {
  WriteLock write_lock(m_lock);
  m_cond_not_full.wait(write_lock, [&]{ return can_write(num_samples); });

  // Fill a potential partially filled chunk
  int samples_read = 0;
  if (!m_chunks.empty()) {
    SampleChunk front = m_chunks.front();
    if (front.num_samples < m_chunk_samples) {
      int remaining =
          std::min(m_chunk_samples - front.num_samples, num_samples);
      int dst_start = front.num_samples * m_sample_bytes;
      int src_end = remaining * m_sample_bytes;
      std::copy(samples, samples + src_end, front.data + dst_start);
      samples_read = remaining;
    }
  }

  // Split the samples up into chunks and add those
  // chunks (which can be partially filled) to the linked list
  while (samples_read < num_samples) {
    int remaining = std::min(num_samples - samples_read, m_chunk_samples);
    int src_start = samples_read * m_sample_bytes;
    int src_end = (samples_read + remaining) * m_sample_bytes;

    SampleChunk chunk;
    chunk.num_samples = remaining;
    chunk.data = (uint8_t *)calloc(m_chunk_samples, m_sample_bytes);
    std::copy(samples + src_start, samples + src_end, chunk.data);

    m_chunks.push_back(chunk);
    samples_read += remaining;
  }

  write_lock.unlock();
  m_cond_not_empty.notify_all();
}

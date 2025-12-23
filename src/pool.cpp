#include "pool.h"

#include <iostream>

bool ChunkQueue::is_full() { return m_chunks.size() == m_max_chunks; }

void ChunkQueue::push(uint8_t *samples, int num_samples) {
  if (is_full())
    return;
  int samples_read = 0;
  std::cout << "Num samples: " << num_samples << "\n";

  // Fill a potentially partially filled chunk
  if (!m_chunks.empty()) {
    AudioChunk front = m_chunks.front();
    int front_samples = front->size() / m_bytes_per_sample;
    if (front_samples < m_samples_per_chunk) {
      int remaining =
          std::min(m_samples_per_chunk - front_samples, num_samples);
      int end = remaining * m_bytes_per_sample;
      std::cout << "Front samples: " << front_samples << "\n";
      std::cout << "Remaining: " << remaining << "\n";
      front->insert(front->end(), samples, samples + end);
      samples_read = remaining;
    }
  }

  std::cout << "Initial samples read: " << samples_read << "\n\n";

  // Split the samples up into chunks and add those
  // chunks (which can be partially filled) to the linked list
  while (samples_read < num_samples) {
    int remaining = std::min(num_samples - samples_read, m_samples_per_chunk);
    int start = samples_read * m_bytes_per_sample;
    int end = (samples_read + remaining) * m_bytes_per_sample;
    std::cout << "Remaining: " << remaining << " Start: " << start
              << " End: " << end << "\n";

    AudioChunk chunk = new std::vector<uint8_t>();
    chunk->reserve(m_samples_per_chunk * m_bytes_per_sample);
    chunk->insert(chunk->end(), samples + start, samples + end);
    m_chunks.push_back(chunk);
    samples_read += remaining;
  }

  std::cout << "Samples read: " << samples_read << "\n\n";
}

std::optional<AudioChunk> ChunkQueue::pop() {
  if (m_chunks.empty())
    return std::nullopt;

  AudioChunk front = m_chunks.front();
  int front_samples = front->size() / m_bytes_per_sample;
  if (front_samples < m_samples_per_chunk)
    return std::nullopt; // don't have a full chunk

  m_chunks.pop_front();
  return front; // TODO: we have to deallocate at some point
}

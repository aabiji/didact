#pragma once

#include <list>
#include <optional>
#include <stdint.h>
#include <vector>

// A fixed number of pcm samples
using AudioChunk = std::vector<uint8_t> *;

class ChunkQueue {
public:
  ChunkQueue(int size, int chunk_samples, int sample_size)
      : m_max_chunks(size), m_bytes_per_sample(sample_size),
        m_samples_per_chunk(chunk_samples) {}
  bool is_full();
  std::optional<AudioChunk> pop();
  void push(uint8_t *samples, int size);

private:
  int m_max_chunks;
  int m_bytes_per_sample;
  int m_samples_per_chunk;
  std::list<AudioChunk> m_chunks;
};

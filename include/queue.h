#pragma once

#include <list>
#include <optional>
#include <stdint.h>

struct SampleChunk {
  uint8_t *data;
  int num_samples;
};

class SampleQueue {
public:
  SampleQueue(int max_samples, int samples_per_chunk, int bytes_per_sample);

  bool is_full();

  // Returns whether the remaining samples in the queue
  // don't make up a full chunk
  bool partial_chunk_remaining();

  // Push a variable number of samples to the queue
  void push(uint8_t *samples, int num_samples);

  // Read back samples from the queue in a fixed sized chunk.
  // NOTE: The caller will be responsible for freeing the data
  // refered to by the chunk.
  std::optional<SampleChunk> pop();

private:
  int m_max_chunks;
  int m_bytes_per_sample;
  int m_samples_per_chunk;
  std::list<SampleChunk> m_chunks;
};
